#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <signal.h>
#include <pthread.h>
#include "server.h"

#define PORT 8080

// Counter global untuk memberi ID unik kepada klien
int client_counter = 0;
pthread_mutex_t counter_mutex = PTHREAD_MUTEX_INITIALIZER; // Mutex untuk melindungi counter klien

// Fungsi utama server
int main() {
    int server_fd, new_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);

    // Signal handler untuk menangani proses anak
    signal(SIGCHLD, sigchld_handler);

    // Membuat socket server
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Mengatur opsi socket
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        perror("Setsockopt failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // Menentukan alamat dan port
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY; // Mendengarkan di semua antarmuka
    address.sin_port = htons(PORT);

    // Mengikat socket ke port
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // Mulai mendengarkan koneksi masuk
    if (listen(server_fd, 3) < 0) {
        perror("Listen failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d...\n", PORT);

    // Loop utama untuk menerima koneksi
    while (1) {
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen)) < 0) {
            perror("Accept failed");
            continue;
        }

        // Menghasilkan ID unik untuk klien
        int client_id;
        pthread_mutex_lock(&counter_mutex);
        client_counter++;
        client_id = client_counter;
        pthread_mutex_unlock(&counter_mutex);

        printf("New client connected: Client %d\n", client_id);

        // Fork untuk menangani klien dalam proses terpisah
        if (fork() == 0) {
            close(server_fd); // Tutup server_fd di proses anak
            handle_client(new_socket, client_id); // Tangani klien
            exit(0); // Proses anak selesai
        }

        close(new_socket); // Proses induk tidak memerlukan socket klien
    }

    return 0;
}
