#include <stdio.h> // Library untuk fungsi input/output standar seperti printf dan fgets.
#include <stdlib.h>  // Library untuk fungsi umum seperti malloc, free, dan exit.
#include <string.h>  // Library untuk manipulasi string seperti memset, strlen, dan strcmp.
#include <unistd.h> // Library untuk fungsi POSIX seperti close dan read.
#include <arpa/inet.h>  // Library untuk fungsi jaringan seperti inet_pton dan socket.
#include <sys/socket.h> // Library untuk fungsi pembuatan socket.
#include <sys/select.h> // Library untuk fungsi select, digunakan untuk multiplexing.
#include <signal.h>  // Library untuk menangani sinyal seperti SIGINT.
#include <errno.h> // Library untuk menangani kesalahan sistem seperti EINTR

// Inisialisasi 
#define BUFFER_SIZE 1024 // Ukuran buffer untuk pengiriman/pengecekan pesan.
#define PORT 8080 // Port server yang akan digunakan.

// deklarasi variabel global untuk menyimpan deskriptor socket.
int sock;

/*  Fungsi cleanup untuk menutup socket dan menangani SIGINT(ctrl+c) dengan rapi
    agar program tidak meninggalkan koneksi terbuka jika pengguna keluar secara paksa.
*/
void cleanup(int sig) {
    printf("\nClosing connection...\n");
    close(sock); // Menutup socket.
    exit(0);  // Keluar dari program.
}

int main() {
    struct sockaddr_in server_addr; // Struct untuk menyimpan informasi server (alamat dan port).
    char buffer[BUFFER_SIZE] = {0}; // Inisialisasi buffer untuk menerima data dari server.
    char message[BUFFER_SIZE] = {0}; // Inisialisasi buffer untuk mengirim data ke server.
    fd_set read_fds;  // Set file descriptor untuk fungsi select.
    int max_fd;  // Variabel untuk menyimpan deskriptor socket maksimum.

    // Menghubungkan sinyal Ctrl+C (SIGINT) ke fungsi cleanup.
    signal(SIGINT, cleanup); 

    // Membuat socket
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) { // Memeriksa apakah socket berhasil dibuat.
        perror("Socket creation error");
        return -1; 
    }

     // Konfigurasi alamat server
    server_addr.sin_family = AF_INET;  // Menentukan keluarga alamat (IPv4).
    server_addr.sin_port = htons(PORT); // Konversi port ke format network byte order.

    // Mengatur alamat IP server ke ip yang diinginkan
    if (inet_pton(AF_INET, "192.168.34.52", &server_addr.sin_addr) <= 0) { // Memeriksa apakah alamat IP valid.
        perror("Invalid address/Address not supported");
        return -1;
    }

    // Menghubungkan socket ke server
    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) { // Memeriksa apakah koneksi berhasil.
        perror("Connection failed");
        return -1;
    }

    // Menerima prompt username dari server
    memset(buffer, 0, BUFFER_SIZE); // Mengosongkan buffer.
    int prompt_len = recv(sock, buffer, BUFFER_SIZE - 1, 0); // Menerima data dari server.
    if (prompt_len <= 0) {  // Memeriksa apakah ada data yang diterima. 
        perror("Failed to receive username prompt"); // Jika gagal menerima pesan akan mengirimkan pesan error
        close(sock);
        return -1;
    }
    buffer[prompt_len] = '\0'; // Menambahkan null-terminator pada akhir string.
    printf("%s", buffer);

     // Mengirimkan username ke server
    if (fgets(message, BUFFER_SIZE, stdin) == NULL) { // memeriksa apakah input dari pengguna berhasil dibaca.
        perror("Failed to read username");
        close(sock);
        return -1;
    }
    if (send(sock, message, strlen(message), 0) < 0) { // Memeriksa apakah pengiriman username ke server berhasil.
        perror("Failed to send username");
        close(sock);
        return -1;
    }

    printf("Connected to chat server. Type '/exit' to quit.\n");

     // Loop utama untuk komunikasi
    while (1) {
        FD_ZERO(&read_fds); // Mengosongkan set file descriptor.
        FD_SET(STDIN_FILENO, &read_fds); // Menambahkan input standar (keyboard) ke set.
        FD_SET(sock, &read_fds);         // Menambahkan socket ke set.
        max_fd = sock; // Menentukan deskriptor socket maksimum.

        // Menunggu aktivitas pada input atau socket
        // Menggunakan select untuk multiplexing.
        int activity = select(max_fd + 1, &read_fds, NULL, NULL, NULL); 
        
        // Memeriksa error selain interupsi.
        if (activity < 0 && errno != EINTR) {
            perror("Select error");
            break;
        }

        // Menangani pesan masuk dari server
        if (FD_ISSET(sock, &read_fds)) {   // Memeriksa apakah socket siap untuk dibaca.
            memset(buffer, 0, BUFFER_SIZE);
            int valread = recv(sock, buffer, BUFFER_SIZE - 1, 0); // Membaca data dari server.
            if (valread <= 0) { // Memeriksa apakah ada data atau koneksi terputus.
                printf("Server disconnected.\n");
                break;
            }
            buffer[valread] = '\0';
            printf("%s\n", buffer);
        }

        // Menangani input dari pengguna
        if (FD_ISSET(STDIN_FILENO, &read_fds)) {  // Memeriksa apakah ada input dari keyboard.
            memset(message, 0, BUFFER_SIZE);
            if (fgets(message, BUFFER_SIZE, stdin) == NULL) { // memeriksa apakah input dari pengguna berhasil dibaca.
                break;
            }

            // Menghapus karakter newline
            message[strcspn(message, "\n")] = 0;

             // Menangani perintah keluar
            if (strcmp(message, "/exit") == 0) { // Memeriksa apakah perintah adalah '/exit'.
                send(sock, message, strlen(message), 0); // Mengirim perintah ke server.
                break; // Keluar dari loop utama.
            }

            // Mengirimkan pesan ke server
            if (send(sock, message, strlen(message), 0) < 0) { // Memeriksa apakah pengiriman pesan berhasil.
                perror("Send failed");
                break;
            }
        }
    }

    cleanup(0);
    return 0;
}