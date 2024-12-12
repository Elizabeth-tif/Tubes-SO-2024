#include <stdio.h> // Library untuk fungsi input/output standar seperti printf, scanf, dan lainnya.
#include <stdlib.h> // Library untuk fungsi-fungsi utilitas seperti alokasi memori, konversi angka, dan keluar dari program dengan exit.
#include <string.h> // Library untuk manipulasi string seperti strlen, strcpy, strncpy, dan lainnya.
#include <unistd.h> // Library untuk fungsi POSIX seperti close, fork, read, write, dan sleep.
#include <arpa/inet.h> // Library untuk fungsi jaringan seperti inet_pton (konversi IP), htons (konversi byte order), dan struktur sockaddr_in.
#include <sys/socket.h> // Library untuk operasi socket seperti socket(), bind(), listen(), accept(), send(), dan recv().
#include <sys/wait.h> // Library untuk menunggu terminasi proses anak menggunakan fungsi seperti wait() dan waitpid().
#include <errno.h>// Library untuk mendeteksi dan menangani kode error sistem menggunakan makro seperti EINTR, EAGAIN.
#include <time.h> // Library untuk menangani waktu seperti time(), clock(), dan manipulasi waktu lainnya.

// inisialisasi 
#define MAX_CLIENTS 100 // Maksimum jumlah klien yang dapat terhubung secara bersamaan
#define BUFFER_SIZE 1024 // Ukuran buffer untuk pesan
#define PORT 8080 // Port yang digunakan oleh server
#define USERNAME_LENGTH 50 // Panjang maksimum username klien
#define CLIENT_ID_LENGTH 10  // Panjang maksimum ID klien

// Struct ClientInfo yang digunakan untuk menyimpan informasi klien
typedef struct {
    int socket;  // File descriptor socket klien
    char username[USERNAME_LENGTH]; // Username klien
    char client_id[CLIENT_ID_LENGTH]; // ID unik klien
    pid_t pid; // Proses ID untuk klien
} ClientInfo;

ClientInfo clients[MAX_CLIENTS]; // Array untuk menyimpan informasi semua klien
int client_count = 0; // Hitungan klien yang sedang terhubung
int server_fd;                   // File descriptor server socket

// Fungsi untuk menghasilkan ID klien yang unik
void generate_client_id(char *client_id) {
    static int counter = 0; //inisialisasi counter untuk melacak jumlah klien
    counter++;
    snprintf(client_id, CLIENT_ID_LENGTH, "Client%d", counter);
}

// Fungsi untuk menghapus karakter newline dari string
void trim_newline(char *str) {
    size_t length = strlen(str);
    if (length > 0 && str[length - 1] == '\n') { // Memeriksa apakah string memiliki panjang > 0 dan karakter terakhir adalah newline
        str[length - 1] = '\0'; // Mengganti newline dengan null-terminator
    }
}

// Fungsi untuk mengirimkan pesan ke semua klien kecuali pengirimnya.
void safe_broadcast(char *message, const char *sender_client_id) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].socket > 0 &&                                // Memeriksa apakah socket klien valid (client aktif)
            strcmp(clients[i].client_id, sender_client_id) != 0) {  // dan ID klien bukan pengirim
            send(clients[i].socket, message, strlen(message), 0); 
        }
    }
}

// Fungsi untuk menangani penghentian proses anak (klien yang terputus).
void sigchld_handler(int sig) {
    int status;
    pid_t pid;

     // Loop untuk menangani semua proses anak yang telah selesai.
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        for (int i = 0; i < MAX_CLIENTS; i++) { // Cari proses anak dalam daftar klien.
            if (clients[i].pid == pid) { // Memeriksa apakah Proses dengan pid yang sedang dicek cocok dengan PID klien di array
                printf("Client %s (%s) disconnected\n",
                       clients[i].username,
                       clients[i].client_id);

                close(clients[i].socket);  // Tutup socket klien.
                clients[i].socket = 0;  // Reset data klien di array.
                clients[i].pid = 0;
                memset(clients[i].username, 0, sizeof(clients[i].username));
                memset(clients[i].client_id, 0, sizeof(clients[i].client_id));
                client_count--; // Kurangi jumlah klien aktif.
                break;
            }
        }
    }
}

// Fungsi untuk memformat pesan dari klien dengan panjang yang aman sebelum dikirim ke semua klien.
void safe_format_message(char *full_message, size_t full_message_size,
                         const char *username, const char *client_id,
                         const char *buffer) {
    char safe_username[USERNAME_LENGTH]; // Buffer aman untuk username.
    strncpy(safe_username, username, sizeof(safe_username) - 1); // Menyalin nilai dari string username ke dalam array safe_username
    safe_username[sizeof(safe_username) - 1] = '\0'; 

    char safe_buffer[BUFFER_SIZE];
    strncpy(safe_buffer, buffer, sizeof(safe_buffer) - 1); // Menyalin pesan dari buffer ke dalam array safe_buffrer
    safe_buffer[sizeof(safe_buffer) - 1] = '\0';

    snprintf(full_message, full_message_size, "[%s] %s: %s",
             client_id, safe_username, safe_buffer);
}

