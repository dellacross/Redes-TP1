#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define BUFSZ 500 //buff size

#define OK01 "Sala Instanciada com Sucesso\n"
#define OK02 "Sensores Inicializados com Sucesso\n"
#define OK03 "Sensores Desligados com Sucesso\n"
#define OK04 "Informações Atualizadas com Sucesso\n"

void usage(int argc, char **argv) {
    // recebe o tipo de protocolo do servidor e o porto onde ficara esperando
    printf("usage: %s <v4/v6> <server port>\n", argv[0]);
    printf("example: %s v4 51551\n", argv[0]);
    exit(EXIT_FAILURE);
}

typedef struct {
    int id;
    char *dados_sensores;
    char *ventiladores;
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
        s.dados_sensores = malloc(21);
        strcpy(s.dados_sensores, "-99"); // sensores ainda nao instalados 

        char *default_estados = "10 20 30 40"; // ventiladores desligados (padrao)
        s.ventiladores = malloc(sizeof(default_estados));
        strcpy(s.ventiladores, default_estados);

        salas[i] = s;
    }

    return salas;
}

char* concatenar_salas(sala *salas) {
    char* resultado = malloc(1);  // Começa com uma string vazia
    resultado[0] = '\0';  // Termina a string com o caractere nulo

    for (int i = 0; i < 7; i++) {
        if(salas[i].id != -1) {        
            char temp[100];  // Buffer temporário para construir a string de cada sala
            sprintf(temp, "%d (%s %s) ", salas[i].id, salas[i].dados_sensores, salas[i].ventiladores);
            resultado = realloc(resultado, strlen(resultado) + strlen(temp) + 1);  // Realoca a memória para a nova string
            strcat(resultado, temp);  // Concatena a string temporária ao resultado
        }
    }

    return resultado;
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

        if(count != 0) printf("[msg] %s, %d bytes: %s\n", caddrstr, (int)count, buf); // printa a msg do cliente
        
        char mss[BUFSZ];
        memset(mss, 0, BUFSZ);

        if(strncmp(buf, "CAD_REQ", 7) == 0) { // 1a funcionalidade
            char *sala_id_s = strchr(buf, ' ');
            int sala_id = sala_id_s[1] - '0'; // converte de char para int

            if(salas[sala_id-1].id != -1) memcpy(mss, "ERROR02", 7);
            else {
                salas[sala_id-1].id = sala_id;
                memcpy(mss, "OK01", strlen("OK01")+1);
            }
        } else if(strncmp(buf, "INI_REQ", 7) == 0) { // 2a funcionalidade
            char *sala_id_s = strchr(buf, ' ');
            int sala_id = sala_id_s[1] - '0'; // converte de char para int
            char *valores_sensores = malloc(strlen(buf) * sizeof(char));
            char *estados_ventiladores = malloc(strlen(buf) * sizeof(char));
            int data1, data2, data3, data4, data5, data6, data7;

            if (sscanf(buf, "%*s %*s %d %d %d %d %d %d %d", &data1, &data2, &data3, &data4, &data5, &data6, &data7) >= 4) {
                sprintf(valores_sensores, "%d %d", data1, data2);
                sprintf(estados_ventiladores, "%d %d %d %d", data3, data4, data5, data6);
            }

            if(salas[sala_id-1].id == -1) memcpy(mss, "ERROR03", 7);
            else {
                if(strncmp(salas[sala_id-1].dados_sensores, "-99", 3) != 0) memcpy(mss, "ERROR05", 7);
                else {
                    strcpy(salas[sala_id-1].dados_sensores, valores_sensores);
                    strcpy(salas[sala_id-1].ventiladores, estados_ventiladores); 

                    memcpy(mss, "OK02", strlen("OK02")+1);
                }
            }
        } else if(strncmp(buf, "DES_REQ", 7) == 0) { // 3a funcionalidade
            char *sala_id_s = strchr(buf, ' ');
            int sala_id = sala_id_s[1] - '0'; // converte de char para int
            if(salas[sala_id-1].id == -1) memcpy(mss, "ERROR03", 7);
            else {
                if(strncmp(salas[sala_id-1].dados_sensores, "-99", 3) == 0) memcpy(mss, "ERROR 06", 8);
                else {
                    for(int i = 0; i < 2; i++) strcpy(salas[sala_id-1].dados_sensores, "-1");
                    memcpy(mss, "OK03", strlen("OK03")+1);
                }
            }
        } else if(strncmp(buf, "ALT_REQ", 7) == 0) { // 4a funcionalidade
            char *sala_id_s = strchr(buf, ' ');
            int sala_id = sala_id_s[1] - '0'; // converte de char para int
            char *valores_sensores = malloc(strlen(buf) * sizeof(char));
            char *estados_ventiladores = malloc(strlen(buf) * sizeof(char));
            int data1, data2, data3, data4, data5, data6, data7;

            if (sscanf(buf, "%*s %*s %d %d %d %d %d %d %d", &data1, &data2, &data3, &data4, &data5, &data6, &data7) >= 4) {
                sprintf(valores_sensores, "%d %d", data1, data2);
                sprintf(estados_ventiladores, "%d %d %d %d", data3, data4, data5, data6);
            }

            if(salas[sala_id-1].id == -1) memcpy(mss, "ERROR03", 7);
            else {
                if(strncmp(salas[sala_id-1].dados_sensores, "-99", 3) == 0) memcpy(mss, "ERROR06", 7);
                else {
                    strcpy(salas[sala_id-1].dados_sensores, valores_sensores);
                    strcpy(salas[sala_id-1].ventiladores, estados_ventiladores); 

                    memcpy(mss, "OK04", strlen("OK04")+1);
                }
            }
        } else if(strncmp(buf, "SAL_REQ", 7) == 0) { // 5a funcionalidade
            char *sala_id_s = strchr(buf, ' ');
            int sala_id = sala_id_s[1] - '0'; // converte de char para int
            if(salas[sala_id-1].id == -1) memcpy(mss, "ERROR03", 7);
            else {
                if(strncmp(salas[sala_id-1].dados_sensores, "-99", 3) == 0) memcpy(mss, "ERROR06", 7);
                else {
                    char* resp = malloc(8 + strlen(salas[sala_id-1].dados_sensores) + strlen(salas[sala_id-1].ventiladores) + 1);
                    sprintf(resp, "SAL_RES %d %s %s", sala_id, salas[sala_id-1].dados_sensores, salas[sala_id-1].ventiladores);

                    memcpy(mss, resp, strlen(resp)+1);
                    free(resp);
                }
            }
        } else if(strncmp(buf, "VAL_REQ", 7) == 0) { // 6a funcionalidade
            int aux = 0;
            for(int i = 0; i < 7; i ++) if(salas[i].id != -1) aux = 1; // verifica se ha salas cadastradas

            if(aux == 0) memcpy(mss, "ERROR03", 7);
            else {
                char *dados_salas = concatenar_salas(salas);
                char *resp = malloc(strlen("CAD_RES ") + strlen(dados_salas));
                sprintf(resp, "CAD_RES %s", dados_salas);
                memcpy(mss, resp, strlen(resp)+1);
                free(resp);
            }
        } 
        
        if(strncmp(buf, "kill", 4) == 0) close(_socket);
        else {
            strcpy(buf, mss); 
            count = send(csock, buf, strlen(buf)+1, 0); // manda a resposta para o cliente | count -> nmr de bytes
            if(count != strlen(buf)+1) logexit("send");
        }
        close(csock);                                       
    }

    for (int i = 0; i < 8; i++) free(salas[i].ventiladores);
    free(salas);
}