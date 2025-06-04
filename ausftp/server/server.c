#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include "serverdtp.h"

#define VERSION "1.0"
#define PORT_DEFAULT 21

int main(int argc, char *argv[]){
    int port;
    if (argc > 2) {
        fprintf(stderr, "Error: se solicita solo un número de puerto o ninguno.\n");
        return 1;
    }
    if (argc == 2) {
        port = atoi(argv[1]);
        if (port <= 0 || port > 65535) {
            fprintf(stderr, "Error: el número de puerto debe ser un entero entre 1 y 65535.\n");
            return 1;
        }
    } else {
        port = PORT_DEFAULT; // Puerto por defecto
    }

    printf("Check credentials: %d\n", check_credentials("test", "test"));

    int master_socket, slave_socket;
    struct sockaddr_in master_addr, slave_addr;
    socklen_t slave_addr_len;

    master_socket = socket(AF_INET, SOCK_STREAM, 0);
    master_addr.sin_family = AF_INET;
    master_addr.sin_addr.s_addr = INADDR_ANY; 
    master_addr.sin_port = htons(port);
    bind(master_socket, (struct sockaddr *)&master_addr, sizeof(master_addr)) < 0;
    listen(master_socket, 5);
    while (1) {
        slave_addr_len = sizeof(slave_addr);
        slave_socket = accept(master_socket, (struct sockaddr *)&slave_addr, &slave_addr_len);
        send(slave_socket, "220 1", sizeof("220 1"), 0);
        printf("Funcionó\n");
    }
    close(master_socket);

    return 0;
}