#include <stdio.h>          // Library untuk fungsi input-output
#include <stdlib.h>         // Library untuk fungsi umum seperti malloc, free, dan exit
#include <string.h>         // Library untuk manipulasi string
#include <unistd.h>         // Library untuk fungsi POSIX seperti close dan read
#include <pthread.h>        // Library untuk multithreading
#include <sys/socket.h>     // Library untuk fungsi socket
#include <arpa/inet.h>      // Library untuk fungsi dan struktur jaringan
#include <errno.h>          // Library untuk menangani kesalahan sistem
#include <asm-generic/socket.h> // Library untuk definisi tambahan socket

#define MAX_CLIENTS 100            // Maksimal jumlah klien yang bisa terhubung
#define BUFFER_SIZE 2048           // Ukuran buffer untuk pesan
#define USERNAME_LENGTH 50         // Maksimal panjang username
#define MAX_MESSAGE_LENGTH (BUFFER_SIZE - USERNAME_LENGTH - 10) // Maksimal panjang pesan setelah username

// Struktur untuk informasi klien
typedef struct {
    int socket;                      // Socket untuk komunikasi dengan klien
    int id;                          // ID unik untuk setiap klien
    char username[USERNAME_LENGTH];  // Username klien
    int active;                      // Status apakah klien aktif
} ClientInfo;

// Variabel global
ClientInfo clients[MAX_CLIENTS];     // Array untuk menyimpan informasi semua klien
int client_count = 0;                // Jumlah klien yang terhubung
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER; // Mutex untuk sinkronisasi akses ke array klien

// Fungsi untuk menghapus karakter newline dari string
void trim_newline(char *str) {
    size_t length = strlen(str);     // Mengambil panjang string
    if (length > 0 && str[length - 1] == '\n') { // Memeriksa apakah karakter terakhir adalah newline
        str[length - 1] = '\0';      // Menghapus karakter newline
    }
}

// Fungsi untuk mengirim pesan ke semua klien
void broadcast_message(char *message, int sender_id) {
    if (!message) return;            // Memeriksa apakah pesan tidak null

    pthread_mutex_lock(&clients_mutex); // Mengunci akses ke array klien
    for (int i = 0; i < MAX_CLIENTS; i++) {
        // Mengirim pesan ke semua klien kecuali pengirim
        if (clients[i].active && clients[i].id != sender_id) { // Memeriksa apakah klien aktif dan bukan pengirim
            if (send(clients[i].socket, message, strlen(message), 0) < 0) { // Mengirim pesan
                perror("Failed to broadcast message"); // Menampilkan kesalahan jika gagal
            }
        }
    }
    pthread_mutex_unlock(&clients_mutex); // Membuka akses ke array klien
}

// Fungsi untuk membuat pesan aman dengan mencegah pemotongan
int create_safe_message(char *dest, size_t dest_size, const char *username, const char *message) {
    char safe_username[USERNAME_LENGTH]; // Buffer aman untuk username
    strncpy(safe_username, username, sizeof(safe_username) - 1); // Menyalin username ke buffer aman
    safe_username[sizeof(safe_username) - 1] = '\0';             // Menambahkan null terminator

    char safe_message[BUFFER_SIZE]; // Buffer aman untuk pesan
    strncpy(safe_message, message, sizeof(safe_message) - 1); // Menyalin pesan ke buffer aman
    safe_message[sizeof(safe_message) - 1] = '\0';           // Menambahkan null terminator

    // Membuat pesan dalam format "username: message" secara aman
    return snprintf(dest, dest_size, "%s: %s", safe_username, safe_message);
}

// Fungsi untuk menangani komunikasi dengan klien
void* handle_client(void *arg) {
    int client_socket = *((int*)arg);    // Socket klien
    char buffer[BUFFER_SIZE] = {0};      // Buffer untuk pesan
    char username[USERNAME_LENGTH] = {0}; // Buffer untuk username
    char full_message[BUFFER_SIZE] = {0}; // Buffer untuk pesan lengkap
    int client_id = -1;                  // ID klien

     // Mengirim prompt untuk meminta username
    const char *prompt = "Masukkan username: ";
    if (send(client_socket, prompt, strlen(prompt), 0) < 0) { // Mengirim prompt
        perror("Failed to send username prompt"); // Menampilkan kesalahan jika gagal
        close(client_socket);                     // Menutup socket
        return NULL;
    }

    // Menerima username dari klien
    ssize_t username_length = recv(client_socket, username, sizeof(username) - 1, 0); // Menerima data
    if (username_length <= 0) {                  // Memeriksa apakah username berhasil diterima
        perror("Failed to receive username");    // Menampilkan kesalahan jika gagal
        close(client_socket);                    // Menutup socket
        return NULL;
    }

    username[username_length] = '\0';  // Menambahkan null terminator ke username
    trim_newline(username);            // Menghapus karakter newline

    pthread_mutex_lock(&clients_mutex); // Mengunci akses ke array klien
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (!clients[i].active) {      // Memeriksa slot kosong untuk klien
            clients[i].socket = client_socket;
            clients[i].id = i;
            clients[i].active = 1;
            strncpy(clients[i].username, username, sizeof(clients[i].username) - 1);
            client_count++;           // Menambah jumlah klien
            client_id = i;            // Menyimpan ID klien
            break;
        }
    }
    pthread_mutex_unlock(&clients_mutex); // Membuka akses ke array klien

    // Mengirim pesan untuk memberitahu semuanya bahwa client telah bergabung
    memset(full_message, 0, sizeof(full_message)); 
    create_safe_message(full_message, sizeof(full_message), "Server",
                        username ? username : "Unknown" " bergabung dalam chat");
    broadcast_message(full_message, client_id);

    // Loop utama untuk menangani pesan
    while (1) {
        memset(buffer, 0, sizeof(buffer));  // Mengosongkan buffer
        ssize_t valread = recv(client_socket, buffer, sizeof(buffer) - 1, 0);

        if (valread <= 0) { // Memeriksa apakah klien terputus
            break;
        }

        buffer[valread] = '\0';   // Menambahkan null terminator
        trim_newline(buffer);     // Menghapus karakter newline

        // Memeriksa apakah klien ingin keluar
        if (strcmp(buffer, "/exit") == 0) {
            break;
        }

        memset(full_message, 0, sizeof(full_message)); // Mengosongkan buffer pesan
        create_safe_message(full_message, sizeof(full_message), username, buffer); // Membuat pesan lengkap

        // Mengirim pesan ke semua klien
        broadcast_message(full_message, client_id); 
    }

    // Memberitahukan bahwa klien telah keluar
    memset(full_message, 0, sizeof(full_message));
    create_safe_message(full_message, sizeof(full_message), "Server",
                        username ? username : "Unknown" " keluar dari chat");
    broadcast_message(full_message, client_id);

    // Membersihkan dan menutup koneksi dengan klien
    close(client_socket);

     // Menandai klien sebagai tidak aktif
    pthread_mutex_lock(&clients_mutex);
    clients[client_id].active = 0;
    client_count--;
    pthread_mutex_unlock(&clients_mutex);

    return NULL;
}

