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
 * STRUCTURA UNUI RAPORT - mărime FIXĂ
 * ========================================== */
typedef struct {
    int     id;
    char    inspector[32];
    float   latitude;
    float   longitude;
    char    category[16];
    int     severity;
    time_t  timestamp;
    char    description[128];
} Report;

/* ==========================================
 * CONSTANTE PENTRU ROLURI
 * ========================================== */
#define ROLE_NONE       0
#define ROLE_INSPECTOR  1
#define ROLE_MANAGER    2

/* ==========================================
 * CONSTANTE PENTRU COMENZI
 * ========================================== */
#define CMD_NONE             0
#define CMD_ADD              1
#define CMD_LIST             2
#define CMD_VIEW             3
#define CMD_REMOVE_REPORT    4
#define CMD_UPDATE_THRESHOLD 5
#define CMD_FILTER           6

/* ==========================================
 * STRUCTURA CARE TINE ARGUMENTELE PARSATE
 * ========================================== */
typedef struct {
    int   role;                  // ROLE_INSPECTOR sau ROLE_MANAGER
    char  user[32];              // numele utilizatorului
    int   command;               // CMD_ADD, CMD_LIST, etc.
    char  district[64];          // ex: "downtown"
    int   report_id;             // pentru --view, --remove_report
    int   threshold_value;       // pentru --update_threshold
    char  filter_conditions[10][128]; // condițiile pentru --filter
    int   filter_count;          // câte condiții am primit
} Args;

/* ==========================================
 * FUNCTIE: afișează cum se folosește programul
 * ========================================== */
void print_usage(const char *prog_name) {
    fprintf(stderr,
        "Utilizare: %s --role <inspector|manager> --user <name> --<command> [args]\n"
        "Comenzi disponibile:\n"
        "  --add <district_id>\n"
        "  --list <district_id>\n"
        "  --view <district_id> <report_id>\n"
        "  --remove_report <district_id> <report_id>     (manager only)\n"
        "  --update_threshold <district_id> <value>      (manager only)\n"
        "  --filter <district_id> <condition> [more...]\n",
        prog_name);
}

/* ==========================================
 * FUNCTIE: parsează argumentele din linia de comandă
 * Returnează 0 la succes, -1 la eroare
 * ========================================== */
int parse_args(int argc, char *argv[], Args *args) {
    // Inițializăm tot pe zero
    memset(args, 0, sizeof(Args));

    int i = 1;  // începem de la 1, sărind peste numele programului

    while (i < argc) {
        // --role <inspector|manager>
        if (strcmp(argv[i], "--role") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Eroare: --role necesita un argument\n");
                return -1;
            }
            if (strcmp(argv[i + 1], "inspector") == 0) {
                args->role = ROLE_INSPECTOR;
            } else if (strcmp(argv[i + 1], "manager") == 0) {
                args->role = ROLE_MANAGER;
            } else {
                fprintf(stderr, "Eroare: rol invalid '%s'\n", argv[i + 1]);
                return -1;
            }
            i += 2;
        }
        // --user <name>
        else if (strcmp(argv[i], "--user") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Eroare: --user necesita un argument\n");
                return -1;
            }
            strncpy(args->user, argv[i + 1], sizeof(args->user) - 1);
            i += 2;
        }
        // --add <district_id>
        else if (strcmp(argv[i], "--add") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Eroare: --add necesita <district_id>\n");
                return -1;
            }
            args->command = CMD_ADD;
            strncpy(args->district, argv[i + 1], sizeof(args->district) - 1);
            i += 2;
        }
        // --list <district_id>
        else if (strcmp(argv[i], "--list") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Eroare: --list necesita <district_id>\n");
                return -1;
            }
            args->command = CMD_LIST;
            strncpy(args->district, argv[i + 1], sizeof(args->district) - 1);
            i += 2;
        }
        // --view <district_id> <report_id>
        else if (strcmp(argv[i], "--view") == 0) {
            if (i + 2 >= argc) {
                fprintf(stderr, "Eroare: --view necesita <district_id> <report_id>\n");
                return -1;
            }
            args->command = CMD_VIEW;
            strncpy(args->district, argv[i + 1], sizeof(args->district) - 1);
            args->report_id = atoi(argv[i + 2]);
            i += 3;
        }
        // --remove_report <district_id> <report_id>
        else if (strcmp(argv[i], "--remove_report") == 0) {
            if (i + 2 >= argc) {
                fprintf(stderr, "Eroare: --remove_report necesita <district_id> <report_id>\n");
                return -1;
            }
            args->command = CMD_REMOVE_REPORT;
            strncpy(args->district, argv[i + 1], sizeof(args->district) - 1);
            args->report_id = atoi(argv[i + 2]);
            i += 3;
        }
        // --update_threshold <district_id> <value>
        else if (strcmp(argv[i], "--update_threshold") == 0) {
            if (i + 2 >= argc) {
                fprintf(stderr, "Eroare: --update_threshold necesita <district_id> <value>\n");
                return -1;
            }
            args->command = CMD_UPDATE_THRESHOLD;
            strncpy(args->district, argv[i + 1], sizeof(args->district) - 1);
            args->threshold_value = atoi(argv[i + 2]);
            i += 3;
        }
        // --filter <district_id> <cond1> [cond2 ...]
        else if (strcmp(argv[i], "--filter") == 0) {
            if (i + 2 >= argc) {
                fprintf(stderr, "Eroare: --filter necesita <district_id> si cel putin o conditie\n");
                return -1;
            }
            args->command = CMD_FILTER;
            strncpy(args->district, argv[i + 1], sizeof(args->district) - 1);
            i += 2;
            // Toate argumentele rămase sunt condiții
            while (i < argc && args->filter_count < 10) {
                strncpy(args->filter_conditions[args->filter_count],
                        argv[i], 127);
                args->filter_count++;
                i++;
            }
        }
        else {
            fprintf(stderr, "Eroare: argument necunoscut '%s'\n", argv[i]);
            return -1;
        }
    }

    // Verificări finale
    if (args->role == ROLE_NONE) {
        fprintf(stderr, "Eroare: lipseste --role\n");
        return -1;
    }
    if (args->user[0] == '\0') {
        fprintf(stderr, "Eroare: lipseste --user\n");
        return -1;
    }
    if (args->command == CMD_NONE) {
        fprintf(stderr, "Eroare: lipseste comanda\n");
        return -1;
    }

    return 0;
}

/* ==========================================
 * MAIN
 * ========================================== */
int main(int argc, char *argv[]) {
    Args args;

    if (parse_args(argc, argv, &args) < 0) {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    // Pentru moment, doar afisam ce am parsat ca sa verificam
    printf("=== Argumente parsate ===\n");
    printf("Rol: %s\n", args.role == ROLE_MANAGER ? "manager" : "inspector");
    printf("User: %s\n", args.user);
    printf("District: %s\n", args.district);

    const char *cmd_names[] = {"none", "add", "list", "view",
                               "remove_report", "update_threshold", "filter"};
    printf("Comanda: %s\n", cmd_names[args.command]);

    if (args.command == CMD_VIEW || args.command == CMD_REMOVE_REPORT)
        printf("Report ID: %d\n", args.report_id);
    if (args.command == CMD_UPDATE_THRESHOLD)
        printf("Threshold: %d\n", args.threshold_value);
    if (args.command == CMD_FILTER) {
        printf("Conditii (%d):\n", args.filter_count);
        for (int i = 0; i < args.filter_count; i++)
            printf("  [%d] %s\n", i, args.filter_conditions[i]);
    }

    return 0;
}