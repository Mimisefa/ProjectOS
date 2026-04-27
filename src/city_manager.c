/* ===========================================================
 * city_manager.c - Phase 1
 * Sistem de raportare a problemelor de infrastructura urbana
 * =========================================================== */

#include <stdio.h>      /* printf, perror, fgets */
#include <stdlib.h>     /* exit, atoi, atof */
#include <string.h>     /* strcmp, strncpy, strlen, memset */
#include <unistd.h>     /* read, write, close, lseek, ftruncate, unlink, symlink */
#include <fcntl.h>      /* open, O_RDONLY, O_WRONLY, O_CREAT, O_APPEND, O_TRUNC, O_RDWR */
#include <sys/stat.h>   /* stat, lstat, mkdir, chmod, S_IRUSR, etc. */
#include <sys/types.h>
#include <time.h>       /* time_t, time(), localtime(), strftime() */
#include <dirent.h>
#include <errno.h>

/* ==========================================
 * STRUCTURA UNUI RAPORT - marime FIXA
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
 * CONSTANTE
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
 void scan_symlinks(void);
void print_usage(const char *prog_name) {
    fprintf(stderr,
        "Utilizare: %s --role <inspector|manager> --user <name> --<command> [args]\n",
        prog_name);
}

/* ==========================================
 * parse_args - parseaza argumentele din linia de comanda
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
 * permissions_to_string - converteste st_mode in "rw-rw-r--"
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
 * check_permissions - verifica daca rolul declarat are accesul cerut
 *   manager  = OWNER  (S_IRUSR / S_IWUSR)
 *   inspector = GROUP (S_IRGRP / S_IWGRP)
 *   access_mode: 'r' pentru read, 'w' pentru write
 * ========================================== */
int check_permissions(const char *path, int role, char access_mode) {
    struct stat st;
    if (stat(path, &st) < 0) {
        perror("stat (check_permissions)");
        return -1;
    }

    mode_t needed_bit = 0;

    if (role == ROLE_MANAGER) {
        needed_bit = (access_mode == 'r') ? S_IRUSR : S_IWUSR;
    } else if (role == ROLE_INSPECTOR) {
        needed_bit = (access_mode == 'r') ? S_IRGRP : S_IWGRP;
    } else {
        fprintf(stderr, "check_permissions: rol necunoscut\n");
        return -1;
    }

    if (!(st.st_mode & needed_bit)) {
        const char *role_name = (role == ROLE_MANAGER) ? "manager" : "inspector";
        const char *op_name = (access_mode == 'r') ? "citire" : "scriere";
        fprintf(stderr,
                "Eroare: rolul '%s' nu are drept de %s pe '%s'\n",
                role_name, op_name, path);
        return -1;
    }

    return 0;
}/* ==========================================
 * #define pentru parse_condition (generat cu AI)
 * ========================================== */
#define MAX_PART_LEN 64

/* ==========================================
 * parse_condition - generata cu AI (vezi ai_usage.md)
 * Sparge un string "field:operator:value" in 3 parti.
 * Returneaza 0 la succes, -1 la eroare.
 * ========================================== */
int parse_condition(const char *input, char *field, char *op, char *value) {
    if (input == NULL || field == NULL || op == NULL || value == NULL) {
        return -1;
    }
    /* Primul ':' */
    const char *c1 = strchr(input, ':');
    if (c1 == NULL || c1 == input) {
        return -1;
    }
    /* Al doilea ':' */
    const char *c2 = strchr(c1 + 1, ':');
    if (c2 == NULL || c2 == c1 + 1) {
        return -1;
    }
    /* Nu trebuie sa existe un al treilea ':' */
    if (strchr(c2 + 1, ':') != NULL) {
        return -1;
    }
    size_t field_len = (size_t)(c1 - input);
    size_t op_len    = (size_t)(c2 - c1 - 1);
    size_t value_len = strlen(c2 + 1);
    if (field_len == 0 || field_len > MAX_PART_LEN) return -1;
    if (op_len    == 0 || op_len    > MAX_PART_LEN) return -1;
    if (value_len == 0 || value_len > MAX_PART_LEN) return -1;
    memcpy(field, input,    field_len); field[field_len] = '\0';
    memcpy(op,    c1 + 1,   op_len);    op[op_len]       = '\0';
    memcpy(value, c2 + 1,   value_len); value[value_len] = '\0';
    /* Validare operator */
    if (!(strcmp(op, "==") == 0 ||
          strcmp(op, "!=") == 0 ||
          strcmp(op, "<")  == 0 ||
          strcmp(op, "<=") == 0 ||
          strcmp(op, ">")  == 0 ||
          strcmp(op, ">=") == 0)) {
        return -1;
    }
    return 0;
}

