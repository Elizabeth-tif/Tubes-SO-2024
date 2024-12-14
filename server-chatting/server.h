#ifndef SERVER_H
#define SERVER_H

#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>

void handle_client(int client_socket, int client_id);
void trim_newline(char *str);
void sigchld_handler(int signo); // Deklarasi signal handler
#endif
