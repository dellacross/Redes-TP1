#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define BUFSZ 500 //buff size

#define ERROR01 "Sala Inválida\n"
#define ERROR02 "Sala já Existe\n"
#define ERROR03 "Sala Inexistente\n"
#define ERROR04 "Sensores Inválidos\n"
#define ERROR05 "Sensores já Instalados\n"
#define ERROR06 "Sensores não Instalados\n"

#define OK01 "Sala Instanciada com Sucesso\n"
#define OK02 "Sensores Inicializados com Sucesso\n"
#define OK03 "Sensores Desligados com Sucesso\n"
#define OK04 "Informações Atualizadas com Sucesso\n"

#define MIN_ID_SALA 0 //menor id valido para uma sala
#define MAX_ID_SALA 7 //maior id valido para uma sala
#define MIN_TEMP_SENSOR 0 //menor temperatura valida 
#define MAX_TEMP_SENSOR 40 //maior temperatura valida
#define MIN_UM_SENSOR 0 //menor umidade valida
#define MAX_UM_SENSOR 100 //maior umidade valida
#define MIN_ESTADO_VENT 0 //menor id para estado de ventilador
#define MAX_ESTADO_VENT 2 //maior id para estado de ventilador

#define MIN_VALID_SENSOR_DATAS_SZ 15 // tamanho minimo de uma string com dados dos sensores strlen(0 0 10 20 30 40)
#define MAX_VALID_SENSOR_DATAS_SZ 17 // tamanho minimo de uma string com dados dos sensores strlen(80 80 10 20 30 40)

void usage(int argc, char **argv) {
    printf("usage: %s <server IP> <server port>\n", argv[0]);
    exit(EXIT_FAILURE);
}

int valid_class_identifier(char *sala_id_s) { // identifica se o ID da sala eh valido
    int sala_id = atoi(sala_id_s);
    return sala_id >= MIN_ID_SALA && sala_id <= MAX_ID_SALA;
}

char *get_datas_from_command_line(char *datas) { //manipula uma string (datas) para retirar os valores dos dados
    int spaces = 0;
    char *str = datas;

    while (*datas && spaces < 2) {
        if (*datas == ' ') {
            spaces++;
            if (spaces == 2) str = datas + 1; 
        }
        datas++;
    }

    if (spaces < 2) return "Erro ao ler a entrada\n"; 

    if(strlen(str) < MIN_VALID_SENSOR_DATAS_SZ) return "-1";
    else return str;
}

char *get_datas_from_file(char *filename) {
    FILE *file = fopen(filename, "r");
    if (!file) { // verifica se o arquivo houve algum erro ao abrir o arquivo
        fprintf(stderr, "Erro ao abrir o arquivo %s\n", filename);
        return "Erro ao abrir o arquivo\n";
    }

    // identifica o tamanho do arquivo
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    char *content = (char *)malloc(file_size + 1);
    if (!content) {
        fprintf(stderr, "Erro ao alocar memória\n");
        fclose(file);
        return "Erro ao alocar memória\n";
    }

    // le o conteúdo do arquivo e remove os caracteres de nova linha
    size_t total_read = 0;
    int number;
    while (fscanf(file, "%d", &number) == 1) {
        total_read+=snprintf(content + total_read, file_size - total_read, "%d ", number);
    }
    content[total_read-1] = '\0'; // substitui o ultimo espaço por um terminador nulo

    fclose(file);
    return content;
}

int valid_sensor_values_identifier(char *str) { // identifica se os valores para os sensores e ventiladores sao validos
    int s1, s2, v1, v2, v3, v4;

    sscanf(str, " %d %d %d %d %d %d", &s1, &s2, &v1, &v2, &v3, &v4);

    if( // verifica se os valores para os sensores ou estado para os ventiladores sao validos
        s1 >= MIN_TEMP_SENSOR && 
        s1 <= MAX_TEMP_SENSOR && 
        s2 >= MIN_UM_SENSOR && 
        s2 <= MAX_UM_SENSOR &&
        (v1 - 10) >= MIN_ESTADO_VENT &&
        (v1 - 10) <= MAX_ESTADO_VENT &&
        (v2 - 20) >= MIN_ESTADO_VENT &&
        (v2 - 20) <= MAX_ESTADO_VENT &&
        (v3 - 30) >= MIN_ESTADO_VENT &&
        (v3 - 30) <= MAX_ESTADO_VENT &&
        (v4 - 40) >= MIN_ESTADO_VENT &&
        (v4 - 40) <= MAX_ESTADO_VENT
    ) return 1;
    else return 0;
}