/* ==========================================
 * eval_op - helper pentru match_condition
 * sign < 0 => a<b ; sign == 0 => a==b ; sign > 0 => a>b
 * ========================================== */
static int eval_op(int sign, const char *op) {
    if (strcmp(op, "==") == 0) return sign == 0;
    if (strcmp(op, "!=") == 0) return sign != 0;
    if (strcmp(op, "<")  == 0) return sign <  0;
    if (strcmp(op, "<=") == 0) return sign <= 0;
    if (strcmp(op, ">")  == 0) return sign >  0;
    if (strcmp(op, ">=") == 0) return sign >= 0;
    return 0;
}

/* ==========================================
 * match_condition - generata cu AI (vezi ai_usage.md)
 * Returneaza 1 daca raportul *r satisface conditia "field op value", 0 altfel.
 * ========================================== */
int match_condition(Report *r, const char *field, const char *op, const char *value) {
    if (r == NULL || field == NULL || op == NULL || value == NULL) {
        return 0;
    }
    int sign;
    if (strcmp(field, "severity") == 0) {
        int v = atoi(value);
        sign = (r->severity > v) - (r->severity < v);
    }
    else if (strcmp(field, "timestamp") == 0) {
        long v  = atol(value);
        long ts = (long)r->timestamp;
        sign = (ts > v) - (ts < v);
    }
    else if (strcmp(field, "category") == 0) {
        int cmp = strcmp(r->category, value);
        sign = (cmp > 0) - (cmp < 0);
    }
    else if (strcmp(field, "inspector") == 0) {
        int cmp = strcmp(r->inspector, value);
        sign = (cmp > 0) - (cmp < 0);
    }
    else {
        return 0;
    }
    return eval_op(sign, op);
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
 * write_log - scrie o linie in logged_district
 * Verifica permisiunile inainte (644 = doar owner/manager poate scrie)
 * ========================================== */
void write_log(const char *district, const char *role, const char *user,
               const char *action) {
    char path[256];
    snprintf(path, sizeof(path), "%s/logged_district", district);

    /* Verificare drept de scriere pe logged_district */
    struct stat st;
    if (stat(path, &st) == 0) {
        int role_id = (strcmp(role, "manager") == 0) ? ROLE_MANAGER : ROLE_INSPECTOR;
        mode_t needed = (role_id == ROLE_MANAGER) ? S_IWUSR : S_IWGRP;
        if (!(st.st_mode & needed)) {
            fprintf(stderr,
                "Avertisment: rolul '%s' nu are drept de scriere pe %s; logging sarit.\n",
                role, path);
            return;
        }
    }

    int fd = open(path, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd < 0) {
        perror("write_log: open");
        return;
    }

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
 * ensure_district_files - creeaza directorul si fisierele
 * cu permisiunile corecte daca nu exista deja
 * ========================================== */
int ensure_district_files(const char *district) {
    struct stat st;
    char path[256];

    /* 1. Director - rwxr-x--- (750) */
    if (stat(district, &st) < 0) {
        if (mkdir(district, 0750) < 0) { perror("mkdir district"); return -1; }
        chmod(district, 0750);
        printf("Creat director %s (750)\n", district);
    }

    /* 2. reports.dat - rw-rw-r-- (664) */
    snprintf(path, sizeof(path), "%s/reports.dat", district);
    if (stat(path, &st) < 0) {
        int fd = open(path, O_WRONLY | O_CREAT, 0664);
        if (fd < 0) { perror("open reports.dat"); return -1; }
        close(fd);
        chmod(path, 0664);
        printf("Creat fisier %s (664)\n", path);
    }

    /* 3. district.cfg - rw-r----- (640) */
    snprintf(path, sizeof(path), "%s/district.cfg", district);
    if (stat(path, &st) < 0) {
        int fd = open(path, O_WRONLY | O_CREAT, 0640);
        if (fd < 0) { perror("open district.cfg"); return -1; }
        const char *default_cfg = "severity_threshold=2\n";
        write(fd, default_cfg, strlen(default_cfg));
        close(fd);
        chmod(path, 0640);
        printf("Creat fisier %s (640)\n", path);
    }

    /* 4. logged_district - rw-r--r-- (644) */
    snprintf(path, sizeof(path), "%s/logged_district", district);
    if (stat(path, &st) < 0) {
        int fd = open(path, O_WRONLY | O_CREAT, 0644);
        if (fd < 0) { perror("open logged_district"); return -1; }
        close(fd);
        chmod(path, 0644);
        printf("Creat fisier %s (644)\n", path);
    }

    /* 5. Symlink active_reports-<district> -> <district>/reports.dat */
    char link_name[256];
    snprintf(link_name, sizeof(link_name), "active_reports-%s", district);
    snprintf(path, sizeof(path), "%s/reports.dat", district);

    struct stat link_st;
    if (lstat(link_name, &link_st) < 0) {
        if (symlink(path, link_name) < 0) { perror("symlink"); return -1; }
        printf("Creat symlink %s -> %s\n", link_name, path);
    }

    return 0;
}

/* ==========================================
 * get_next_report_id
 * ========================================== */
int get_next_report_id(const char *district) {
    char path[256];
    snprintf(path, sizeof(path), "%s/reports.dat", district);

    int fd = open(path, O_RDONLY);
    if (fd < 0) return 1;

    Report r;
    int max_id = 0;
    while (read(fd, &r, sizeof(Report)) == sizeof(Report)) {
        if (r.id > max_id) max_id = r.id;
    }
    close(fd);
    return max_id + 1;
}

/* ==========================================
 * cmd_add
 * ========================================== */
int cmd_add(Args *args) {
    if (ensure_district_files(args->district) < 0) return -1;

    /* Verificare permisiuni: ambele roluri pot scrie pe reports.dat */
    char check_path[256];
    snprintf(check_path, sizeof(check_path), "%s/reports.dat", args->district);
    if (check_permissions(check_path, args->role, 'w') < 0) return -1;

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
        buf[strcspn(buf, "\n")] = '\0';
        strncpy(r.category, buf, sizeof(r.category) - 1);
    }

    printf("Severitate (1=minor, 2=moderate, 3=critical): ");
    if (fgets(buf, sizeof(buf), stdin)) r.severity = atoi(buf);

    printf("Descriere (max 127 caractere): ");
    if (fgets(buf, sizeof(buf), stdin)) {
        buf[strcspn(buf, "\n")] = '\0';
        strncpy(r.description, buf, sizeof(r.description) - 1);
    }

    char path[256];
    snprintf(path, sizeof(path), "%s/reports.dat", args->district);
    int fd = open(path, O_WRONLY | O_APPEND);
    if (fd < 0) { perror("open reports.dat"); return -1; }

    if (write(fd, &r, sizeof(Report)) != sizeof(Report)) {
        perror("write report");
        close(fd);
        return -1;
    }
    close(fd);
    chmod(path, 0664);

    printf("Raport %d adaugat cu succes.\n", r.id);

    char action[64];
    snprintf(action, sizeof(action), "add report id=%d", r.id);
    write_log(args->district,
              args->role == ROLE_MANAGER ? "manager" : "inspector",
              args->user, action);

    return 0;
}

/* ==========================================
 * cmd_list
 * ========================================== */
int cmd_list(Args *args) {
    char path[256];
    snprintf(path, sizeof(path), "%s/reports.dat", args->district);

    if (check_permissions(path, args->role, 'r') < 0) return -1;

    struct stat st;
    if (stat(path, &st) < 0) { perror("stat reports.dat"); return -1; }

    char perms[10];
    permissions_to_string(st.st_mode, perms);

    char mtime_str[32];
    struct tm *tm_info = localtime(&st.st_mtime);
    strftime(mtime_str, sizeof(mtime_str), "%Y-%m-%d %H:%M:%S", tm_info);

    printf("=== Fisier reports.dat ===\n");
    printf("  Permisiuni : %s\n", perms);
    printf("  Marime     : %ld bytes\n", (long)st.st_size);
    printf("  Modificat  : %s\n", mtime_str);
    printf("  Numar rec. : %ld\n\n", (long)(st.st_size / sizeof(Report)));

    int fd = open(path, O_RDONLY);
    if (fd < 0) { perror("open reports.dat"); return -1; }

    Report r;
    int count = 0;
    while (read(fd, &r, sizeof(Report)) == sizeof(Report)) {
        print_report(&r);
        count++;
    }
    close(fd);

    if (count == 0) printf("(Niciun raport in district)\n");
    else            printf("\nTotal: %d rapoarte.\n", count);

    write_log(args->district,
              args->role == ROLE_MANAGER ? "manager" : "inspector",
              args->user, "list");
    scan_symlinks();
    return 0;
}

/* ==========================================
 * cmd_view
 * ========================================== */
int cmd_view(Args *args) {
    char path[256];
    snprintf(path, sizeof(path), "%s/reports.dat", args->district);

    if (check_permissions(path, args->role, 'r') < 0) return -1;

    int fd = open(path, O_RDONLY);
    if (fd < 0) { perror("open reports.dat"); return -1; }

    Report r;
    int found = 0;
    while (read(fd, &r, sizeof(Report)) == sizeof(Report)) {
        if (r.id == args->report_id) {
            print_report(&r);
            found = 1;
            break;
        }
    }
    close(fd);

    if (!found) {
        fprintf(stderr, "Eroare: nu exista raport cu ID %d in districtul %s\n",
                args->report_id, args->district);
        return -1;
    }

    char action[64];
    snprintf(action, sizeof(action), "view report id=%d", args->report_id);
    write_log(args->district,
              args->role == ROLE_MANAGER ? "manager" : "inspector",
              args->user, action);
    return 0;
}

/* ==========================================
 * cmd_remove_report - lseek + ftruncate (MANAGER ONLY)
 * ========================================== */
int cmd_remove_report(Args *args) {
    if (args->role != ROLE_MANAGER) {
        fprintf(stderr, "Eroare: doar managerul poate sterge rapoarte\n");
        return -1;
    }

    char path[256];
    snprintf(path, sizeof(path), "%s/reports.dat", args->district);

    if (check_permissions(path, args->role, 'r') < 0) return -1;
    if (check_permissions(path, args->role, 'w') < 0) return -1;

    struct stat st_before;
    if (stat(path, &st_before) < 0) { perror("stat reports.dat"); return -1; }
    off_t size_before = st_before.st_size;
    int total_records = size_before / sizeof(Report);

    if (total_records == 0) {
        fprintf(stderr, "Eroare: districtul %s nu are rapoarte\n", args->district);
        return -1;
    }

    int fd = open(path, O_RDWR);
    if (fd < 0) { perror("open reports.dat"); return -1; }

    Report r;
    int found_index = -1;
    for (int i = 0; i < total_records; i++) {
        if (lseek(fd, (off_t)i * sizeof(Report), SEEK_SET) < 0) {
            perror("lseek search"); close(fd); return -1;
        }
        if (read(fd, &r, sizeof(Report)) != sizeof(Report)) {
            perror("read search"); close(fd); return -1;
        }
        if (r.id == args->report_id) { found_index = i; break; }
    }

    if (found_index < 0) {
        fprintf(stderr, "Eroare: nu exista raport cu ID %d\n", args->report_id);
        close(fd);
        return -1;
    }

    printf("Raportul cu ID %d e la pozitia %d (din %d).\n",
           args->report_id, found_index, total_records);
    printf("Marime fisier INAINTE: %ld bytes (%d rapoarte)\n",
           (long)size_before, total_records);

    /* Shift toate rapoartele de dupa cu o pozitie inapoi */
    for (int i = found_index + 1; i < total_records; i++) {
        if (lseek(fd, (off_t)i * sizeof(Report), SEEK_SET) < 0) {
            perror("lseek shift read"); close(fd); return -1;
        }
        if (read(fd, &r, sizeof(Report)) != sizeof(Report)) {
            perror("read shift"); close(fd); return -1;
        }
        if (lseek(fd, (off_t)(i - 1) * sizeof(Report), SEEK_SET) < 0) {
            perror("lseek shift write"); close(fd); return -1;
        }
        if (write(fd, &r, sizeof(Report)) != sizeof(Report)) {
            perror("write shift"); close(fd); return -1;
        }
    }

    off_t new_size = size_before - sizeof(Report);
    if (ftruncate(fd, new_size) < 0) { perror("ftruncate"); close(fd); return -1; }
    close(fd);

    struct stat st_after;
    if (stat(path, &st_after) == 0) {
        printf("Marime fisier DUPA  : %ld bytes (%ld rapoarte)\n",
               (long)st_after.st_size,
               (long)(st_after.st_size / sizeof(Report)));
    }

    printf("Raport %d sters cu succes.\n", args->report_id);

    char action[64];
    snprintf(action, sizeof(action), "remove report id=%d", args->report_id);
    write_log(args->district, "manager", args->user, action);
    return 0;
}

/* ==========================================
 * cmd_update_threshold (MANAGER ONLY + verificare 640 strict)
 * ========================================== */
int cmd_update_threshold(Args *args) {
    if (args->role != ROLE_MANAGER) {
        fprintf(stderr, "Eroare: doar managerul poate modifica threshold-ul\n");
        return -1;
    }

    char path[256];
    snprintf(path, sizeof(path), "%s/district.cfg", args->district);

    if (check_permissions(path, args->role, 'r') < 0) return -1;
    if (check_permissions(path, args->role, 'w') < 0) return -1;

    struct stat st;
    if (stat(path, &st) < 0) { perror("stat district.cfg"); return -1; }

    mode_t actual_perms = st.st_mode & 0777;
    if (actual_perms != 0640) {
        fprintf(stderr,
            "Eroare: permisiunile lui district.cfg sunt %03o, asteptam 640.\n"
            "        Refuz sa scriu intr-un fisier cu permisiuni modificate.\n",
            actual_perms);
        return -1;
    }

    int new_val = args->threshold_value;
    if (new_val < 1 || new_val > 3) {
        fprintf(stderr, "Eroare: threshold trebuie sa fie 1, 2 sau 3 (a fost %d)\n", new_val);
        return -1;
    }

    int fd = open(path, O_WRONLY | O_TRUNC);
    if (fd < 0) { perror("open district.cfg"); return -1; }

    char line[64];
    int len = snprintf(line, sizeof(line), "severity_threshold=%d\n", new_val);
    if (write(fd, line, len) != len) {
        perror("write district.cfg"); close(fd); return -1;
    }
    close(fd);
    chmod(path, 0640);

    printf("Threshold actualizat la %d in %s\n", new_val, path);

    char action[64];
    snprintf(action, sizeof(action), "update_threshold value=%d", new_val);
    write_log(args->district, "manager", args->user, action);
    return 0;
}
/* ==========================================
 * cmd_filter - SCRISA DE MINE (foloseste parse_condition si match_condition)
 *
 * Algoritm:
 * 1. Parseaza fiecare conditie din linia de comanda cu parse_condition
 * 2. Pentru fiecare raport din reports.dat:
 *      Pentru fiecare conditie:
 *          Daca match_condition returneaza 0 -> raportul nu trece, sarim la urmatorul
 *      Daca toate conditiile au returnat 1 -> afisam raportul
 * ========================================== */
int cmd_filter(Args *args) {
    char path[256];
    snprintf(path, sizeof(path), "%s/reports.dat", args->district);

    /* Verificare permisiuni: ambele roluri pot citi reports.dat */
    if (check_permissions(path, args->role, 'r') < 0) return -1;

    /* Pregatim conditiile parsate */
    typedef struct {
        char field[MAX_PART_LEN + 1];
        char op[MAX_PART_LEN + 1];
        char value[MAX_PART_LEN + 1];
    } ParsedCondition;

    ParsedCondition parsed[10];
    int parsed_count = 0;

    for (int i = 0; i < args->filter_count; i++) {
        if (parse_condition(args->filter_conditions[i],
                            parsed[i].field,
                            parsed[i].op,
                            parsed[i].value) < 0) {
            fprintf(stderr, "Eroare: conditie invalida '%s'\n",
                    args->filter_conditions[i]);
            return -1;
        }
        printf("Conditie [%d]: field='%s' op='%s' value='%s'\n",
               i, parsed[i].field, parsed[i].op, parsed[i].value);
        parsed_count++;
    }

    /* Deschidem reports.dat si parcurgem raport cu raport */
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        perror("open reports.dat");
        return -1;
    }

    Report r;
    int matched = 0;
    int total = 0;

    printf("\n=== Rapoarte care satisfac toate conditiile ===\n");

    while (read(fd, &r, sizeof(Report)) == sizeof(Report)) {
        total++;
        int passes_all = 1;  /* presupunem ca trece, daca esueaza vreuna -> 0 */

        for (int i = 0; i < parsed_count; i++) {
            if (!match_condition(&r, parsed[i].field, parsed[i].op, parsed[i].value)) {
                passes_all = 0;
                break;  /* AND logic - opreste la prima conditie nesatisfacuta */
            }
        }

        if (passes_all) {
            print_report(&r);
            matched++;
        }
    }
    close(fd);

    printf("\nGasite %d rapoarte (din %d) care satisfac toate cele %d conditii.\n",
           matched, total, parsed_count);

    /* Logam actiunea */
    char action[128];
    snprintf(action, sizeof(action), "filter (%d conditions, %d matches)",
             parsed_count, matched);
    write_log(args->district,
              args->role == ROLE_MANAGER ? "manager" : "inspector",
              args->user, action);

    return 0;
}
/* ==========================================
 * scan_symlinks - scaneaza directorul curent si verifica symlink-urile
 * "active_reports-*". Detecteaza si raporteaza link-urile rupte.
 *
 * Foloseste lstat() (nu stat()) ca sa identifice symlink-urile FARA
 * sa le urmeze. Apoi verifica daca tinta lor exista cu stat().
 * ========================================== */
