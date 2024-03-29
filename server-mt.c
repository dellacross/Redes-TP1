#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define BUFSZ 1024 //buff size

void usage(int argc, char **argv) {
    // recebe o tipo de protocolo do servidor e o porto onde ficara esperando
    printf("usage: %s <v4/v6> <server port>\n", argv[0]);
    printf("example: %s v4 51551\n", argv[0]);
    exit(EXIT_FAILURE);
}

struct client_data {
    int csock; //socket do cliente
    struct sockaddr_storage storage;
};

void *client_thread(void *data) {
    struct client_data *cdata = (struct client_data *)data;
    struct sockaddr *caddr = (struct sockaddr *)(&cdata->storage);

    char caddrstr[BUFSZ];
    addrtostr(caddr, caddrstr, BUFSZ);
    printf("[log] conenction from %s\n", caddrstr); // printa sempre que o cliente conecta

    //leitura da msg do cliente
    char buf[BUFSZ];
    memset(buf, 0, BUFSZ); 
    size_t count = recv(cdata->csock, buf, BUFSZ-1, 0); // retorna numero de bytes recebido
                                                    // csock -> socket de onde vai receber (socket do cliente) | buf -> onde se coloca o dado do cliente
    // nesse caso, o que chegar no primeiro recv sera a msg do cliente a ser considerada (msm se incompleta)

    printf("[msg] %s, %d bytes: %s\n", caddrstr, (int)count, buf); // printa a msg do cliente
                                                                    
    sprintf(buf, "remote endpoint: %.1000s\n", caddrstr); // limita print para 1000 bytes
    count = send(cdata->csock, buf, strlen(buf)+1, 0); // manda a resposta para o cliente | count -> nmr de bytes
    if(count != strlen(buf)+1) logexit("send");
    close(cdata->csock);

    pthread_exit(EXIT_SUCCESS);
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

        struct client_data *cdata = malloc(sizeof(*cdata));
        if(!cdata) logexit("malloc");
        cdata->csock = csock;
        memcpy(&(cdata->storage), &cstorage, sizeof(cstorage)); //copia a estrutura inteira de cstorage para cdata->storage 

        // se nao deu erro, deve-se criar uma thread 
        pthread_t tid;
        pthread_create(&tid, NULL, client_thread, cdata); // funcao que roda "em paralelo" e permite o servidor "voltar para o while", ou seja, sempre esta pronto para receber o novo cliente                                           
    }
}