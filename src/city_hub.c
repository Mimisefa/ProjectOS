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
#include <signal.h>
#include <sys/stat.h>

#define MAX_DISTRICTS 16
#define BUF_SIZE 4096

void hub_mon_main(void);

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

/* ==========================================
 * hub_mon_main - rulat in procesul copil dupa fork() la start_monitor
 *
 * 1. Creeaza un pipe
 * 2. Face fork si in copil lanseaza monitor_reports cu execlp,
 *    redirectand stdout-ul lui catre capatul de scriere al pipe-ului (dup2)
 * 3. In parinte (hub_mon) citeste mesajele structurate din pipe,
 *    linie cu linie, si le afiseaza utilizatorului
 * 4. Cand monitorul se inchide, afiseaza un mesaj specific
 * ========================================== */
void hub_mon_main(void) {
    int pipe_fd[2];

    if (pipe(pipe_fd) < 0) {
        perror("hub_mon: pipe");
        _exit(EXIT_FAILURE);
    }

    pid_t monitor_pid = fork();
    if (monitor_pid < 0) {
        perror("hub_mon: fork");
        _exit(EXIT_FAILURE);
    }

    if (monitor_pid == 0) {
        /* COPIL: devine monitor_reports */
        close(pipe_fd[0]);  /* nu citim */
        if (dup2(pipe_fd[1], STDOUT_FILENO) < 0) {
            perror("hub_mon: dup2");
            _exit(EXIT_FAILURE);
        }
        close(pipe_fd[1]);

        execlp("./monitor_reports", "monitor_reports", (char *)NULL);
        perror("hub_mon: execlp monitor_reports");
        _exit(EXIT_FAILURE);
    }

    /* PARINTE (hub_mon): citeste mesajele din pipe */
    close(pipe_fd[1]);  /* nu scriem */

    char buf[BUF_SIZE];
    char line[BUF_SIZE];
    size_t line_len = 0;

    fprintf(stderr, "\n[hub_mon] monitor pornit (PID %d), citesc mesajele...\n",
            (int)monitor_pid);
    fflush(stderr);

    /* Citim caracter cu caracter pana la newline ca sa formam linii complete */
    ssize_t n;
    while ((n = read(pipe_fd[0], buf, sizeof(buf))) > 0) {
        for (ssize_t i = 0; i < n; i++) {
            if (buf[i] == '\n' || line_len >= sizeof(line) - 1) {
                line[line_len] = '\0';
                /* Avem o linie completa - afisam */
                fprintf(stderr, "[hub_mon] %s\n", line);
                fflush(stderr);

                /* Daca e mesaj de ERROR sau EXIT, monitorul s-a terminat */
                if (strstr(line, "[ERROR]") != NULL) {
                    fprintf(stderr,
                        "[hub_mon] monitorul a raportat eroare; ies.\n");
                    fflush(stderr);
                }
                if (strstr(line, "[EXIT]") != NULL) {
                    fprintf(stderr,
                        "[hub_mon] monitorul s-a inchis curat.\n");
                    fflush(stderr);
                }

                line_len = 0;
            } else {
                line[line_len++] = buf[i];
            }
        }
    }

    /* Pipe inchis = monitorul s-a terminat */
    close(pipe_fd[0]);

    /* Asteptam terminarea monitorului */
    int status;
    waitpid(monitor_pid, &status, 0);

    fprintf(stderr,
        "[hub_mon] monitor terminat (exit status %d). hub_mon iese.\n",
        WEXITSTATUS(status));
    fflush(stderr);

    _exit(EXIT_SUCCESS);
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
            pid_t hub_pid = fork();
            if (hub_pid < 0) { perror("fork"); continue; }
            if (hub_pid == 0) { hub_mon_main(); _exit(0); }
            printf("hub_mon pornit (PID %d). Mesajele monitorului apar mai jos.\n", (int)hub_pid);
        }
        else {
            printf("Comanda necunoscuta: '%s'. Scrie 'help'.\n", tokens[0]);
        }
    }

    return 0;
}