void scan_symlinks(void) {
    DIR *dir = opendir(".");
    if (dir == NULL) {
        perror("opendir");
        return;
    }

    struct dirent *entry;
    int total_links = 0;
    int broken_links = 0;

    printf("\n=== Scanare symlink-uri active_reports-* ===\n");

    while ((entry = readdir(dir)) != NULL) {
        /* Filtram doar fisierele care incep cu "active_reports-" */
        if (strncmp(entry->d_name, "active_reports-", 15) != 0) {
            continue;
        }

        struct stat lst;
        /* lstat ne arata daca e symlink (NU urmeaza link-ul) */
        if (lstat(entry->d_name, &lst) < 0) {
            fprintf(stderr, "  AVERTISMENT: nu pot lstat '%s'\n", entry->d_name);
            continue;
        }

        /* Verificam ca e intr-adevar symlink */
        if (!S_ISLNK(lst.st_mode)) {
            printf("  '%s' NU este un symlink (?)\n", entry->d_name);
            continue;
        }

        total_links++;

        /* stat (NU lstat!) verifica daca tinta exista */
        struct stat target_st;
        if (stat(entry->d_name, &target_st) < 0) {
            /* Linkul exista, dar tinta nu - link rupt */
            char target[256];
            ssize_t len = readlink(entry->d_name, target, sizeof(target) - 1);
            if (len > 0) target[len] = '\0';
            else strcpy(target, "(necunoscut)");

            fprintf(stderr, "  LINK RUPT: '%s' -> '%s' (tinta nu exista)\n",
                    entry->d_name, target);
            broken_links++;
        } else {
            printf("  OK: '%s' (tinta: %ld bytes)\n",
                   entry->d_name, (long)target_st.st_size);
        }
    }

    closedir(dir);

    printf("\nTotal symlink-uri: %d, rupte: %d\n", total_links, broken_links);
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

    int rc = 0;
    switch (args.command) {
        case CMD_ADD:               rc = cmd_add(&args); break;
        case CMD_LIST:              rc = cmd_list(&args); break;
        case CMD_VIEW:              rc = cmd_view(&args); break;
        case CMD_REMOVE_REPORT:     rc = cmd_remove_report(&args); break;
        case CMD_UPDATE_THRESHOLD:  rc = cmd_update_threshold(&args); break;
        case CMD_FILTER:            rc=cmd_filter(&args); break;
            printf("Comanda --filter va fi implementata cu AI in pasul urmator.\n");
            break;
    }
    return rc < 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}