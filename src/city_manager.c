#include <stdio.h>      // printf, perror, fgets
#include <stdlib.h>     // exit, EXIT_FAILURE, atoi
#include <string.h>     // strcmp, strncpy, strlen
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
 * CONSTANTE PENTRU ROLURI / COMENZI
 * ========================================== */
#define ROLE_NONE       0
#define ROLE_INSPECTOR  1
#define ROLE_MANAGER    2

#define CMD_NONE             0
#define CMD_ADD              1
#define CMD_LIST             2
#define CMD_VIEW             3
#define CMD_REMOVE_REPORT    4
#define CMD_UPDATE_THRESHOLD 5
#define CMD_FILTER           6

/* ==========================================
 * STRUCTURA CU ARGUMENTELE PARSATE
 * ========================================== */
typedef struct {
    int   role;
    char  user[32];
    int   command;
    char  district[64];
    int   report_id;
    int   threshold_value;
    char  filter_conditions[10][128];
    int   filter_count;
} Args;

/* ==========================================
 * print_usage
 * ========================================== */
void print_usage(const char *prog_name) {
    fprintf(stderr,
        "Utilizare: %s --role <inspector|manager> --user <name> --<command> [args]\n",
        prog_name);
}

/* ==========================================
 * parse_args - identic cu ce aveam înainte
 * ========================================== */
int parse_args(int argc, char *argv[], Args *args) {
    memset(args, 0, sizeof(Args));
    int i = 1;

    while (i < argc) {
        if (strcmp(argv[i], "--role") == 0) {
            if (i + 1 >= argc) return -1;
            if (strcmp(argv[i + 1], "inspector") == 0) args->role = ROLE_INSPECTOR;
            else if (strcmp(argv[i + 1], "manager") == 0) args->role = ROLE_MANAGER;
            else { fprintf(stderr, "Rol invalid: %s\n", argv[i+1]); return -1; }
            i += 2;
        }
        else if (strcmp(argv[i], "--user") == 0) {
            if (i + 1 >= argc) return -1;
            strncpy(args->user, argv[i + 1], sizeof(args->user) - 1);
            i += 2;
        }
        else if (strcmp(argv[i], "--add") == 0) {
            if (i + 1 >= argc) return -1;
            args->command = CMD_ADD;
            strncpy(args->district, argv[i + 1], sizeof(args->district) - 1);
            i += 2;
        }
        else if (strcmp(argv[i], "--list") == 0) {
            if (i + 1 >= argc) return -1;
            args->command = CMD_LIST;
            strncpy(args->district, argv[i + 1], sizeof(args->district) - 1);
            i += 2;
        }
        else if (strcmp(argv[i], "--view") == 0) {
            if (i + 2 >= argc) return -1;
            args->command = CMD_VIEW;
            strncpy(args->district, argv[i + 1], sizeof(args->district) - 1);
            args->report_id = atoi(argv[i + 2]);
            i += 3;
        }
        else if (strcmp(argv[i], "--remove_report") == 0) {
            if (i + 2 >= argc) return -1;
            args->command = CMD_REMOVE_REPORT;
            strncpy(args->district, argv[i + 1], sizeof(args->district) - 1);
            args->report_id = atoi(argv[i + 2]);
            i += 3;
        }
        else if (strcmp(argv[i], "--update_threshold") == 0) {
            if (i + 2 >= argc) return -1;
            args->command = CMD_UPDATE_THRESHOLD;
            strncpy(args->district, argv[i + 1], sizeof(args->district) - 1);
            args->threshold_value = atoi(argv[i + 2]);
            i += 3;
        }
        else if (strcmp(argv[i], "--filter") == 0) {
            if (i + 2 >= argc) return -1;
            args->command = CMD_FILTER;
            strncpy(args->district, argv[i + 1], sizeof(args->district) - 1);
            i += 2;
            while (i < argc && args->filter_count < 10) {
                strncpy(args->filter_conditions[args->filter_count], argv[i], 127);
                args->filter_count++;
                i++;
            }
        }
        else {
            fprintf(stderr, "Argument necunoscut: %s\n", argv[i]);
            return -1;
        }
    }

    if (args->role == ROLE_NONE)    { fprintf(stderr, "Lipseste --role\n"); return -1; }
    if (args->user[0] == '\0')      { fprintf(stderr, "Lipseste --user\n"); return -1; }
    if (args->command == CMD_NONE)  { fprintf(stderr, "Lipseste comanda\n"); return -1; }
    return 0;
}

/* ==========================================
 * write_log - scrie o linie in logged_district
 * Format: [timestamp] role=X user=Y action=Z
 * ========================================== */
void write_log(const char *district, const char *role, const char *user,
               const char *action) {
    char path[256];
    snprintf(path, sizeof(path), "%s/logged_district", district);

    int fd = open(path, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd < 0) {
        perror("write_log: open");
        return;
    }

    // Timestamp formatat
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char ts[32];
    strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", tm_info);

    char line[512];
    int len = snprintf(line, sizeof(line),
                       "[%s] role=%s user=%s action=%s\n",
                       ts, role, user, action);

    write(fd, line, len);
    close(fd);
}

