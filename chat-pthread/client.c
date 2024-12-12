#include <stdio.h>// Library untuk fungsi input/output standar
#include <stdlib.h> // Library untuk fungsi standar seperti malloc, free, dan exit
#include <string.h> // Library untuk fungsi manipulasi string
#include <unistd.h> // Library untuk fungsi POSIX seperti close dan read
#include <arpa/inet.h> // Library untuk fungsi jaringan seperti inet_pton
#include <sys/select.h> // Library untuk fungsi select untuk multiplexer I/O
#include <errno.h> // Library untuk menangani error dengan errno

#define BUFFER_SIZE 1024 // Ukuran buffer untuk pesan

int main() {
    int sock; // File descriptor untuk socket
    struct sockaddr_in server_addr; // Struct untuk menyimpan alamat server
    char buffer[BUFFER_SIZE] = {0};  // Buffer untuk menerima data dari server
    char message[BUFFER_SIZE] = {0}; // Buffer untuk mengirim data ke server
    fd_set read_fds; // Set file descriptor untuk select
    int max_fd; // Nilai maksimum file descriptor

    // Membuat socket
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) { // Memeriksa apakah socket berhasil dibuat
        perror("Socket creation error"); // Tampilkan pesan error jika gagal
        return -1;
    }

    // Konfigurasi alamat server
    server_addr.sin_family = AF_INET; // Menggunakan IPv4
    server_addr.sin_port = htons(8080); // Port server, diubah ke format network byte order

    // Mengatur alamat IP server
    if (inet_pton(AF_INET, "172.22.85.245", &server_addr.sin_addr) <= 0) {  // Memeriksa apakah alamat IP valid
        perror("Invalid address/Address not supported");
        return -1;
    }

    // Menghubungkan socket ke server
    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {  // Memeriksa apakah koneksi ke server berhasil
        perror("Connection failed");
        return -1;
    }

    printf("Terhubung ke server chat. Ketik '/exit' untuk keluar.\n");

    // Membersihkan buffer sebelum menerima pesan
    memset(buffer, 0, BUFFER_SIZE);

    //  Menerima prompt username dari server
    int prompt_len = recv(sock, buffer, BUFFER_SIZE - 1, 0);
    if (prompt_len <= 0) { // Memeriksa apakah pesan prompt diterima
        perror("Failed to receive username prompt"); 
        close(sock); // Tutup socket
        return -1;
    }
    buffer[prompt_len] = '\0'; // Tambahkan null-terminator
    printf("%s", buffer); // Menampilkan prompt username

    // Input username dari pengguna
    fgets(message, BUFFER_SIZE, stdin); // Membaca input dari stdin
    if (send(sock, message, strlen(message), 0) < 0) { // Memeriksa apakah username berhasil dikirim
        perror("Failed to send username");
        close(sock);
        return -1;
    }

     // Loop utama untuk chat menggunakan select()
    while (1) {
        // Reset file descriptor set
        FD_ZERO(&read_fds); // Bersihkan semua bit dalam set
        FD_SET(STDIN_FILENO, &read_fds); // Tambahkan stdin ke set
        FD_SET(sock, &read_fds); // Tambahkan socket ke set
        max_fd = sock + 1; // Set nilai maksimum file descriptor

        // Tunggu aktivitas di stdin atau socket
        int activity = select(max_fd, &read_fds, NULL, NULL, NULL);
        if (activity < 0) { // Memeriksa apakah fungsi select berhasil
            perror("select error"); 
            break;
        }

        // Periksa apakah ada pesan masuk dari socket
        if (FD_ISSET(sock, &read_fds)) { // Memeriksa apakah socket memiliki data yang masuk
            memset(buffer, 0, BUFFER_SIZE); // Bersihkan buffer
            int valread = recv(sock, buffer, BUFFER_SIZE - 1, 0); // Terima pesan

            if (valread <= 0) { // Memeriksa apakah server terputus
                printf("Server disconnected.\n");
                break;
            }

            buffer[valread] = '\0'; // Tambahkan null-terminator
            printf("%s\n", buffer); // Tampilkan pesan dari server
        }

        // Periksa apakah ada input dari pengguna
        if (FD_ISSET(STDIN_FILENO, &read_fds)) { // Memeriksa apakah pengguna memberikan input
            printf("> "); // Tampilkan prompt
            memset(message, 0, BUFFER_SIZE); // Bersihkan buffer

            if (fgets(message, BUFFER_SIZE, stdin) == NULL) { // Memeriksa apakah input pengguna valid
                break; // Keluar jika EOF
            }

            // Hapus karakter newline
            message[strcspn(message, "\n")] = 0;

            // Periksa apakah pengguna ingin keluar
            if (strcmp(message, "/exit") == 0) { // Memeriksa apakah pengguna mengetik '/exit'
                break;
            }

            // Kirim pesan ke server
            if (send(sock, message, strlen(message), 0) < 0) { // Memeriksa apakah pesan berhasil dikirim
                perror("Send failed"); // Error jika gagal mengirim
                break;
            }
        }
    }

    // Tutup koneksi
    close(sock);
    printf("Koneksi ditutup.\n");
    return 0;
}