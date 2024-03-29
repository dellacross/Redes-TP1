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

char *get_datas(char *str) {
    int spaces = 0;
    char *start = str;

    while (*str && spaces < 2) {
        if (*str == ' ') {
            spaces++;
            if (spaces == 2) start = str + 1; // Start at the character after the second space
        }
        str++;
    }

    if (spaces < 2) return NULL; // Less than 2 spaces found, return NULL

    return strdup(start); // Return a copy of the substring
}

char *get_datas_from_file(char *filename) {
    //printf("filename: %s %ld\n", filename, strlen(filename));
    FILE *file = fopen(filename, "r");
    if (!file) {
        fprintf(stderr, "Erro ao abrir o arquivo %s\n", filename);
        return NULL;
    }

    // Obtém o tamanho do arquivo
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    // Aloca memória para armazenar o conteúdo do arquivo
    char *content = (char *)malloc(file_size + 1);
    if (!content) {
        fprintf(stderr, "Erro ao alocar memória\n");
        fclose(file);
        return NULL;
    }

    // Lê o conteúdo do arquivo e remove os caracteres de nova linha
    size_t total_read = 0;
    int number;
    while (fscanf(file, "%d", &number) == 1) {
        total_read += snprintf(content + total_read, file_size - total_read, "%d ", number);
    }
    content[total_read - 1] = '\0'; // Substitui o último espaço por um terminador nulo

    fclose(file);
    return content;
}

int valid_sensor_values_identifier(char *str) {
    char *token;
    char *copy = strdup(str);

    if (!copy) {
        fprintf(stderr, "Erro ao alocar memória\n");
        return 0;
    }

    token = strtok(copy, " ");
    while (token != NULL) {
        int value = atoi(token);
        if (value < 0 || value > 100) {
            free(copy);
            return 0; // Valor inválido encontrado
        }
        token = strtok(NULL, " ");
    }

    free(copy);
    return 1; // Todos os valores são válidos
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
    char mss[BUFSZ];
    memset(buf, 0, BUFSZ); //inicializa o buffer como 0
    memset(mss, 0, BUFSZ);
    printf("mensagem: ");
    fgets(buf, BUFSZ-1, stdin); //le do teclado o que o user vai digitar
    
    size_t count;

    if(strncmp(buf, "register", 8) == 0) { // 1a funcionalidade
        char *sala_id_s = strchr(buf, ' ');
        //printf("sala_id: %s", sala_id_s);
        if(valid_class_identifier(sala_id_s)) {
            //printf("valid\n");
            //mss = "CAD_REQ";
            memcpy(mss, "CAD_REQ", 7);
            strcat(mss, sala_id_s);
            //printf("valid: %s\n", mss);
            count = send(_socket, mss, strlen(mss)+1, 0);
        }
        else {
            printf("ERROR 01\n");
            exit(EXIT_FAILURE);
        }
    }
    else if(strncmp(buf, "init info", 9) == 0 || strncmp(buf, "init file", 9) == 0) { // 2a funcionalidade
        char *datas;
        if(strncmp(buf, "init info", 9) == 0) datas = get_datas(buf);
        else {
            // manipulacao para retornar o nome correto do arquivo, sem caracteres indesejados
            char *filename = strrchr(buf, ' ');
            char *result = (char *)malloc(strlen(filename)-1);
            strncpy(result, filename+1, strlen(filename)-2);
            result[strlen(filename)-2] = '\0';
            datas = get_datas_from_file(result);
        }
        memcpy(mss, "INI_REQ ", 8);
        strcat(mss, datas);
        char *sala_id_s = (char *)(&datas[0]);
        if(valid_class_identifier(sala_id_s)) {
            char * values = strchr(datas, ' ');
            if(valid_sensor_values_identifier(values)) count = send(_socket, mss, strlen(mss)+1, 0);
            else {
                printf("ERROR 04\n");
                exit(EXIT_FAILURE);
            }
        }
        else {
            printf("ERROR 01\n");
            exit(EXIT_FAILURE);
        }
    }
    else if(strncmp(buf, "shutdown", 8) == 0) { // 3a funcionalidade
        char *sala_id_s = strchr(buf, ' ');
        memcpy(mss, "DES_REQ", 7);
        strcat(mss, sala_id_s);
        count = send(_socket, mss, strlen(mss)+1, 0);
    }
    else if(strncmp(buf, "update info", 11) == 0 || strncmp(buf, "update file", 11) == 0) { // 4a funcionalidade
        char *datas;
        if(strncmp(buf, "update info", 11) == 0) datas = get_datas(buf);
        else {
            // manipulacao para retornar o nome correto do arquivo, sem caracteres indesejados
            char *filename = strrchr(buf, ' ');
            char *result = (char *)malloc(strlen(filename)-1);
            strncpy(result, filename+1, strlen(filename)-2);
            result[strlen(filename)-2] = '\0';
            datas = get_datas_from_file(result);
        }
        memcpy(mss, "ALT_REQ ", 8);
        strcat(mss, datas);
        char *sala_id_s = (char *)(&datas[0]);
        if(valid_class_identifier(sala_id_s)) {
            char * values = strchr(datas, ' ');
            if(valid_sensor_values_identifier(values)) count = send(_socket, mss, strlen(mss)+1, 0);
            else {
                printf("ERROR 04\n");
                exit(EXIT_FAILURE);
            }
        }
        else {
            printf("ERROR 01\n");
            exit(EXIT_FAILURE);
        }
    }
    else if(strncmp(buf, "load info", 9) == 0) { // 5a funcionalidade
        char *sala_id_s = strrchr(buf, ' ');
        memcpy(mss, "SAL_REQ", 7);
        strcat(mss, sala_id_s);
        count = send(_socket, mss, strlen(mss)+1, 0);
    }   
    else if(strncmp(buf, "load rooms", 10) == 0) { // 6a funcionalidade
        memcpy(mss, "CAD_REQ", 7);
        count = send(_socket, mss, strlen(mss)+1, 0);
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