/* ==========================================
 * ensure_district_files - creează directorul și fișierele
 * cu permisiunile cerute, dacă nu există deja
 * ========================================== */
int ensure_district_files(const char *district) {
    struct stat st;
    char path[256];

    // 1. Directorul districtului - rwxr-x--- (750)
    if (stat(district, &st) < 0) {
        if (mkdir(district, 0750) < 0) {
            perror("mkdir district");
            return -1;
        }
        // Forțăm permisiunile chiar dacă umask le-a modificat
        chmod(district, 0750);
        printf("Creat director %s (750)\n", district);
    }

    // 2. reports.dat - rw-rw-r-- (664)
    snprintf(path, sizeof(path), "%s/reports.dat", district);
    if (stat(path, &st) < 0) {
        int fd = open(path, O_WRONLY | O_CREAT, 0664);
        if (fd < 0) { perror("open reports.dat"); return -1; }
        close(fd);
        chmod(path, 0664);
        printf("Creat fisier %s (664)\n", path);
    }

    // 3. district.cfg - rw-r----- (640)
    snprintf(path, sizeof(path), "%s/district.cfg", district);
    if (stat(path, &st) < 0) {
        int fd = open(path, O_WRONLY | O_CREAT, 0640);
        if (fd < 0) { perror("open district.cfg"); return -1; }
        // Scriem un threshold default
        const char *default_cfg = "severity_threshold=2\n";
        write(fd, default_cfg, strlen(default_cfg));
        close(fd);
        chmod(path, 0640);
        printf("Creat fisier %s (640)\n", path);
    }

    // 4. logged_district - rw-r--r-- (644)
    snprintf(path, sizeof(path), "%s/logged_district", district);
    if (stat(path, &st) < 0) {
        int fd = open(path, O_WRONLY | O_CREAT, 0644);
        if (fd < 0) { perror("open logged_district"); return -1; }
        close(fd);
        chmod(path, 0644);
        printf("Creat fisier %s (644)\n", path);
    }

    // 5. Symlink active_reports-<district> -> <district>/reports.dat
    char link_name[256];
    snprintf(link_name, sizeof(link_name), "active_reports-%s", district);
    snprintf(path, sizeof(path), "%s/reports.dat", district);

    struct stat link_st;
    if (lstat(link_name, &link_st) < 0) {
        // Nu există încă - îl creăm
        if (symlink(path, link_name) < 0) {
            perror("symlink");
            return -1;
        }
        printf("Creat symlink %s -> %s\n", link_name, path);
    }

    return 0;
}

/* ==========================================
 * get_next_report_id - parcurge reports.dat și
 * returnează cel mai mare id + 1
 * ========================================== */
int get_next_report_id(const char *district) {
    char path[256];
    snprintf(path, sizeof(path), "%s/reports.dat", district);

    int fd = open(path, O_RDONLY);
    if (fd < 0) return 1;  // Nu există fișier => primul ID

    Report r;
    int max_id = 0;
    while (read(fd, &r, sizeof(Report)) == sizeof(Report)) {
        if (r.id > max_id) max_id = r.id;
    }
    close(fd);
    return max_id + 1;
}

/* ==========================================
 * cmd_add - implementarea comenzii --add
 * ========================================== */
int cmd_add(Args *args) {
    // 1. Asigurăm că directorul și fișierele există cu permisiuni corecte
    if (ensure_district_files(args->district) < 0) return -1;

    // 2. Construim raportul - citim datele de la utilizator
    Report r;
    memset(&r, 0, sizeof(Report));

    r.id = get_next_report_id(args->district);
    strncpy(r.inspector, args->user, sizeof(r.inspector) - 1);
    r.timestamp = time(NULL);

    char buf[256];

    printf("=== Raport nou pentru districtul %s (ID = %d) ===\n",
           args->district, r.id);

    printf("Latitudine (ex: 45.7589): ");
    if (fgets(buf, sizeof(buf), stdin)) r.latitude = atof(buf);

    printf("Longitudine (ex: 21.2287): ");
    if (fgets(buf, sizeof(buf), stdin)) r.longitude = atof(buf);

    printf("Categorie (road/lighting/flooding/other): ");
    if (fgets(buf, sizeof(buf), stdin)) {
        buf[strcspn(buf, "\n")] = '\0';  // taie newline-ul
        strncpy(r.category, buf, sizeof(r.category) - 1);
    }

    printf("Severitate (1=minor, 2=moderate, 3=critical): ");
    if (fgets(buf, sizeof(buf), stdin)) r.severity = atoi(buf);

    printf("Descriere (max 127 caractere): ");
    if (fgets(buf, sizeof(buf), stdin)) {
        buf[strcspn(buf, "\n")] = '\0';
        strncpy(r.description, buf, sizeof(r.description) - 1);
    }

    // 3. Deschidem reports.dat în mod append și scriem raportul
    char path[256];
    snprintf(path, sizeof(path), "%s/reports.dat", args->district);
    int fd = open(path, O_WRONLY | O_APPEND);
    if (fd < 0) {
        perror("open reports.dat");
        return -1;
    }

    if (write(fd, &r, sizeof(Report)) != sizeof(Report)) {
        perror("write report");
        close(fd);
        return -1;
    }
    close(fd);

    // Forțăm permisiunile la 664 (în caz că umask le-a schimbat)
    chmod(path, 0664);

    printf("Raport %d adaugat cu succes.\n", r.id);

    // 4. Logăm acțiunea
    char action[64];
    snprintf(action, sizeof(action), "add report id=%d", r.id);
    write_log(args->district,
              args->role == ROLE_MANAGER ? "manager" : "inspector",
              args->user, action);

    return 0;
}

