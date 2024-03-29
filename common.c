#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

void logexit(const char *msg) {
    perror(msg); //func perror -> imprime a msg do erro e o que ocorreu no erro
    exit(EXIT_FAILURE);
}

int addrparse(const char *addrstr, const char *portstr, struct sockaddr_storage *storage) {
    if(addrstr == NULL || portstr == NULL) return -1;

    uint16_t port = (uint16_t)atoi(portstr); //unsigned short -> 16bits (padrao do protocolo da internet)

    if(port == 0) return -1;

    port = htons(port); //host to network short -> converte a representacao de dispositivo para representacao de rede (o contrario é ntohs())

    struct in_addr inaddr4; //32 bits ip address (ipv4)
    if(inet_pton(AF_INET, addrstr, &inaddr4)) { 
        // tentativa de parser de ipv4 (indicado pelo AF_INET), parser do addstr e, se der certo, joga no inaddr4
        // se o parser der certo, o protocolo é ipv4. Mas, para inicializar o struct sockaddr_storage storage, deve-se converte-lo para sockaddr_in (que é o struct sockaddr da internet)

        struct sockaddr_in *addr4 = (struct sockaddr_in *)storage; // o *addr4 aponta para o storage passado como parametro. Logo, todas inicializacoes abaixo são feitas no *storage
        addr4->sin_family = AF_INET;
        addr4->sin_port = port;
        addr4->sin_addr = inaddr4;

        return 0;
    }

    struct in6_addr inaddr6; //128 bits ipv6 address (ipv6)
    if(inet_pton(AF_INET6, addrstr, &inaddr6)) { 
        // tentativa de parser de ipv6 (indicado pelo AF_INET), parser do addstr e, se der certo, joga no inaddr6
        // se o parser der certo, o protocolo é ipv6. Mas, para inicializar o struct sockaddr_storage storage, deve-se converte-lo para sockaddr_in (que é o struct sockaddr da internet)

        struct sockaddr_in6 *addr6 = (struct sockaddr_in6 *)storage; // o *addr6 aponta para o storage passado como parametro. Logo, todas inicializacoes abaixo são feitas no *storage
        addr6->sin6_family= AF_INET6;
        addr6->sin6_port = port;
        //addr6->sin_addr = inaddr6;
        memcpy(&(addr6->sin6_addr), &inaddr6, sizeof(inaddr6));

        return 0;
    }

    return -1; // nao conseguiu fazer o parse nem para ipv4 nem para ipv6
}

void addrtostr(const struct sockaddr *addr, char *str, size_t strsize) {
    int version;
    char addrstr[INET6_ADDRSTRLEN+1] = "";
    uint16_t port;

    if(addr->sa_family == AF_INET) {
        version = 4;
        struct sockaddr_in *addr4 = (struct sockaddr_in *)addr;

        if(!inet_ntop(AF_INET, &(addr4->sin_addr), addrstr, INET6_ADDRSTRLEN+1)) logexit("ntop"); // passa de representacao de rede para representacao textual
        
        port = ntohs(addr4->sin_port); //network to host short

    } else if(addr->sa_family == AF_INET6) {
        version = 6;
        struct sockaddr_in6 *addr6 = (struct sockaddr_in6 *)addr;

        if(!inet_ntop(AF_INET6, &(addr6->sin6_addr), addrstr, INET6_ADDRSTRLEN+1)) logexit("ntop"); // passa de representacao de rede para representacao textual
        
        port = ntohs(addr6->sin6_port); //network to host short
    } else logexit("unknown protocol family");

    if(str) snprintf(str, strsize, "IPV%d %s %hu", version, addrstr, port); //depois de pegar o addrstr e o port, printa na variavel passada (addrstr)
}

int server_sockaddr_init(const char *proto, const char* portstr, struct sockaddr_storage *storage) {

    uint16_t port = (uint16_t)atoi(portstr); //unsigned short -> 16bits (padrao do protocolo da internet)

    if(port == 0) return -1;

    port = htons(port); //host to network short -> converte a representacao de dispositivo para representacao de rede (o contrario é ntohs())

    memset(storage, 0, sizeof(*storage)); //evita dor de cabeça (by italo)

    //qual protocolo a pessoa mandou rodar
    if(strcmp(proto, "v4") == 0) { //ipv4 
        struct sockaddr_in *addr4 = (struct sockaddr_in *)storage;
        addr4->sin_family = AF_INET;
        addr4->sin_addr.s_addr = INADDR_ANY; // qualquer endereco que o computador tenha na interface dele
        addr4->sin_port = port;
        return 0;
    } else if(strcmp(proto, "v6") == 0) { //ipv6
        struct sockaddr_in6 *addr6 = (struct sockaddr_in6 *)storage;
        addr6->sin6_family = AF_INET6;
        addr6->sin6_addr = in6addr_any; // qualquer endereco que o computador tenha na interface dele
        addr6->sin6_port = port;
        return 0;
    } else return -1;
}