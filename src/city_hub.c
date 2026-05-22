/* ===========================================================
 * city_hub.c - Phase 3
 * Interfata interactiva linie de comanda pentru sistemul de raportare.
 *
 * Student: Cadar Minodora
 *
 * Comenzi suportate:
 *   start_monitor              - porneste monitorul (in etapa urmatoare)
 *   calculate_scores <d1> <d2> - calculeaza scoruri de workload
 *   help                       - afiseaza comenzile
 *   exit                       - iese din city_hub
 * =========================================================== */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/stat.h>

#define MAX_DISTRICTS 16
#define BUF_SIZE 4096

int district_exists(const char *district) {
    struct stat st;
    if (stat(district, &st) < 0) return 0;
    return S_ISDIR(st.st_mode);
}

int run_scorer_for_district(const char *district, char *out_buf, size_t buf_size) {
    int pipe_fd[2];

    if (pipe(pipe_fd) < 0) {
        perror("pipe");
        return -1;
    }

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        close(pipe_fd[0]);
        close(pipe_fd[1]);
        return -1;
    }

    if (pid == 0) {
        /* COPIL */
        close(pipe_fd[0]);
        if (dup2(pipe_fd[1], STDOUT_FILENO) < 0) {
            perror("dup2");
            _exit(EXIT_FAILURE);
        }
        close(pipe_fd[1]);
        execlp("./scorer", "scorer", district, (char *)NULL);
        perror("execlp scorer");
        _exit(EXIT_FAILURE);
    }

    /* PARINTE */
    close(pipe_fd[1]);

    size_t total = 0;
    ssize_t n;
    while (total < buf_size - 1 &&
           (n = read(pipe_fd[0], out_buf + total, buf_size - 1 - total)) > 0) {
        total += n;
    }
    out_buf[total] = '\0';
    close(pipe_fd[0]);

    int status;
    waitpid(pid, &status, 0);
    return 0;
}

void cmd_calculate_scores(char *districts[], int count) {
    if (count == 0) {
        printf("Utilizare: calculate_scores <district1> [district2] ...\n");
        return;
    }

    printf("\n========== RAPORT COMBINAT WORKLOAD ==========\n");

    for (int i = 0; i < count; i++) {
        if (!district_exists(districts[i])) {
            printf("\n[!] Districtul '%s' nu exista - sarit.\n", districts[i]);
            continue;
        }

        char output[BUF_SIZE];
        if (run_scorer_for_district(districts[i], output, sizeof(output)) == 0) {
            printf("\n%s", output);
        } else {
            printf("\n[!] Eroare la rularea scorer pentru '%s'\n", districts[i]);
        }
    }

    printf("\n==============================================\n\n");
}

void print_help(void) {
    printf("Comenzi disponibile:\n");
    printf("  start_monitor                    - porneste monitorul de rapoarte\n");
    printf("  calculate_scores <d1> [d2] ...   - calculeaza scoruri workload\n");
    printf("  help                             - afiseaza acest mesaj\n");
    printf("  exit                             - iese din city_hub\n");
}

int main(void) {
    char line[512];

    printf("=== city_hub - interfata interactiva (Phase 3) ===\n");
    printf("Scrie 'help' pentru lista de comenzi.\n\n");

    while (1) {
        printf("city_hub> ");
        fflush(stdout);

        if (fgets(line, sizeof(line), stdin) == NULL) {
            printf("\n");
            break;
        }

        line[strcspn(line, "\n")] = '\0';

        char *tokens[MAX_DISTRICTS + 2];
        int ntokens = 0;
        char *tok = strtok(line, " \t");
        while (tok != NULL && ntokens < MAX_DISTRICTS + 1) {
            tokens[ntokens++] = tok;
            tok = strtok(NULL, " \t");
        }

        if (ntokens == 0) continue;

        if (strcmp(tokens[0], "exit") == 0) {
            printf("La revedere!\n");
            break;
        }
        else if (strcmp(tokens[0], "help") == 0) {
            print_help();
        }
        else if (strcmp(tokens[0], "calculate_scores") == 0) {
            cmd_calculate_scores(&tokens[1], ntokens - 1);
        }
        else if (strcmp(tokens[0], "start_monitor") == 0) {
            printf("start_monitor va fi implementat in etapa urmatoare.\n");
        }
        else {
            printf("Comanda necunoscuta: '%s'. Scrie 'help'.\n", tokens[0]);
        }
    }

    return 0;
}
