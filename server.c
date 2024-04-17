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

#define MIN_ID_SALA 0 //menor id valido para uma sala
#define MAX_ID_SALA 7 //maior id valido para uma sala

void usage(int argc, char **argv) {
    // recebe o tipo de protocolo do servidor e o porto onde ficara esperando
    printf("usage: %s <v4/v6> <server port>\n", argv[0]);
    exit(EXIT_FAILURE);
}

typedef struct {
    int id;
    char *dados_sensores;
    char *ventiladores;
    int inicializado;
} sala;

// inicializa as salas de forma que, inicialmente, todas sao identificadas como nao existentes (a partir do id = -1)
sala* init_salas() { 
    sala *salas = malloc(8 * sizeof(sala));

    if(salas == NULL) {
        printf("Erro ao alocar memória.\n");
        exit(EXIT_FAILURE);
    }

    for(int i = 0; i < 8; i++) {
        sala s;
        s.id = -1;
        char *default_sensores = "-1 -1";
        s.dados_sensores = malloc(sizeof(default_sensores));
        strcpy(s.dados_sensores, default_sensores); // sensores ainda nao instalados 

        char *default_estados = "-1 -1 -1 -1"; // ventiladores desligados (padrao)
        s.ventiladores = malloc(sizeof(default_estados));
        strcpy(s.ventiladores, default_estados);

        s.inicializado = 0; // sensores nao inicializados

        salas[i] = s;
    }

    return salas;
}

