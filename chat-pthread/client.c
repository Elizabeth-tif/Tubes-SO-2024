#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <errno.h>

#define BUFFER_SIZE 1024

int main() {
    int sock;
    struct sockaddr_in server_addr;
    char buffer[BUFFER_SIZE] = {0};
    char message[BUFFER_SIZE] = {0};
    fd_set read_fds;
    int max_fd;

    // Create socket
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation error");
        return -1;
    }

    // Configure server
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(8080);

    // IMPORTANT: Replace with ACTUAL IP of your server VM
    if (inet_pton(AF_INET, "172.22.85.245", &server_addr.sin_addr) <= 0) {
        perror("Invalid address/Address not supported");
        return -1;
    }

    // Connect to server
    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connection failed");
        return -1;
    }

    printf("Terhubung ke server chat. Ketik '/exit' untuk keluar.\n");

    // Clear buffer before receiving
    memset(buffer, 0, BUFFER_SIZE);

    // Receive username prompt
    int prompt_len = recv(sock, buffer, BUFFER_SIZE - 1, 0);
    if (prompt_len <= 0) {
        perror("Failed to receive username prompt");
        close(sock);
        return -1;
    }
    buffer[prompt_len] = '\0';
    printf("%s", buffer);

    // Input username
    fgets(message, BUFFER_SIZE, stdin);
    if (send(sock, message, strlen(message), 0) < 0) {
        perror("Failed to send username");
        close(sock);
        return -1;
    }

    // Main chat loop with select()
    while (1) {
        // Reset file descriptor set
        FD_ZERO(&read_fds);
        FD_SET(STDIN_FILENO, &read_fds);
        FD_SET(sock, &read_fds);
        max_fd = sock + 1;

        // Wait for activity on stdin or socket
        int activity = select(max_fd, &read_fds, NULL, NULL, NULL);
        if (activity < 0) {
            perror("select error");
            break;
        }

        // Check if socket has incoming message
        if (FD_ISSET(sock, &read_fds)) {
            memset(buffer, 0, BUFFER_SIZE);
            int valread = recv(sock, buffer, BUFFER_SIZE - 1, 0);

            if (valread <= 0) {
                // Server disconnected
                printf("Server disconnected.\n");
                break;
            }

            buffer[valread] = '\0';
            printf("%s\n", buffer);
        }

        // Check if user has input
        if (FD_ISSET(STDIN_FILENO, &read_fds)) {
            printf("> ");
            memset(message, 0, BUFFER_SIZE);

            if (fgets(message, BUFFER_SIZE, stdin) == NULL) {
                break;
            }

            // Remove newline
            message[strcspn(message, "\n")] = 0;

            // Check for exit
            if (strcmp(message, "/exit") == 0) {
                break;
            }

            // Send message
            if (send(sock, message, strlen(message), 0) < 0) {
                perror("Send failed");
                break;
            }
        }
    }

    // Close the connection
    close(sock);
    printf("Koneksi ditutup.\n");
    return 0;
}