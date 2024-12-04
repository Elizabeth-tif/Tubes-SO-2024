#include <stdio.h>
#include <sys/wait.h>
#include <signal.h>
#include "server.h"

// Signal handler untuk membersihkan proses anak
void sigchld_handler(int signo) {
    while (waitpid(-1, NULL, WNOHANG) > 0) {
        printf("Cleaned up child process.\n");
    }
}
