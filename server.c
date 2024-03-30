#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define BUFSZ 500 //buff size

void usage(int argc, char **argv) {
    // recebe o tipo de protocolo do servidor e o porto onde ficara esperando
    printf("usage: %s <v4/v6> <server port>\n", argv[0]);
    printf("example: %s v4 51551\n", argv[0]);
    exit(EXIT_FAILURE);
}

typedef struct {
    int id;
    char *dado;
} sensor;

typedef struct {
    int id;
    int estado;
} ventilador;

typedef struct {
    int id;
    sensor *sensores;
    ventilador *ventiladores;
} sala;

// inicializa as salas de forma que, inicialmente, todas sao identificadas como nao existentes (a partir do id = -1)
sala* init_salas() { 
    sala *salas = malloc(8 * sizeof(sala));

    if(salas == NULL) {
        printf("Erro ao alocar memória.\n");
        logexit("Erro ao alocar memória.\n");
    }

    for(int i = 0; i < 8; i++) {
        sala s;
        s.id = -1;
        s.sensores = malloc(2 * sizeof(sensor));
        s.ventiladores = malloc(4 * sizeof(ventilador));

        for(int j = 0; j < 2; j++) {
            s.sensores[j].id = j;
            s.sensores[j].dado = malloc(21);
            strcpy(s.sensores[j].dado, "-99"); // sensor ainda nao instalado
        }
        for(int j = 0; j < 4; j++) {
            s.ventiladores[j].id = j;
            s.ventiladores[j].estado = 0;
        }

        salas[i] = s;
    }

    return salas;
}

int main(int argc, char **argv) {
    if(argc < 3) usage(argc, argv);

    //o connect recebe um ponteiro para a struct sockaddr
    struct sockaddr_storage storage;
    if(server_sockaddr_init(argv[1], argv[2], &storage) != 0) usage(argc, argv); // argv[1] -> endereco do servidor | argv[2] -> porto que recebeu | ponteiro para o storage 

    int _socket;
    _socket = socket(storage.ss_family, SOCK_STREAM, 0); //storage.ss_family -> socket (ipv4 | ipv6) | SOCK_STREAM -> SOCKET TCP
    if(_socket == -1) logexit("socket");

    // reutilizacao de porto mesmo se ja estiver sendo utilizado
    int enable = 1;
    if(setsockopt(_socket, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) != 0) logexit("setsockopt"); 

    //o servidor nao da connect, mas sim, bind() (depois o listen e depois o accept)

    //bind
    struct sockaddr *addr = (struct sockaddr *)(&storage);
    if(bind(_socket, addr, sizeof(storage)) != 0) logexit("bind");

    //listen
    if(listen(_socket, 10) != 0) logexit("listen"); // 10 -> quantidade de conexoes que podem estar pendentes para tratamento (pode ser outro valor)

    sala *salas = init_salas();
    
    char addrstr[BUFSZ];
    addrtostr(addr, addrstr, BUFSZ);
    printf("bound to %s, waiting connections\n", addrstr);

    // tratamento das conexoes pelo while
    while(1) {
        struct sockaddr_storage cstorage; //client storage
        struct sockaddr *caddr = (struct sockaddr *)(&cstorage);
        socklen_t caddrlen = sizeof(cstorage); 

        int csock = accept(_socket, caddr, &caddrlen); // retorna um novo socket (client socket)
                                                       // dá accept no _socket e cria um outro socket para falar com o cliente (socket que recebe conexao e que conversa com o cliente)  
                                        
        if(csock == -1) logexit("accept");

        char caddrstr[BUFSZ];
        addrtostr(caddr, caddrstr, BUFSZ);
        printf("[log] conenction from %s\n", caddrstr); // printa sempre que o cliente conecta

        //leitura da msg do cliente
        char buf[BUFSZ]; // armazena a mensagem do cliente
        memset(buf, 0, BUFSZ); 
        size_t count = recv(csock, buf, BUFSZ-1, 0); // retorna numero de bytes recebido
                                                     // csock -> socket de onde vai receber (socket do cliente) | buf -> onde se coloca o dado do cliente
        // nesse caso, o que chegar no primeiro recv sera a msg do cliente a ser considerada (msm se incompleta)

        printf("[msg] %s, %d bytes: %s\n", caddrstr, (int)count, buf); // printa a msg do cliente
        
        char mss[BUFSZ];
        memset(mss, 0, BUFSZ);

        if(strncmp(buf, "CAD_REQ", 7) == 0) { // 1a funcionalidade
            char *sala_id_s = strchr(buf, ' ');
            int sala_id = sala_id_s[1] - '0'; // converte de char para int
            //printf("sala: %d\n", sala_id);
            if(salas[sala_id].id != -1) memcpy(mss, "ERROR 02", 8);
            else {
                salas[sala_id].id = sala_id;
                memcpy(mss, "OK 01", 5);
            }
        } else if(strncmp(buf, "DES_REQ", 7) == 0) { // 3a funcionalidade
            char *sala_id_s = strchr(buf, ' ');
            int sala_id = sala_id_s[1] - '0'; // converte de char para int
            if(salas[sala_id].id == -1) memcpy(mss, "ERROR 03", 8);
            else {
               if(strncmp(salas[sala_id].sensores[0].dado, "-99", 3) == 0) memcpy(mss, "ERROR 06", 8);
               else {
                for(int i = 0; i < 2; i++) strcpy(salas[sala_id].sensores[i].dado, "-1");
                memcpy(mss, "OK 03", 5);
               }
            }
        } 
        else printf("other action\n");
                                                                       
        sprintf(buf, "mensagem do servidor: %.100s\n", mss); // limita print para 1000 bytes
        count = send(csock, buf, strlen(buf)+1, 0); // manda a resposta para o cliente | count -> nmr de bytes
        if(count != strlen(buf)+1) logexit("send");
        close(csock);
    }

    for (int i = 0; i < 8; i++) {
        free(salas[i].sensores);
        free(salas[i].ventiladores);
    }
    free(salas);
}