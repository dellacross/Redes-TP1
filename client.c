#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define BUFSZ 1024 //buff size

void usage(int argc, char **argv) {
    printf("usage: %s <server IP> <server port>\n", argv[0]);
    printf("example: %s 127.0.0.1 51551\n", argv[0]);
    exit(EXIT_FAILURE);
}

int valid_class_identifier(char *sala_id_s) {
    int sala_id = atoi(sala_id_s);
    return sala_id >= 0 && sala_id <= 7;
}

int main(int argc, char **argv) {
    if (argc < 3) usage(argc, argv);

    // o connect recebe um ponteiro para a struct sockaddr
    struct sockaddr_storage storage;
    if(addrparse(argv[1], argv[2], &storage) != 0) usage(argc, argv); // argv[1] -> endereco do servidor | argv[2] -> porto que recebeu | ponteiro para o storage 

    int _socket;
    _socket = socket(storage.ss_family, SOCK_STREAM, 0); //storage.ss_family -> tipo do protocolo (ipv4 | ipv6) | SOCK_STREAM -> SOCKET TCP
    if(_socket == -1) logexit("socket");

    struct sockaddr *addr = (struct sockaddr *)(&storage); //pega o ponteiro para o storage, converte para o tipo do ponteiro *addr (sockaddr) e joga para a variavel addr, para depois passar para o connect

    if(connect(_socket, addr, sizeof(storage)) != 0) logexit("connect"); //addr -> endereco do servidor

    char addrstr[BUFSZ];
    addrtostr(addr, addrstr, BUFSZ);

    printf("conntected to %s\n", addrstr);

    // cliente manda uma mensagem (linha 43 a 49)
    char buf[BUFSZ];
    char *mss;
    memset(buf, 0, BUFSZ); //inicializa o buffer como 0

    printf("mensagem: ");
    fgets(buf, BUFSZ-1, stdin); //le do teclado o que o user vai digitar
    
    size_t count;

    if(strncmp(buf, "register", 8) == 0) {
        char *sala_id_s = strchr(buf, ' ');
        //printf("sala_id: %s", sala_id_s);
        if(valid_class_identifier(sala_id_s)) {
            //printf("valid\n");
            mss = "CAD_REQ";
            count = send(_socket, mss, strlen(mss)+1, 0);
        }
        else {
            printf("ERROR 01\n");
            exit(EXIT_FAILURE);
        }
    }
    else printf("other action\n");

    //size_t count = send(_socket, buf, strlen(buf)+1, 0); //(socket, buffer, tamanho da mensagem)
    // count -> nmr de bytes efetivamente transmitidos na rede
    if(count != (strlen(mss)+1)) logexit("exit"); // se o nmr de bytes for diferente do que foi pedido para se transmitir (strlen(buf)+1)

    // cliente recebe uma resposta (linha 51 a 62)
    memset(buf, 0, BUFSZ); //inicializa o buffer como 0

    // o recv recebe o dado, mas o servidor pode mandar o dado parcelas pequenas, logo, deve-se receber até o count == 0. Com isso, deve-se criar uma variavel (total) que armazena o total de dados recebidos até o momento, mas sempre colocar o dado no local correto do buffer (buff)
    
    unsigned total = 0;
    while(1) { // recebe x bytes por vezes e coloca em ordem no buffer (buff) até o recv retornar 0 (servidor terminou de mandar os dados)
        count = recv(_socket, buf + total, BUFSZ - total, 0); //recebe a resposta do servidor (recebe o dado no socket, coloca-o no buf e limita o tamanho do dado em BUFFSZ)

        if(count == 0) break; // não recebeu nada (só ocorre qnd a conexão está fechada) - conexão finalizada

        total+=count;                                           
    }
    close(_socket);

    printf("received %u bytes\n", total);
    puts(buf);

    exit(EXIT_SUCCESS);
}