char* concatenar_salas(sala *salas) {
    char* resultado = malloc(1);  // Começa com uma string vazia
    resultado[0] = '\0';  // Termina a string com o caractere nulo

    for (int i = 0; i < 8; i++) {
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
    if(_socket == -1) exit(EXIT_FAILURE);

    // reutilizacao de porto mesmo se ja estiver sendo utilizado
    int enable = 1;
    if(setsockopt(_socket, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) != 0) exit(EXIT_FAILURE); 

    //o servidor nao da connect, mas sim, bind() (depois o listen e depois o accept)

    //bind
    struct sockaddr *addr = (struct sockaddr *)(&storage);
    if(bind(_socket, addr, sizeof(storage)) != 0) exit(EXIT_FAILURE);

    //listen
    if(listen(_socket, 10) != 0) exit(EXIT_FAILURE); // 10 -> quantidade de conexoes que podem estar pendentes para tratamento (pode ser outro valor)

    sala *salas = init_salas(); // inicializa um array de structs sala
    
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
                                        
        if(csock == -1) exit(EXIT_FAILURE);

        char caddrstr[BUFSZ];
        addrtostr(caddr, caddrstr, BUFSZ);
        printf("[log] connection from %s\n", caddrstr); // printa sempre que o cliente conecta

        //leitura das msgs do cliente
        
        char buf[BUFSZ]; // armazena a mensagem do cliente
        char mss[BUFSZ]; 
        while(1) {
            memset(buf, 0, BUFSZ); 
            memset(mss, 0, BUFSZ);

            size_t count = recv(csock, buf, BUFSZ-1, 0); // retorna numero de bytes recebido
                                                         // csock -> socket de onde vai receber (socket do cliente) | buf -> onde se coloca o dado do cliente
            // nesse caso, o que chegar no primeiro recv sera a msg do cliente a ser considerada (msm se incompleta)


            if(count != 0) printf("[msg] %s, %d bytes: %s\n", caddrstr, (int)count, buf); // printa a msg do cliente

            if(strncmp(buf, "kill", 4) == 0) {
                close(_socket);
                close(csock); 
                break;
            }
            else if(strncmp(buf, "CAD_REQ", strlen("CAD_REQ")) == 0) { // 1a funcionalidade
                char *sala_id_s = strchr(buf, ' ');
                int sala_id = atoi(sala_id_s);

                if(salas[sala_id].id != -1) memcpy(mss, "ERROR02", strlen("ERROR02"));
                else {
                    salas[sala_id].id = sala_id;
                    salas[sala_id].dados_sensores = "-1 -1";
                    salas[sala_id].ventiladores = "-1 -1 -1 -1";
                    salas[sala_id].inicializado = 0;
                    memcpy(mss, "OK01", strlen("OK01"));
                }
            } else if(strncmp(buf, "INI_REQ", strlen("INI_REQ")) == 0) { // 2a funcionalidade
                char *sala_id_s = strchr(buf, ' ');
                int sala_id = atoi(sala_id_s);
                char *valores_sensores = malloc(strlen(buf) * sizeof(char));
                char *estados_ventiladores = malloc(strlen(buf) * sizeof(char));
                int data1, data2, data3, data4, data5, data6, data7;

                if (sscanf(buf, "%*s %*s %d %d %d %d %d %d %d", &data1, &data2, &data3, &data4, &data5, &data6, &data7) >= 4) {
                    sprintf(valores_sensores, "%d %d", data1, data2);
                    sprintf(estados_ventiladores, "%d %d %d %d", data3, data4, data5, data6);
                }

                if(salas[sala_id].id == -1) memcpy(mss, "ERROR03", strlen("ERROR03"));
                else {
                    if(salas[sala_id].inicializado) memcpy(mss, "ERROR05", strlen("ERROR05"));
                    else {
                        salas[sala_id].dados_sensores = valores_sensores;
                        salas[sala_id].ventiladores = estados_ventiladores;
                        salas[sala_id].inicializado = 1; 
                        memcpy(mss, "OK02", strlen("OK02"));
                    }
                }
            } else if(strncmp(buf, "DES_REQ", 7) == 0) { // 3a funcionalidade
                char *sala_id_s = strchr(buf, ' ');
                int sala_id = atoi(sala_id_s); // converte de char para int
                if(salas[sala_id].id == -1) memcpy(mss, "ERROR03", strlen("ERROR03"));
                else {
                    if(salas[sala_id].inicializado == 0) memcpy(mss, "ERROR06", strlen("ERROR06"));
                    else {
                        strcpy(salas[sala_id].dados_sensores, "-1 -1");
                        strcpy(salas[sala_id].ventiladores, "-1 -1 -1 -1"); 
                        memcpy(mss, "OK03", strlen("OK03"));
                    }
                }
            } else if(strncmp(buf, "ALT_REQ", 7) == 0) { // 4a funcionalidade
                char *sala_id_s = strchr(buf, ' ');
                int sala_id = atoi(sala_id_s); // converte de char para int
                char *valores_sensores = malloc(strlen(buf) * sizeof(char));
                char *estados_ventiladores = malloc(strlen(buf) * sizeof(char));
                int data1, data2, data3, data4, data5, data6, data7;

                if (sscanf(buf, "%*s %*s %d %d %d %d %d %d %d", &data1, &data2, &data3, &data4, &data5, &data6, &data7) >= 4) {
                    sprintf(valores_sensores, "%d %d", data1, data2);
                    sprintf(estados_ventiladores, "%d %d %d %d", data3, data4, data5, data6);
                }

                if(salas[sala_id].id == -1) memcpy(mss, "ERROR03", strlen("ERROR03"));
                else {
                    if(salas[sala_id].inicializado == 0) memcpy(mss, "ERROR06", strlen("ERROR06"));
                    else {
                        strcpy(salas[sala_id].dados_sensores, valores_sensores);
                        strcpy(salas[sala_id].ventiladores, estados_ventiladores); 

                        memcpy(mss, "OK04", strlen("OK04"));
                    }
                }
            } else if(strncmp(buf, "SAL_REQ", 7) == 0) { // 5a funcionalidade
                char *sala_id_s = strchr(buf, ' ');
                int sala_id = atoi(sala_id_s);
                
                if((sala_id == 0 && strncmp(sala_id_s, " info", strlen(" info")) == 0) || sala_id < MIN_ID_SALA || sala_id > MAX_ID_SALA) memcpy(mss, "ERROR01", strlen("ERROR01"));
                else if(salas[sala_id].id == -1) memcpy(mss, "ERROR03", strlen("ERROR03"));
                else {

                    if(salas[sala_id].inicializado == 0) memcpy(mss, "ERROR06", strlen("ERROR06"));
                    else {
                        char* resp = malloc(8 + strlen(salas[sala_id].dados_sensores) + strlen(salas[sala_id].ventiladores) + 1);
                        sprintf(resp, "SAL_RES %d %s %s", sala_id, salas[sala_id].dados_sensores, salas[sala_id].ventiladores);

                        memcpy(mss, resp, strlen(resp));
                        free(resp);
                    }
                }
            } else if(strncmp(buf, "VAL_REQ", 7) == 0) { // 6a funcionalidade
                int aux = 0;
                for(int i = MIN_ID_SALA; i <= MAX_ID_SALA; i++) if(salas[i].id != -1) aux = 1; // verifica se ha salas cadastradas

                if(aux == 0) memcpy(mss, "ERROR03", strlen("ERROR03"));
                else {
                    char *dados_salas = concatenar_salas(salas);
                    char *resp = malloc(strlen("CAD_RES ") + strlen(dados_salas));
                    sprintf(resp, "CAD_RES %s", dados_salas);
                    memcpy(mss, resp, strlen(resp));
                    free(resp);
                }
            } 
            
            //strcpy(buf, mss); 
            //count = send(csock, buf, strlen(buf)+1, 0); // manda a resposta para o cliente | count -> nmr de bytes
            //if(count != strlen(buf)+1) exit(EXIT_FAILURE);
            count = send(csock, mss, strlen(mss)+1, 0); // manda a resposta para o cliente | count -> nmr de bytes
            if(count != strlen(mss)+1) exit(EXIT_FAILURE);
        }

        close(csock); 
    }

    for(int i = 0; i < 8; i++) {
        free(salas[i].ventiladores);
        free(salas[i].dados_sensores);
    }
    free(salas);
}