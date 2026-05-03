/* ===========================================================
 * monitor_reports.c - Phase 2
 * Proces de monitorizare care raspunde la SIGUSR1 si SIGINT.
 *
 * Student: Cadar Minodora
 *
 * Functionare:
 * - La startup, scrie PID-ul propriu in .monitor_pid
 * - La SIGUSR1: afiseaza un mesaj (raport nou adaugat)
 * - La SIGINT: sterge .monitor_pid, afiseaza mesaj si iese
 *
 * TODO Phase 2:
 * - Implementare handler-e cu sigaction (NU signal())
 * - Bucla principala cu pause()
 * - Cleanup la SIGINT
 * =========================================================== */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <fcntl.h>

#define PID_FILE ".monitor_pid"

int main(void) {
    /* TODO: scriere PID in .monitor_pid */
    /* TODO: instalare handler SIGUSR1 cu sigaction */
    /* TODO: instalare handler SIGINT cu sigaction */
    /* TODO: bucla while(1) pause() */

    printf("monitor_reports: schelet creat, urmeaza implementarea Phase 2.\n");
    return 0;
}
