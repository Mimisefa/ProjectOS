# City Manager - Sistem de raportare a infrastructurii urbane

**Student:** Cadar Minodora
**Curs:** Sisteme de Operare (SO/SO1/OS)
**Faza:** Phase 1 - File Systems

## Descriere

Program C in mediul UNIX care implementeaza un sistem de raportare a problemelor de infrastructura urbana. Inspectorii orasului inregistreaza rapoarte structurate despre probleme (drumuri deteriorate, iluminat defect, inundatii, etc.) in cartiere.

Sistemul:
- Stocheaza rapoarte in fisiere binare cu inregistrari de marime fixa
- Aplica drepturi de acces bazate pe permisiunile UNIX si rolul utilizatorului
- Foloseste system calls UNIX (open, read, write, lseek, ftruncate, stat, lstat, mkdir, symlink, chmod)
- Suporta filtrare cu conditii de tip `field:operator:value`

## Compilare
gcc -Wall -o city_manager src/city_manager.c

## Roluri

- **manager** - acces deplin (creare, citire, modificare, stergere rapoarte si threshold)
- **inspector** - acces limitat (citire si adaugare rapoarte)

## Comenzi disponibile

### Adaugare raport (ambele roluri)
./city_manager --role manager --user alice --add downtown
./city_manager --role inspector --user bob --add downtown

### Listare rapoarte (ambele roluri)
./city_manager --role inspector --user bob --list downtown
Afiseaza si:
- Permisiunile fisierului `reports.dat` in format simbolic (rw-rw-r--)
- Marimea si data ultimei modificari
- Verificarea symlink-urilor pentru detectarea link-urilor rupte

### Vizualizare raport specific (ambele roluri)
./city_manager --role inspector --user bob --view downtown 1

### Stergere raport (DOAR manager)
./city_manager --role manager --user alice --remove_report downtown 2
Foloseste lseek() pentru shift si ftruncate() pentru taierea fisierului.

### Modificare threshold (DOAR manager)
./city_manager --role manager --user alice --update_threshold downtown 3
Verifica strict ca permisiunile lui `district.cfg` sunt 640 inainte de scriere.

### Filtrare (ambele roluri)
./city_manager --role inspector --user bob --filter downtown "severity:>=:2"
./city_manager --role inspector --user bob --filter downtown "severity:>=:2" "category:==:road"
**ATENTIE:** conditiile cu `>=`, `<=`, `>` trebuie puse in GHILIMELE in shell.

Operatori suportati: `==`, `!=`, `<`, `<=`, `>`, `>=`
Field-uri suportate: `severity`, `category`, `inspector`, `timestamp`

## Structura fisierelor
ProjectOS/
├── src/
│   └── city_manager.c       # Cod sursa principal
├── ai_usage.md              # Documentare utilizare AI (Gemini)
├── README.md                # Acest fisier
├── city_manager             # Executabilul (generat de gcc)
├── active_reports-<id>      # Symlink-uri spre reports.dat din cartiere
└── <district>/              # Director per cartier
├── reports.dat          # Fisier binar cu rapoarte (664)
├── district.cfg         # Configuratie threshold (640)
└── logged_district      # Log de operatii (644)

## Permisiuni

Conform cerintelor proiectului:

| Fisier | Permisiuni | Octal |
|---|---|---|
| Director district | rwxr-x--- | 750 |
| reports.dat | rw-rw-r-- | 664 |
| district.cfg | rw-r----- | 640 |
| logged_district | rw-r--r-- | 644 |

Maparea rolurilor pe biti:
- **manager** = OWNER (S_IRUSR / S_IWUSR)
- **inspector** = GROUP (S_IRGRP / S_IWGRP)

## Utilizarea AI in proiect

Conform cerintelor explicite ale proiectului, am folosit **Google Gemini** pentru a genera doua functii ajutatoare ale comenzii `--filter`:

- `parse_condition()` - sparge un string "field:op:value" in 3 parti
- `match_condition()` - testeaza daca un raport satisface o conditie

Restul codului (logica de filtrare, toate celelalte comenzi) este scris de mine, fara asistenta AI.

Detalii complete despre prompt-uri, codul generat si evaluarea critica se gasesc in [ai_usage.md](ai_usage.md).

## System calls folosite

- **File I/O:** open, close, read, write, lseek, ftruncate
- **Metadata:** stat, lstat, chmod
- **Directoare:** mkdir, opendir, readdir, closedir
- **Link-uri:** symlink, readlink, unlink

---

## Phase 2: Processes and Signals (in progres)

Modificari planificate fata de Phase 1:

- Comanda noua `--remove_district <district_id>` (manager only):
  - Foloseste `fork()` + `execlp("rm", "rm", "-rf", ...)` pentru stergere
  - Sterge si symlink-ul corespunzator
- Program nou `monitor_reports`:
  - Scrie PID in `.monitor_pid` la startup, sterge la iesire
  - Raspunde la `SIGUSR1` (raport nou) si `SIGINT` (terminare)
  - Foloseste `sigaction()` (NU `signal()`)
- Modificare comanda `--add` din city_manager:
  - Notifica monitorul cu `kill(pid, SIGUSR1)` cand se adauga raport
  - Logheaza in `logged_district` daca notificarea a reusit sau nu

Compilare Phase 2:
gcc -Wall -o city_manager src/city_manager.c
gcc -Wall -o monitor_reports src/monitor_reports.c
