#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include "server.h"

#define MSG_220 "220 srvFtp version 1.0\r\n"
#define MSG_331 "331 Password required for %s\r\n"
#define MSG_230 "230 User %s logged in\r\n"
#define MSG_530 "530 Login incorrect\r\n"
#define MSG_221 "221 Goodbye\r\n"
#define MSG_550 "550 %s: no such file or directory\r\n"
#define MSG_299 "299 File %s size %ld bytes\r\n"
#define MSG_226 "226 Transfer complete\r\n"

/* 
 * Función recv_cmd recibe un comando del cliente y lo separa en operación y parámetro.
 *
 * @param socket_descriptor: Descriptor del socket desde el cual se recibe el comando.
 * @param operation: Cadena donde se almacenará la operación del comando 
 *                   (comandos básicos de ftp, por ejemplo: "USER", "PASS", "LIST", etc.).
 * @param param: Cadena donde se almacenará el parámetro del comando, si existe.
 * @return 0 si se recibió correctamente, 1 si hubo un error.
 */
int recv_cmd(int socket_descriptor, char *operation, char *param) {
    char buffer[BUFSIZE];
    char * token;

    if (recv(socket_descriptor, buffer, BUFSIZE, 0) < 0) {
        fprintf(stderr, "Error: no se pudo recibir el comando.\n");
        return 1;
    }
    buffer[strcspn(buffer, "\r\n")] = 0;
    token = strtok(buffer, " ");
    if (token == NULL || strlen(token) < 4) {
        fprintf(stderr, "Error: comando no válido.\n");
        return 1;
    } else {
        strcpy(operation, token);
        token = strtok(NULL, " ");
        #if DEBUG 
        printf("par %s\n", token);
        #endif
        if (token != NULL) strcpy(param, token);
    }
    return 0;
}