#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <signal.h>
#include <sys/wait.h>
#include <errno.h>

#define MAX_CLIENTS 100
#define BUFFER_SIZE 2048
#define USERNAME_LENGTH 50
#define MAX_MESSAGE_LENGTH (BUFFER_SIZE - USERNAME_LENGTH - 10)

// Structure for client information
typedef struct {
    int socket;
    char username[USERNAME_LENGTH];
    pid_t pid;
} ClientInfo;

// Global variables
ClientInfo clients[MAX_CLIENTS];
int client_count = 0;
int server_fd;

// Trim newline from strings
void trim_newline(char *str) {
    size_t length = strlen(str);
    if (length > 0 && str[length - 1] == '\n') {
        str[length - 1] = '\0';
    }
}

// Broadcast a message safely to all clients except the sender
void safe_broadcast(char *message, pid_t sender_pid) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].socket > 0 && clients[i].pid != sender_pid) {
            send(clients[i].socket, message, strlen(message), 0);
        }
    }
}

// Cleanup function for terminated child processes
void sigchld_handler(int sig) {
    int status;
    pid_t pid;

    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i].pid == pid) {
                close(clients[i].socket);
                clients[i].socket = 0;
                clients[i].pid = 0;
                memset(clients[i].username, 0, sizeof(clients[i].username));
                client_count--;
                break;
            }
        }
    }
}

// Handle client communication in a forked process
void handle_client(int client_socket) {
    char buffer[BUFFER_SIZE] = {0};
    char username[USERNAME_LENGTH] = {0};
    char full_message[BUFFER_SIZE] = {0};
    char safe_buffer[MAX_MESSAGE_LENGTH] = {0};
    char safe_username[USERNAME_LENGTH] = {0};

    // Username prompt
    const char *prompt = "Masukkan username: ";
    send(client_socket, prompt, strlen(prompt), 0);

    // Receive username
    ssize_t username_length = recv(client_socket, username, sizeof(username) - 1, 0);
    if (username_length <= 0) {
        close(client_socket);
        exit(EXIT_FAILURE);
    }
    username[username_length] = '\0';
    trim_newline(username);

    // Safely copy username
    strncpy(safe_username, username, sizeof(safe_username) - 1);
    safe_username[sizeof(safe_username) - 1] = '\0';

    // Notify join
    snprintf(full_message, sizeof(full_message), "Server: %s bergabung dalam chat", safe_username);
    safe_broadcast(full_message, getpid());

    // Chat loop
    while (1) {
        memset(buffer, 0, sizeof(buffer));
        memset(safe_buffer, 0, sizeof(safe_buffer));

        ssize_t valread = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
        if (valread <= 0) break;

        buffer[valread] = '\0';
        trim_newline(buffer);

        if (strcmp(buffer, "/exit") == 0) break;

        // Safely truncate message
        strncpy(safe_buffer, buffer, sizeof(safe_buffer) - 1);
        safe_buffer[sizeof(safe_buffer) - 1] = '\0';

        // Prepare broadcast message with safe lengths
        memset(full_message, 0, sizeof(full_message));
        snprintf(full_message, sizeof(full_message), "%s: %s", safe_username, safe_buffer);
        safe_broadcast(full_message, getpid());
    }

    // Client exit notification
    memset(full_message, 0, sizeof(full_message));
    snprintf(full_message, sizeof(full_message), "Server: %s keluar dari chat", safe_username);
    safe_broadcast(full_message, getpid());

    close(client_socket);
    exit(EXIT_SUCCESS);
}

int main() {
    struct sockaddr_in address;
    int addrlen = sizeof(address);

    // Initialize clients array
    memset(clients, 0, sizeof(clients));

    // Create socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Allow socket reuse
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    // Configure server address
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(8080);

    // Bind socket
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    // Listen for connections
    if (listen(server_fd, MAX_CLIENTS) < 0) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    printf("Server chat berjalan di port 8080...\n");

    // Setup signal handler for child process cleanup
    signal(SIGCHLD, sigchld_handler);

    // Accept new clients
    while (1) {
        int new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen);
        if (new_socket < 0) {
            perror("Accept failed");
            continue;
        }

        // Check if max clients reached
        if (client_count >= MAX_CLIENTS) {
            const char *msg = "Server penuh. Silakan coba lagi nanti.\n";
            send(new_socket, msg, strlen(msg), 0);
            close(new_socket);
            continue;
        }

        // Fork process for client
        pid_t pid = fork();
        if (pid < 0) {
            perror("Fork failed");
            close(new_socket);
        } else if (pid == 0) {
            // Child process
            close(server_fd);
            handle_client(new_socket);
        } else {
            // Parent process
            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (clients[i].socket == 0) {
                    clients[i].socket = new_socket;
                    clients[i].pid = pid;
                    client_count++;
                    break;
                }
            }
        }
    }

    close(server_fd);
    return 0;
}