int main() {
    int server_fd, new_socket; // Deklarasi file descriptor untuk server dan socket baru
    struct sockaddr_in address; // Struct untuk menyimpan alamat server
    int addrlen = sizeof(address); // Panjang struct alamat

    // Inisialisasi array klien
    memset(clients, 0, sizeof(clients)); // Mengisi array `clients` dengan nilai nol

    // Membuat socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) { // Memeriksa apakah socket berhasil dibuat
        perror("Socket creation failed"); // Pesan kesalahan jika socket gagal dibuat
        exit(EXIT_FAILURE); // Keluar dari program dengan kode kesalahan
    }

    // Mengizinkan penggunaan ulang alamat socket untuk mencegah error "Address already in use"
    int opt = 1; // Opsi reuse address diaktifkan
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) { // Memeriksa apakah opsi berhasil diterapkan
        perror("setsockopt"); // Pesan kesalahan jika gagal
        exit(EXIT_FAILURE); // Keluar dari program dengan kode kesalahan
    }

    // Konfigurasi alamat server
    address.sin_family = AF_INET; // Menggunakan protokol IPv4
    address.sin_addr.s_addr = INADDR_ANY; // Mendengarkan pada semua antarmuka jaringan
    address.sin_port = htons(8080); // Port server yang akan digunakan (8080)

    // Bind socket ke alamat server
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) { // Memeriksa apakah bind berhasil
        perror("Bind failed"); // Pesan kesalahan jika gagal
        exit(EXIT_FAILURE); // Keluar dari program dengan kode kesalahan
    }

    // Memulai mendengarkan koneksi masuk
    if (listen(server_fd, MAX_CLIENTS) < 0) { // Memeriksa apakah mendengarkan berhasil
        perror("Listen failed"); // Pesan kesalahan jika gagal
        exit(EXIT_FAILURE); // Keluar dari program dengan kode kesalahan
    }

    printf("Server chat berjalan di port 8080...\n"); // Pesan bahwa server sudah berjalan

    // Loop utama untuk menerima klien baru
    while (1) {
        // Menerima koneksi klien baru
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) { // Memeriksa apakah koneksi diterima
            perror("Accept failed"); // Pesan kesalahan jika gagal
            continue; // Melanjutkan loop jika terjadi kesalahan
        }

        // Memeriksa apakah jumlah klien maksimum telah tercapai
        if (client_count >= MAX_CLIENTS) { // Jika jumlah klien aktif mencapai batas
            const char *max_clients_msg = "Server penuh. Silakan coba lagi nanti.\n"; // Pesan untuk klien
            send(new_socket, max_clients_msg, strlen(max_clients_msg), 0); // Mengirim pesan kepada klien
            close(new_socket); // Menutup koneksi klien
            continue; // Melanjutkan loop
        }

        // Membuat thread baru untuk setiap klien
        pthread_t thread_id; // Variabel untuk menyimpan ID thread
        if (pthread_create(&thread_id, NULL, handle_client, (void*)&new_socket) != 0) { // Memeriksa apakah thread berhasil dibuat
            perror("Thread creation failed"); // Pesan kesalahan jika gagal
            close(new_socket); // Menutup koneksi klien
            continue; // Melanjutkan loop
        }

        // Melepaskan thread agar sumber daya dikelola secara otomatis
        pthread_detach(thread_id); // Melepaskan thread dari kontrol utama
    }

    // Menutup socket server (baris ini tidak akan pernah tercapai, tetapi praktik yang baik)
    close(server_fd);

    return 0; // Mengembalikan 0 untuk menandakan keberhasilan eksekusi
}