// Fungsi untuk menangani komunikasi dengan klien individu
void handle_client(int client_socket, char *client_id) {
    char buffer[BUFFER_SIZE] = {0}; // Buffer untuk menerima pesan dari klien.
    char username[USERNAME_LENGTH] = {0}; // Buffer untuk menyimpan username klien.
    char full_message[BUFFER_SIZE] = {0}; // Buffer untuk pesan lengkap yang akan dikirim.

    const char *prompt = "Enter your username: "; // Pesan prompt untuk meminta username.
    send(client_socket, prompt, strlen(prompt), 0);  // Mengirimkan prompt ke klien.

    ssize_t username_length = recv(client_socket, username, sizeof(username) - 1, 0); // Menerima username dari klien.
    if (username_length <= 0) {
        close(client_socket); // Jika gagal menerima, tutup socket klien.
        exit(EXIT_FAILURE);
    }
    username[username_length] = '\0'; // Menambahkan null-terminator pada akhir username.
    trim_newline(username); // Menghapus newline jika ada

    // Mengirimkan pesan bergabung untuk klien baru.
    snprintf(full_message, sizeof(full_message),
             "Server: [%s] %s joined the chat", client_id, username);
    safe_broadcast(full_message, client_id);  // Menyebarkan pesan ke semua klien.
    printf("%s\n", full_message);// Menampilkan pesan bergabung di server.

    while (1) {
        memset(buffer, 0, sizeof(buffer)); // Mengosongkan buffer.

        ssize_t valread = recv(client_socket, buffer, sizeof(buffer) - 1, 0); // Menerima pesan dari klien.
        if (valread <= 0) break;  // Memeriksa apakah ada data yang diterima. Jika tidak ada data, keluar dari loop.

        buffer[valread] = '\0';
        trim_newline(buffer);

        if (strcmp(buffer, "/exit") == 0) break; // Memeriksa apakah klien mengirim /exit. Jika iya, keluar dari loop.

        // Memformat pesan dengan ID klien.
        safe_format_message(full_message, sizeof(full_message),
                            username, client_id, buffer);
        safe_broadcast(full_message, client_id); // Menyebarkan pesan ke semua klien.
        printf("%s\n", full_message); // Menampilkan pesan di server.
    } 

    // Mengirimkan pesan keluar setelah klien menutup koneksi.
    snprintf(full_message, sizeof(full_message),
             "Server: [%s] %s left the chat", client_id, username);
    safe_broadcast(full_message, client_id);
    printf("%s\n", full_message);

    close(client_socket); // Menutup koneksi klien.
    exit(EXIT_SUCCESS); // Keluar dari proses anak.
}

int main() {
    struct sockaddr_in address;  // Struct untuk alamat socket.
    int addrlen = sizeof(address);  // Mendapatkan size dari struct sockaddr_in.

    memset(clients, 0, sizeof(clients)); // Menginisialisasi array client dengan 0.

    //Memeriksa apakah socket berhasil dibuat dengan menggunakan socket()
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) { 
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    int opt = 1;
    // Memeriksa apakah pengaturan opsi socket SO_REUSEADDR berhasil dilakukan dengan menggunakan fungsi setsockopt()
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    // Memeriksa apakah socket berhasil diikat (bind) ke alamat dan port yang telah ditentukan.
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    // Memeriksa apakah server berhasil mendengarkan koneksi masuk dengan listen()
    if (listen(server_fd, MAX_CLIENTS) < 0) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    printf("Chat server running on port %d...\n", PORT);

    signal(SIGCHLD, sigchld_handler);

    while (1) {
        // Memeriksa apakah server berhasil menerima koneksi masuk dari klien dengan accept()
        int new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen);
        if (new_socket < 0) {
            perror("Accept failed");
            continue;
        }

        // Memeriksa apakah jumlah klien yang terhubung telah mencapai batas maksimum (MAX_CLIENTS)
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

        // Memeriksa apakah proses fork berhasil
        if (pid < 0) {
            perror("Fork failed"); 
            close(new_socket);
        } else if (pid == 0) { // Memeriksa apakah proses yang sedang berjalan adalah proses anak
            close(server_fd); // Menutup socket server di proses anak (karena hanya proses anak yang berkomunikasi dengan klien).
            handle_client(new_socket, client_id); // Memanggil fungsi handle_client() untuk menangani klien.
        } else { // pid > 0, berarti berada di dalam proses induk yang menangani manajemen klien.
            sigset_t sigset; // deklarasi set sinyal.
            sigemptyset(&sigset); //inisialisasi set sinyal kosong.
            sigaddset(&sigset, SIGCHLD); //menambahkan sinyal SIGCHLD ke dalam set.
            sigprocmask(SIG_BLOCK, &sigset, NULL); // digunakan untuk memblokir sinyal SIGCHLD sementara

            /* loop ini berguna untuk proses induk mencari tempat yang kosong dalam array clients[]. 
            Jika ditemukan yang socketnya kosong, maka data klien baru akan disimpan di sana*/
            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (clients[i].socket == 0) {
                    clients[i].socket = new_socket;
                    clients[i].pid = pid;
                    strncpy(clients[i].client_id, client_id, CLIENT_ID_LENGTH - 1);
                    client_count++;
                    break;
                }
            }

            sigprocmask(SIG_UNBLOCK, &sigset, NULL); //Membuka kembali sinyal SIGCHLD, yang memungkinkan proses induk untuk menerima sinyal yang memberitahukan bahwa proses anak telah selesai.
        }
    }

    close(server_fd);
    return 0;
}