/* ==========================================
 * MAIN
 * ========================================== */
 /* ==========================================
 * permissions_to_string - converteste st_mode in "rw-rw-r--"
 * Trebuie sa o scriem singuri, nu folosim ls/stat extern
 * ========================================== */
void permissions_to_string(mode_t mode, char *out) {
    out[0] = (mode & S_IRUSR) ? 'r' : '-';
    out[1] = (mode & S_IWUSR) ? 'w' : '-';
    out[2] = (mode & S_IXUSR) ? 'x' : '-';
    out[3] = (mode & S_IRGRP) ? 'r' : '-';
    out[4] = (mode & S_IWGRP) ? 'w' : '-';
    out[5] = (mode & S_IXGRP) ? 'x' : '-';
    out[6] = (mode & S_IROTH) ? 'r' : '-';
    out[7] = (mode & S_IWOTH) ? 'w' : '-';
    out[8] = (mode & S_IXOTH) ? 'x' : '-';
    out[9] = '\0';
}

/* ==========================================
 * print_report - afiseaza un raport frumos
 * ========================================== */
void print_report(const Report *r) {
    char ts[32];
    struct tm *tm_info = localtime(&r->timestamp);
    strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", tm_info);

    printf("--- Report ID %d ---\n", r->id);
    printf("  Inspector  : %s\n", r->inspector);
    printf("  Coordonate : %.4f, %.4f\n", r->latitude, r->longitude);
    printf("  Categorie  : %s\n", r->category);
    printf("  Severitate : %d\n", r->severity);
    printf("  Timestamp  : %s\n", ts);
    printf("  Descriere  : %s\n", r->description);
}

/* ==========================================
 * cmd_list - implementarea comenzii --list
 * ========================================== */
int cmd_list(Args *args) {
    char path[256];
    snprintf(path, sizeof(path), "%s/reports.dat", args->district);

    // 1. Stat ca sa luam info despre fisier (permisiuni, marime, mtime)
    struct stat st;
    if (stat(path, &st) < 0) {
        perror("stat reports.dat");
        return -1;
    }

    // 2. Convertim permisiunile in string simbolic
    char perms[10];
    permissions_to_string(st.st_mode, perms);

    // 3. Formatam timestamp-ul ultimei modificari
    char mtime_str[32];
    struct tm *tm_info = localtime(&st.st_mtime);
    strftime(mtime_str, sizeof(mtime_str), "%Y-%m-%d %H:%M:%S", tm_info);

    // 4. Afisam header-ul cu info despre fisier
    printf("=== Fisier reports.dat ===\n");
    printf("  Permisiuni : %s\n", perms);
    printf("  Marime     : %ld bytes\n", (long)st.st_size);
    printf("  Modificat  : %s\n", mtime_str);
    printf("  Numar rec. : %ld\n", (long)(st.st_size / sizeof(Report)));
    printf("\n");

    // 5. Deschidem fisierul si citim raport cu raport
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        perror("open reports.dat");
        return -1;
    }

    Report r;
    int count = 0;
    while (read(fd, &r, sizeof(Report)) == sizeof(Report)) {
        print_report(&r);
        count++;
    }
    close(fd);

    if (count == 0) {
        printf("(Niciun raport in district)\n");
    } else {
        printf("\nTotal: %d rapoarte.\n", count);
    }

    // 6. Logam actiunea
    write_log(args->district,
              args->role == ROLE_MANAGER ? "manager" : "inspector",
              args->user, "list");

    return 0;
}
int main(int argc, char *argv[]) {
    Args args;

    if (parse_args(argc, argv, &args) < 0) {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

   int rc = 0;
    switch (args.command) {
        case CMD_ADD:
            rc = cmd_add(&args);
            break;
        case CMD_LIST:
            rc = cmd_list(&args);
            break;
        case CMD_VIEW:
        case CMD_REMOVE_REPORT:
        case CMD_UPDATE_THRESHOLD:
        case CMD_FILTER:
            printf("Comanda nu e inca implementata.\n");
            break;
    }

    return rc < 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}