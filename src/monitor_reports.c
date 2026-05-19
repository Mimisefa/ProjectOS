/* ===========================================================
 * monitor_reports.c - Phase 3 (modificat fata de Phase 2)
 *
 * Student: Cadar Minodora
 *
 * Modificari Phase 3:
 * - La pornire verifica daca alt monitor ruleaza deja (prin .monitor_pid
 *   + kill(pid,0)). Daca da, scrie un mesaj [ERROR] si se inchide.
 * - Mesajele scrise la stdout sunt STRUCTURATE cu un prefix de tip:
 *     [INFO]   - mesaj informativ
 *     [REPORT] - raport nou semnalat prin SIGUSR1
 *     [ERROR]  - eroare (ex: alt monitor exista deja)
 *     [EXIT]   - monitorul se inchide
 *   Acest prefix permite lui hub_mon sa distinga tipurile de mesaje.
 *
 * Functionare de baza (din Phase 2):
 * - Scrie PID-ul propriu in .monitor_pid
 * - Raspunde la SIGUSR1 (raport nou)
 * - Se inchide curat la SIGINT, stergand .monitor_pid
 * - Foloseste sigaction() (NU signal())
 * =========================================================== */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>

#define PID_FILE ".monitor_pid"

static volatile sig_atomic_t should_exit = 0;
static volatile sig_atomic_t got_sigusr1 = 0;

void handle_sigusr1(int signo) {
    (void)signo;
    got_sigusr1 = 1;
}

void handle_sigint(int signo) {
    (void)signo;
    should_exit = 1;
}

/* emit_message - scrie un mesaj structurat la stdout: [TYPE] text\n */
void emit_message(const char *type, const char *text) {
    char line[512];
    int len = snprintf(line, sizeof(line), "[%s] %s\n", type, text);
    write(STDOUT_FILENO, line, len);
}

/* check_existing_monitor - verifica daca alt monitor ruleaza deja.
 * Returneaza PID-ul monitorului existent sau 0 daca nu exista. */
int check_existing_monitor(void) {
    int fd = open(PID_FILE, O_RDONLY);
    if (fd < 0) {
        return 0;
    }
    char buf[32];
    ssize_t n = read(fd, buf, sizeof(buf) - 1);
    close(fd);
    if (n <= 0) {
        return 0;
    }
    buf[n] = '\0';
    int pid = atoi(buf);
    if (pid <= 0) {
        return 0;
    }
    /* kill(pid, 0) verifica daca procesul exista FARA sa-i trimita semnal */
    if (kill(pid, 0) == 0) {
        return pid;
    }
    return 0;
}

int write_pid_file(void) {
    int fd = open(PID_FILE, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        return -1;
    }
    char buf[32];
    int len = snprintf(buf, sizeof(buf), "%d\n", (int)getpid());
    if (write(fd, buf, len) != len) {
        close(fd);
        return -1;
    }
    close(fd);
    return 0;
}

int main(void) {
    /* 1. Verificam daca alt monitor ruleaza deja */
    int existing = check_existing_monitor();
    if (existing > 0) {
        char msg[128];
        snprintf(msg, sizeof(msg),
                 "alt monitor ruleaza deja (PID %d); acest monitor se inchide",
                 existing);
        emit_message("ERROR", msg);
        return EXIT_FAILURE;
    }

    /* 2. Scriem PID-ul nostru */
    if (write_pid_file() < 0) {
        emit_message("ERROR", "nu pot scrie .monitor_pid");
        return EXIT_FAILURE;
    }

    char start_msg[128];
    snprintf(start_msg, sizeof(start_msg),
             "monitor pornit (PID %d)", (int)getpid());
    emit_message("INFO", start_msg);

    /* 3. Handler SIGUSR1 */
    struct sigaction sa_usr1;
    memset(&sa_usr1, 0, sizeof(sa_usr1));
    sa_usr1.sa_handler = handle_sigusr1;
    sigemptyset(&sa_usr1.sa_mask);
    sa_usr1.sa_flags = SA_RESTART;
    if (sigaction(SIGUSR1, &sa_usr1, NULL) < 0) {
        emit_message("ERROR", "sigaction SIGUSR1 a esuat");
        unlink(PID_FILE);
        return EXIT_FAILURE;
    }

    /* 4. Handler SIGINT */
    struct sigaction sa_int;
    memset(&sa_int, 0, sizeof(sa_int));
    sa_int.sa_handler = handle_sigint;
    sigemptyset(&sa_int.sa_mask);
    sa_int.sa_flags = 0;
    if (sigaction(SIGINT, &sa_int, NULL) < 0) {
        emit_message("ERROR", "sigaction SIGINT a esuat");
        unlink(PID_FILE);
        return EXIT_FAILURE;
    }

    emit_message("INFO", "astept semnale (SIGUSR1 = raport nou, SIGINT = oprire)");

    /* 5. Bucla principala */
    while (!should_exit) {
        pause();
        if (got_sigusr1) {
            got_sigusr1 = 0;
            emit_message("REPORT", "raport nou adaugat in sistem");
        }
    }

    /* 6. Cleanup */
    emit_message("EXIT", "monitor oprit prin SIGINT, sterg .monitor_pid");
    unlink(PID_FILE);

    return EXIT_SUCCESS;
}
