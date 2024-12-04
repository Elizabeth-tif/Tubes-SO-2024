#include "server.h"

void sigchld_handler(int signo) {
    while (waitpid(-1, NULL, WNOHANG) > 0); // Bersihkan proses anak yang selesai
    printf("Cleaned up child process.\n");
}