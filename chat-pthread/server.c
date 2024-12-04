#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <errno.h>

#define MAX_CLIENTS 100
#define BUFFER_SIZE 2048  // Increased buffer size for more message capacity
#define USERNAME_LENGTH 50
#define MAX_MESSAGE_LENGTH (BUFFER_SIZE - USERNAME_LENGTH - 10)  // Reserved space for username and formatting

// Structure for client information
typedef struct {
    int socket;
    int id;
    char username[USERNAME_LENGTH];
    int active;
} ClientInfo;

// Global variables
ClientInfo clients[MAX_CLIENTS];
int client_count = 0;
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

// Function to trim newline character from strings
void trim_newline(char *str) {
    size_t length = strlen(str);
    if (length > 0 && str[length - 1] == '\n') {
        str[length - 1] = '\0';
    }
}

// Function to broadcast message to all clients
void broadcast_message(char *message, int sender_id) {
    if (!message) return;

    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        // Send to all clients except the sender
        if (clients[i].active && clients[i].id != sender_id) {
            if (send(clients[i].socket, message, strlen(message), 0) < 0) {
                perror("Failed to broadcast message");
            }
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

// Safe message creation function to prevent truncation
int create_safe_message(char *dest, size_t dest_size, const char *username, const char *message) {
    // Ensure username doesn't exceed half the buffer
    char safe_username[USERNAME_LENGTH];
    strncpy(safe_username, username, sizeof(safe_username) - 1);
    safe_username[sizeof(safe_username) - 1] = '\0';

    // Truncate message if too long
    char safe_message[BUFFER_SIZE];
    strncpy(safe_message, message, sizeof(safe_message) - 1);
    safe_message[sizeof(safe_message) - 1] = '\0';

    // Create the formatted message safely
    return snprintf(dest, dest_size, "%s: %s", safe_username, safe_message);
}

// Function to handle client communication
void* handle_client(void *arg) {
    int client_socket = *((int*)arg);
    char buffer[BUFFER_SIZE] = {0};
    char username[USERNAME_LENGTH] = {0};
    char full_message[BUFFER_SIZE] = {0};
    int client_id = -1;

    // Send prompt for username
    const char *prompt = "Masukkan username: ";
    if (send(client_socket, prompt, strlen(prompt), 0) < 0) {
        perror("Failed to send username prompt");
        close(client_socket);
        return NULL;
    }

    // Receive username from client
    ssize_t username_length = recv(client_socket, username, sizeof(username) - 1, 0);
    if (username_length <= 0) {
        perror("Failed to receive username");
        close(client_socket);
        return NULL;
    }

    // Ensure null termination and trim newline
    username[username_length] = '\0';
    trim_newline(username);

    // Add client to the list
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (!clients[i].active) {
            clients[i].socket = client_socket;
            clients[i].id = i;
            clients[i].active = 1;
            strncpy(clients[i].username, username, sizeof(clients[i].username) - 1);
            client_count++;
            client_id = i;
            break;
        }
    }
    pthread_mutex_unlock(&clients_mutex);

    // Send message to notify everyone that the client joined
    memset(full_message, 0, sizeof(full_message));
    create_safe_message(full_message, sizeof(full_message), "Server",
                        username ? username : "Unknown" " bergabung dalam chat");
    broadcast_message(full_message, client_id);

    // Main loop to handle chat messages
    while (1) {
        memset(buffer, 0, sizeof(buffer));
        ssize_t valread = recv(client_socket, buffer, sizeof(buffer) - 1, 0);

        if (valread <= 0) {
            // Client disconnected
            break;
        }

        buffer[valread] = '\0';
        trim_newline(buffer);

        // Check for exit command
        if (strcmp(buffer, "/exit") == 0) {
            break;
        }

        // Safely create full message with username
        memset(full_message, 0, sizeof(full_message));
        create_safe_message(full_message, sizeof(full_message), username, buffer);

        // Broadcast the message
        broadcast_message(full_message, client_id);
    }

    // Handle client disconnect
    memset(full_message, 0, sizeof(full_message));
    create_safe_message(full_message, sizeof(full_message), "Server",
                        username ? username : "Unknown" " keluar dari chat");
    broadcast_message(full_message, client_id);

    // Clean up and close the connection
    close(client_socket);

    // Mark the client as inactive
    pthread_mutex_lock(&clients_mutex);
    clients[client_id].active = 0;
    client_count--;
    pthread_mutex_unlock(&clients_mutex);

    return NULL;
}

int main() {
    int server_fd, new_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);

    // Initialize client array
    memset(clients, 0, sizeof(clients));

    // Create socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Allow socket reuse to prevent "Address already in use" errors
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT,
                   &opt, sizeof(opt))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    // Configure server address
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(8080);

    // Bind the socket to the address
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    // Start listening for incoming connections
    if (listen(server_fd, MAX_CLIENTS) < 0) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    printf("Server chat berjalan di port 8080...\n");

    // Main loop to accept new clients
    while (1) {
        // Accept new client connection
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address,
                                 (socklen_t*)&addrlen)) < 0) {
            perror("Accept failed");
            continue;
        }

        // Check if max clients reached
        if (client_count >= MAX_CLIENTS) {
            const char *max_clients_msg = "Server penuh. Silakan coba lagi nanti.\n";
            send(new_socket, max_clients_msg, strlen(max_clients_msg), 0);
            close(new_socket);
            continue;
        }

        // Create a new thread for each client
        pthread_t thread_id;
        if (pthread_create(&thread_id, NULL, handle_client, (void*)&new_socket) != 0) {
            perror("Thread creation failed");
            close(new_socket);
            continue;
        }

        // Detach the thread to allow automatic resource cleanup
        pthread_detach(thread_id);
    }

    // Close server socket (this line is technically unreachable, but good practice)
    close(server_fd);

    return 0;
}