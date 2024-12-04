#ifndef SERVER_H
#define SERVER_H

void handle_client(int client_socket, int client_id);
void trim_newline(char *str);

#endif