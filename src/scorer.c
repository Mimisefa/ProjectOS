/* ===========================================================
 * scorer.c - Phase 3
 * Calculeaza scorul de workload pentru fiecare inspector dintr-un district.
 * Workload score = suma severitatilor rapoartelor filate de acel inspector.
 *
 * Student: Cadar Minodora
 *
 * Utilizare: ./scorer <district_id>
 * Afiseaza pe stdout un sumar text. In Phase 3, city_hub redirecteaza
 * acest stdout printr-un pipe folosind dup2().
 * =========================================================== */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

/* Structura Report - IDENTICA cu cea din city_manager.c.
 * Trebuie sa fie identica pentru ca citim acelasi fisier binar. */
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

/* Numarul maxim de inspectori distincti pe care ii contorizam */
#define MAX_INSPECTORS 64

/* Structura pentru a acumula scorul fiecarui inspector */
typedef struct {
    char name[32];
    int  total_severity;   /* suma severitatilor */
    int  report_count;     /* cate rapoarte are */
} InspectorScore;

int main(int argc, char *argv[]) {
    /* 1. Verificam argumentele */
    if (argc != 2) {
        fprintf(stderr, "Utilizare: %s <district_id>\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char *district = argv[1];

    /* 2. Construim calea catre reports.dat */
    char path[256];
    snprintf(path, sizeof(path), "%s/reports.dat", district);

    /* 3. Deschidem fisierul binar */
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        /* Districtul nu exista sau nu are reports.dat */
        printf("District: %s (EROARE: nu pot deschide %s)\n", district, path);
        return EXIT_FAILURE;
    }

    /* 4. Citim rapoartele si acumulam scorurile */
    InspectorScore scores[MAX_INSPECTORS];
    int num_inspectors = 0;

    Report r;
    while (read(fd, &r, sizeof(Report)) == sizeof(Report)) {
        /* Cautam daca inspectorul exista deja in lista noastra */
        int found = -1;
        for (int i = 0; i < num_inspectors; i++) {
            if (strcmp(scores[i].name, r.inspector) == 0) {
                found = i;
                break;
            }
        }

        if (found >= 0) {
            /* Inspector existent - acumulam */
            scores[found].total_severity += r.severity;
            scores[found].report_count++;
        } else {
            /* Inspector nou - il adaugam */
            if (num_inspectors < MAX_INSPECTORS) {
                strncpy(scores[num_inspectors].name, r.inspector,
                        sizeof(scores[num_inspectors].name) - 1);
                scores[num_inspectors].name[sizeof(scores[num_inspectors].name)-1] = '\0';
                scores[num_inspectors].total_severity = r.severity;
                scores[num_inspectors].report_count = 1;
                num_inspectors++;
            }
        }
    }
    close(fd);

    /* 5. Afisam sumarul pe stdout */
    printf("District: %s\n", district);
    if (num_inspectors == 0) {
        printf("  (Niciun raport in acest district)\n");
    } else {
        for (int i = 0; i < num_inspectors; i++) {
            printf("  Inspector %s: workload score %d (%d reports)\n",
                   scores[i].name,
                   scores[i].total_severity,
                   scores[i].report_count);
        }
    }

    return EXIT_SUCCESS;
}
