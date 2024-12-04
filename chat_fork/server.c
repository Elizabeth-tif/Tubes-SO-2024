#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>
#include <time.h>

#define MAX_CLIENTS 100
#define BUFFER_SIZE 1024
#define PORT 8080
#define USERNAME_LENGTH 50
#define CLIENT_ID_LENGTH 10

typedef struct {
    int socket;
    char username[USERNAME_LENGTH];
    char client_id[CLIENT_ID_LENGTH];
    pid_t pid;
} ClientInfo;

ClientInfo clients[MAX_CLIENTS];
int client_count = 0;
int server_fd;

// Generate a unique client ID
void generate_client_id(char *client_id) {
    static int counter = 0;
    counter++;
    snprintf(client_id, CLIENT_ID_LENGTH, "Client%d", counter);
}

// Trim newline from strings
void trim_newline(char *str) {
    size_t length = strlen(str);
    if (length > 0 && str[length - 1] == '\n') {
        str[length - 1] = '\0';
    }
}

// Broadcast message to all clients except sender
void safe_broadcast(char *message, const char *sender_client_id) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].socket > 0 &&
            strcmp(clients[i].client_id, sender_client_id) != 0) {
            send(clients[i].socket, message, strlen(message), 0);
        }
    }
}

// Handler for child process termination
void sigchld_handler(int sig) {
    int status;
    pid_t pid;

    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i].pid == pid) {
                printf("Client %s (%s) disconnected\n",
                       clients[i].username,
                       clients[i].client_id);

                close(clients[i].socket);
                clients[i].socket = 0;
                clients[i].pid = 0;
                memset(clients[i].username, 0, sizeof(clients[i].username));
                memset(clients[i].client_id, 0, sizeof(clients[i].client_id));
                client_count--;
                break;
            }
        }
    }
}

// Safer message formatting with length checks
void safe_format_message(char *full_message, size_t full_message_size,
                         const char *username, const char *client_id,
                         const char *buffer) {
    char safe_username[USERNAME_LENGTH];
    strncpy(safe_username, username, sizeof(safe_username) - 1);
    safe_username[sizeof(safe_username) - 1] = '\0';

    char safe_buffer[BUFFER_SIZE];
    strncpy(safe_buffer, buffer, sizeof(safe_buffer) - 1);
    safe_buffer[sizeof(safe_buffer) - 1] = '\0';

    snprintf(full_message, full_message_size, "[%s] %s: %s",
             client_id, safe_username, safe_buffer);
}

// Handle individual client communication
void handle_client(int client_socket, char *client_id) {
    char buffer[BUFFER_SIZE] = {0};
    char username[USERNAME_LENGTH] = {0};
    char full_message[BUFFER_SIZE] = {0};

    const char *prompt = "Enter your username: ";
    send(client_socket, prompt, strlen(prompt), 0);

    ssize_t username_length = recv(client_socket, username, sizeof(username) - 1, 0);
    if (username_length <= 0) {
        close(client_socket);
        exit(EXIT_FAILURE);
    }
    username[username_length] = '\0';
    trim_newline(username);

    // Construct join message
    snprintf(full_message, sizeof(full_message),
             "Server: [%s] %s joined the chat", client_id, username);
    safe_broadcast(full_message, client_id);
    printf("%s\n", full_message);

    while (1) {
        memset(buffer, 0, sizeof(buffer));

        ssize_t valread = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
        if (valread <= 0) break;

        buffer[valread] = '\0';
        trim_newline(buffer);

        if (strcmp(buffer, "/exit") == 0) break;

        // Format message with client ID
        safe_format_message(full_message, sizeof(full_message),
                            username, client_id, buffer);
        safe_broadcast(full_message, client_id);
        printf("%s\n", full_message);
    }

    // Construct leave message
    snprintf(full_message, sizeof(full_message),
             "Server: [%s] %s left the chat", client_id, username);
    safe_broadcast(full_message, client_id);
    printf("%s\n", full_message);

    close(client_socket);
    exit(EXIT_SUCCESS);
}

int main() {
    struct sockaddr_in address;
    int addrlen = sizeof(address);

    memset(clients, 0, sizeof(clients));

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, MAX_CLIENTS) < 0) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    printf("Chat server running on port %d...\n", PORT);

    signal(SIGCHLD, sigchld_handler);

    while (1) {
        int new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen);
        if (new_socket < 0) {
            perror("Accept failed");
            continue;
        }

        if (client_count >= MAX_CLIENTS) {
            const char *msg = "Server is full. Please try again later.\n";
            send(new_socket, msg, strlen(msg), 0);
            close(new_socket);
            continue;
        }

        // Generate unique client ID
        char client_id[CLIENT_ID_LENGTH];
        generate_client_id(client_id);

        pid_t pid = fork();
        if (pid < 0) {
            perror("Fork failed");
            close(new_socket);
        } else if (pid == 0) {
            close(server_fd);
            handle_client(new_socket, client_id);
        } else {
            sigset_t sigset;
            sigemptyset(&sigset);
            sigaddset(&sigset, SIGCHLD);

            sigprocmask(SIG_BLOCK, &sigset, NULL);

            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (clients[i].socket == 0) {
                    clients[i].socket = new_socket;
                    clients[i].pid = pid;
                    strncpy(clients[i].client_id, client_id, CLIENT_ID_LENGTH - 1);
                    client_count++;
                    break;
                }
            }

            sigprocmask(SIG_UNBLOCK, &sigset, NULL);
        }
    }

    close(server_fd);
    return 0;
}