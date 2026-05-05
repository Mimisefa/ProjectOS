/* ===========================================================
 * monitor_reports.c - Phase 2
 * Proces de monitorizare care raspunde la SIGUSR1 si SIGINT.
 *
 * Student: Cadar Minodora
 *
 * Functionare:
 * - La startup: scrie PID-ul propriu in .monitor_pid (la acelasi nivel
 *   cu directoarele districtelor)
 * - La SIGUSR1: afiseaza un mesaj (raport nou adaugat in sistem)
 * - La SIGINT: afiseaza mesaj, sterge .monitor_pid, iese
 * - Folosim sigaction() (NU signal())
 * =========================================================== */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>

#define PID_FILE ".monitor_pid"

/* Variabile globale modificate din handler-e.
 * volatile sig_atomic_t = sigure pentru semnale. */
static volatile sig_atomic_t should_exit = 0;
static volatile sig_atomic_t got_sigusr1 = 0;

/* ==========================================
 * handle_sigusr1 - apelat cand primim SIGUSR1
 * ========================================== */
void handle_sigusr1(int signo) {
    (void)signo;
    /* In handler-e de semnale folosim DOAR functii async-signal-safe.
     * printf NU este safe - folosim doar variabile sig_atomic_t. */
    got_sigusr1 = 1;
}

/* ==========================================
 * handle_sigint - apelat cand primim SIGINT (Ctrl+C)
 * ========================================== */
void handle_sigint(int signo) {
    (void)signo;
    should_exit = 1;
}

/* ==========================================
 * write_pid_file - scrie PID-ul curent in .monitor_pid
 * ========================================== */
int write_pid_file(void) {
    int fd = open(PID_FILE, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        perror("open .monitor_pid");
        return -1;
    }

    char buf[32];
    int len = snprintf(buf, sizeof(buf), "%d\n", (int)getpid());

    if (write(fd, buf, len) != len) {
        perror("write .monitor_pid");
        close(fd);
        return -1;
    }

    close(fd);
    return 0;
}

/* ==========================================
 * MAIN
 * ========================================== */
int main(void) {
    /* 1. Scriem PID-ul nostru in .monitor_pid */
    if (write_pid_file() < 0) {
        return EXIT_FAILURE;
    }
    printf("monitor_reports: PID %d scris in %s\n", (int)getpid(), PID_FILE);

    /* 2. Instalam handler pentru SIGUSR1 cu sigaction (NU signal!) */
    struct sigaction sa_usr1;
    memset(&sa_usr1, 0, sizeof(sa_usr1));
    sa_usr1.sa_handler = handle_sigusr1;
    sigemptyset(&sa_usr1.sa_mask);
    sa_usr1.sa_flags = SA_RESTART;

    if (sigaction(SIGUSR1, &sa_usr1, NULL) < 0) {
        perror("sigaction SIGUSR1");
        unlink(PID_FILE);
        return EXIT_FAILURE;
    }

    /* 3. Instalam handler pentru SIGINT */
    struct sigaction sa_int;
    memset(&sa_int, 0, sizeof(sa_int));
    sa_int.sa_handler = handle_sigint;
    sigemptyset(&sa_int.sa_mask);
    sa_int.sa_flags = 0;  /* fara SA_RESTART, vrem ca pause() sa iasa la SIGINT */

    if (sigaction(SIGINT, &sa_int, NULL) < 0) {
        perror("sigaction SIGINT");
        unlink(PID_FILE);
        return EXIT_FAILURE;
    }

    printf("monitor_reports: handler-e instalate. Astept semnale...\n");
    printf("                 SIGUSR1 = raport nou, SIGINT (Ctrl+C) = oprire.\n");
    fflush(stdout);

    /* 4. Bucla principala */
    while (!should_exit) {
        pause();  /* asteapta orice semnal, fara consum de CPU */

        if (got_sigusr1) {
            got_sigusr1 = 0;
            printf("[monitor] Notificare SIGUSR1: raport nou adaugat in sistem.\n");
            fflush(stdout);
        }
    }

    /* 5. Cleanup la iesire (SIGINT) */
    printf("\n[monitor] SIGINT primit. Sterg %s si ies.\n", PID_FILE);
    if (unlink(PID_FILE) < 0) {
        perror("unlink .monitor_pid");
    }

    return EXIT_SUCCESS;
}
