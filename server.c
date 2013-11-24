
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

#define REQUEST_SIZE 1000
#define MAX_CLIENTS 1000
#define BACKLOG 10


void error(char* msg) {
  printf("%s", msg);
//  exit(1);
}


int make_server_socket(int port) {
  // Defined in netinet/in.h
  struct sockaddr_in serv_addr;
  
  // Create a new socket for the server
  int server_socket = socket(AF_INET, SOCK_STREAM, 0);
  if (server_socket < 0)
    error("ERROR opening socket\n");
  
  // Initalize the buffer to zero
  bzero((char *) &serv_addr, sizeof(serv_addr));
  
  serv_addr.sin_family = AF_INET;
  // Address of the host, i.e. the machine running the server proc
  serv_addr.sin_addr.s_addr = INADDR_ANY;
  // Set the port, first accounting for network byte ordering
  serv_addr.sin_port = htons(port);
  
  // Bind the socket fd to addr and port of the host
  if (bind(server_socket, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
    error("ERROR on binding\n");
  
  printf("Server socket created.\n");
  return server_socket;
}


int get_client(int server_socket) {
  // Defined in netinet/in.h
  struct sockaddr_in cli_addr;
  socklen_t clilen = sizeof(cli_addr);
  
  // Listen for a connection from a client
  listen(server_socket, BACKLOG);
  
  // Accept and return a client
  return accept(server_socket, (struct sockaddr *) &cli_addr, &clilen);
}

/*
int main(int argc, char* argv[]) {
  //8080 is the default port, the user can change this at runtime though
  int PORT = (argc == 2) ? atoi(argv[1]) : 8080;
  
  int server_socket = make_server_socket(PORT);
  
  // Start accepting clients, forking for each new one
  int i;
  for(i=0; i<MAX_CLIENTS; i++) {
    
    int client_socket = get_client(server_socket);
    if (client_socket < 0) {
      printf("ERROR adding client\n");
      continue;
    }
    
    printf("Another client accepted for a total of %d\n", i+1);
    
    // Fork a process for handling the request
    // Will later change this to giving to a thread
    // Parent just continues the loop, wainting for another client
    if(fork() == 0) {
      // Read a request from the client
      char buffer[REQUEST_SIZE];
      bzero(buffer, REQUEST_SIZE);
      
      int n = read(client_socket, buffer, REQUEST_SIZE-1);
      if (n < 0)
        error("ERROR reading from socket\n");
      
      // Print the request
      printf("From client %d:\n%s\n", i, buffer);
      
      // DO SHIT
      // This part of it be difficult
      // Get the workers, try to find a new one
      // If there is one, give it the task of this user
      // If there not be one, make a new one
      // If there are too many, kill idle ones
      
      return 0;
    }
    // Close socket?
  }
  // The main loop is over, so close the socket and finish up
  close(server_socket);
  return 0;
}
  */





