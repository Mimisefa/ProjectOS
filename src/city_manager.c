#include <stdio.h>      // printf, perror
#include <stdlib.h>     // exit, EXIT_FAILURE
#include <string.h>     // strcmp, strncpy
#include <unistd.h>     // read, write, close, lseek, ftruncate, unlink, symlink
#include <fcntl.h>      // open, O_RDONLY, O_WRONLY, O_CREAT, O_APPEND
#include <sys/stat.h>   // stat, lstat, mkdir, chmod, S_IRUSR, etc.
#include <sys/types.h>  // tipuri pentru system calls
#include <time.h>       // time_t, time(), ctime()
#include <dirent.h>     // pentru scanare directoare (mai târziu)
#include <errno.h>      // errno, perror

/* ==========================================
 * STRUCTURA UNUI RAPORT
 * Mărime FIXĂ - foarte important pentru lseek/ftruncate
 * ========================================== */
typedef struct {
    int     id;                 // ID-ul raportului
    char    inspector[32];      // Numele inspectorului
    float   latitude;           // Coordonate GPS
    float   longitude;
    char    category[16];       // ex: "road", "lighting", "flooding"
    int     severity;           // 1 = minor, 2 = moderate, 3 = critical
    time_t  timestamp;          // Momentul creării
    char    description[128];   // Text descriere
} Report;

/* ==========================================
 * MAIN - deocamdată doar afișează argumentele
 * ========================================== */
int main(int argc, char *argv[]) {
    printf("Programul a primit %d argumente:\n", argc);

    for (int i = 0; i < argc; i++) {
        printf("  argv[%d] = %s\n", i, argv[i]);
    }

    printf("\nMarimea unei structuri Report: %zu bytes\n", sizeof(Report));

    return 0;
}