int main(int argc, char **argv) {
    if (argc < 3) usage(argc, argv);

    // o connect recebe um ponteiro para a struct sockaddr
    struct sockaddr_storage storage;
    if(addrparse(argv[1], argv[2], &storage) != 0) usage(argc, argv); // argv[1] -> endereco do servidor | argv[2] -> porto que recebeu | ponteiro para o storage 

    int _socket;
    _socket = socket(storage.ss_family, SOCK_STREAM, 0); //storage.ss_family -> tipo do protocolo (ipv4 | ipv6) | SOCK_STREAM -> SOCKET TCP
    if(_socket == -1) exit(EXIT_FAILURE);

    struct sockaddr *addr = (struct sockaddr *)(&storage); //pega o ponteiro para o storage, converte para o tipo do ponteiro *addr (sockaddr) e joga para a variavel addr, para depois passar para o connect

    if(connect(_socket, addr, sizeof(storage)) != 0) exit(EXIT_FAILURE); //addr -> endereco do servidor

    char addrstr[BUFSZ];
    addrtostr(addr, addrstr, BUFSZ);

    // cliente manda uma mensagem (linha 43 a 49)
    
    unsigned total = 0;
    char buf[BUFSZ];
    char mss[BUFSZ];
    memset(buf, 0, BUFSZ); //inicializa o buffer como 0
    memset(mss, 0, BUFSZ);

    while(1) { // recebe x bytes por vezes e coloca em ordem no buffer (buff) até o recv retornar 0 (servidor terminou de mandar os dados)
        printf("mensagem: ");
        fgets(buf, BUFSZ-1, stdin); //le do teclado o que o user vai digitar

        printf("leu a msg: %s\n", buf);

        size_t count;

        if(strncmp(buf, "register", 8) == 0) { // 1a funcionalidade
            char *sala_id_s = strchr(buf, ' ');
            if(valid_class_identifier(sala_id_s)) {
                memcpy(mss, "CAD_REQ", 7);
                strcat(mss, sala_id_s);
                count = send(_socket, mss, strlen(mss)+1, 0);
            }
            else printf(ERROR01);
        }
        else if(strncmp(buf, "init info", 9) == 0 || strncmp(buf, "init file", 9) == 0) { // 2a funcionalidade
            char *datas = malloc(MAX_VALID_SENSOR_DATAS_SZ * sizeof(char));
            if(strncmp(buf, "init info", 9) == 0) {
                char *res = get_datas_from_command_line(buf);
                memcpy(datas, res, strlen(res));
            }
            else {
                // manipulacao para retornar o nome correto do arquivo, sem caracteres indesejados
                char *res = strrchr(buf, ' ');
                char *filename = (char *)malloc(strlen(res)-1);
                strncpy(filename, res+1, strlen(res)-2);
                filename[strlen(res)-2] = '\0';
                datas = get_datas_from_file(filename);
                if(filename) free(filename);
            }

            if(strncmp(datas, "-1", 2) == 0) printf(ERROR04);
            else {
                memcpy(mss, "INI_REQ ", strlen("INI_REQ "));
                strcat(mss, datas);
                char *sala_id_s = (char *)(&datas[0]);

                if(valid_class_identifier(sala_id_s) && strncmp(datas, "Erro", strlen("Erro")) != 0) {
                    char * values = strchr(datas, ' ');
                    if(valid_sensor_values_identifier(values)) count = send(_socket, mss, strlen(mss)+1, 0);
                    else printf(ERROR04);
                }
                else printf(ERROR01);
            }
            //if(datas) free(datas);
        }
        else if(strncmp(buf, "shutdown", 8) == 0) { // 3a funcionalidade
            char *sala_id_s = strchr(buf, ' ');
            memcpy(mss, "DES_REQ", strlen("DES_REQ"));
            strcat(mss, sala_id_s);
            count = send(_socket, mss, strlen(mss)+1, 0);
        }
        else if(strncmp(buf, "update info", strlen("update info")) == 0 || strncmp(buf, "update file", strlen("update file")) == 0) { // 4a funcionalidade
            char *datas;
            if(strncmp(buf, "update info", strlen("update info")) == 0) datas = get_datas_from_command_line(buf);
            else {
                // manipulacao para retornar o nome correto do arquivo, sem caracteres indesejados
                char *filename = strrchr(buf, ' '); 
                char *result = (char *)malloc(strlen(filename)-1);
                strncpy(result, filename+1, strlen(filename)-2);
                result[strlen(filename)-2] = '\0';
                datas = get_datas_from_file(result);
            }

            memcpy(mss, "ALT_REQ ", strlen("ALT_REQ "));
            strcat(mss, datas);
            char *sala_id_s = (char *)(&datas[0]); // identifica o ID da sala desejada
            if(valid_class_identifier(sala_id_s) && strncmp(datas, "Erro", strlen("Erro")) != 0) {
                char * values = strchr(datas, ' '); // identifica os dados para atualizacao
                if(valid_sensor_values_identifier(values)) count = send(_socket, mss, strlen(mss)+1, 0);
                else printf(ERROR04);
            }
            else printf(ERROR01);
        }
        else if(strncmp(buf, "load info", strlen("load info")) == 0) { // 5a funcionalidade
            char *sala_id_s = strrchr(buf, ' '); // identifica o ID da sala desejada (valor apos o ultimo caracter ' ')
            memcpy(mss, "SAL_REQ", strlen("SAL_REQ"));
            strcat(mss, sala_id_s);
            count = send(_socket, mss, strlen(mss)+1, 0);
        }   
        else if(strncmp(buf, "load rooms", strlen("load rooms")) == 0) { // 6a funcionalidade
            memcpy(mss, "VAL_REQ", strlen("VAL_REQ"));
            count = send(_socket, mss, strlen(mss)+1, 0);
        } else if(strncmp(buf, "kill", 4) == 0) {
            count = send(_socket, buf, strlen(buf)+1, 0);
            break;
        }
        else printf("Mensagem Inválida\n");

        // count -> nmr de bytes efetivamente transmitidos na rede
        //if(count != (strlen(mss)+1)) exit(EXIT_FAILURE); // se o nmr de bytes for diferente do que foi pedido para se transmitir (strlen(buf)+1)
    
        count = recv(_socket, buf + total, BUFSZ - total, 0); //recebe a resposta do servidor (recebe o dado no socket, coloca-o no buf e limita o tamanho do dado em BUFFSZ)
        printf("count %ld\n", count);
        if(count == 0) break; // não recebeu nada (só ocorre qnd a conexão está fechada) - conexão finalizada

        total+=count;        

        if(strncmp(buf, "OK01", 4) == 0) printf(OK01);
        else if(strncmp(buf, "OK02", 4) == 0) printf(OK02);
        else if(strncmp(buf, "OK03", 4) == 0) printf(OK03);
        else if(strncmp(buf, "OK04", 4) == 0) printf(OK04);
        else if(strncmp(buf, "SAL_RES", 7) == 0) {
            char *datas = strchr(buf, ' ');
            char *data_form = malloc(strlen(buf) * sizeof(char));

            int id, s1, s2, v1, v2, v3, v4;
            sscanf(datas, " %d %d %d %d %d %d %d", &id, &s1, &s2, &v1, &v2, &v3, &v4);
            sprintf(data_form, "sala %d: %d %d %d %d %d %d", id, s1, s2, v1, v2, v3, v4);
            printf("%s\n", data_form);
            
        } else if(strncmp(buf, "CAD_RES", 7) == 0) {
            char *datas = strchr(buf, ' ');
            printf("salas:%s\n", datas);
        } 
        else if(strncmp(buf, "ERROR01", 7) == 0) printf(ERROR01);
        else if(strncmp(buf, "ERROR02", 7) == 0) printf(ERROR02);
        else if(strncmp(buf, "ERROR03", 7) == 0) printf(ERROR03);
        else if(strncmp(buf, "ERROR04", 7) == 0) printf(ERROR04);
        else if(strncmp(buf, "ERROR05", 7) == 0) printf(ERROR05);
        else if(strncmp(buf, "ERROR06", 7) == 0) printf(ERROR06);
        
    }
    close(_socket);
}