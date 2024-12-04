#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <signal.h>
#include <errno.h>

#define BUFFER_SIZE 1024
#define PORT 8080

int sock;

// Cleanup function to close the socket and handle SIGINT gracefully
void cleanup(int sig) {
    printf("\nClosing connection...\n");
    close(sock);
    exit(0);
}

int main() {
    struct sockaddr_in server_addr;
    char buffer[BUFFER_SIZE] = {0};
    char message[BUFFER_SIZE] = {0};
    fd_set read_fds;
    int max_fd;

    // Signal handling for Ctrl+C
    signal(SIGINT, cleanup);

    // Create socket
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation error");
        return -1;
    }

    // Configure server address
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);

    // Set the server IP address (change "127.0.0.1" to your server's IP if needed)
if (inet_pton(AF_INET, "172.22.85.245", &server_addr.sin_addr) <= 0) {
        perror("Invalid address/Address not supported");
        return -1;
    }

    // Connect to the server
    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connection failed");
        return -1;
    }

    // Receive username prompt from the server
    memset(buffer, 0, BUFFER_SIZE);
    int prompt_len = recv(sock, buffer, BUFFER_SIZE - 1, 0);
    if (prompt_len <= 0) {
        perror("Failed to receive username prompt");
        close(sock);
        return -1;
    }
    buffer[prompt_len] = '\0';
    printf("%s", buffer);

    // Send username
    if (fgets(message, BUFFER_SIZE, stdin) == NULL) {
        perror("Failed to read username");
        close(sock);
        return -1;
    }
    if (send(sock, message, strlen(message), 0) < 0) {
        perror("Failed to send username");
        close(sock);
        return -1;
    }

    printf("Connected to chat server. Type '/exit' to quit.\n");

    // Main communication loop
    while (1) {
        FD_ZERO(&read_fds);
        FD_SET(STDIN_FILENO, &read_fds); // Monitor user input
        FD_SET(sock, &read_fds);        // Monitor server messages
        max_fd = sock;

        // Wait for activity on either input or socket
        int activity = select(max_fd + 1, &read_fds, NULL, NULL, NULL);

        if (activity < 0 && errno != EINTR) {
            perror("Select error");
            break;
        }

        // Handle incoming messages from the server
        if (FD_ISSET(sock, &read_fds)) {
            memset(buffer, 0, BUFFER_SIZE);
            int valread = recv(sock, buffer, BUFFER_SIZE - 1, 0);
            if (valread <= 0) {
                printf("Server disconnected.\n");
                break;
            }
            buffer[valread] = '\0';
            printf("%s\n", buffer);
        }

        // Handle user input
        if (FD_ISSET(STDIN_FILENO, &read_fds)) {
            memset(message, 0, BUFFER_SIZE);
            if (fgets(message, BUFFER_SIZE, stdin) == NULL) {
                break;
            }

            // Remove newline character
            message[strcspn(message, "\n")] = 0;

            // Handle exit command
            if (strcmp(message, "/exit") == 0) {
                send(sock, message, strlen(message), 0);
                break;
            }

            // Send the message to the server
            if (send(sock, message, strlen(message), 0) < 0) {
                perror("Send failed");
                break;
            }
        }
    }

    cleanup(0);
    return 0;
}