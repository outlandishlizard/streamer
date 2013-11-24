
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

#define REQUEST_SIZE 1000
#define MAX_CLIENTS 1000
#define BACKLOG 10

void error(char *msg);
int make_server_socket(int port);
int get_client(int server_socket);
