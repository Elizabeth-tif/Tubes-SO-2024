#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "server.h"

// Fungsi untuk menghapus karakter newline (\n atau \r)
void trim_newline(char *str) {
    size_t len = strlen(str);
    if (len > 0 && (str[len - 1] == '\n' || str[len - 1] == '\r')) {
        str[len - 1] = '\0';
    }
}

// Fungsi untuk menangani koneksi klien
void handle_client(int client_socket, int client_id) {
    char buffer[1024];
    int valread;

    printf("Handling Client %d\n", client_id);

    while (1) {
        // Membaca pesan dari klien
        valread = read(client_socket, buffer, sizeof(buffer) - 1);

        if (valread > 0) {
            buffer[valread] = '\0'; // Null-terminate pesan
            trim_newline(buffer);  // Hapus karakter newline

            printf("Client %d: %s\n", client_id, buffer);

            // Periksa apakah klien mengirim "exit"
            if (strcmp(buffer, "exit") == 0) {
                printf("Client %d requested to disconnect.\n", client_id);
                break; // Keluar dari loop
            }

            // Kirim kembali pesan (Echo)
            send(client_socket, buffer, strlen(buffer), 0);
        } else if (valread == 0) {
            // Klien telah menutup koneksi
            printf("Client %d disconnected.\n", client_id);
            break;
        } else {
            // Terjadi error saat membaca
            perror("Error reading from client");
            break;
        }
    }

    close(client_socket); // Tutup koneksi klien
}
