# AI Usage Documentation - Phases 1 and 2

**Student:** Cadar Minodora
**Proiect:** SO/SO1/OS - Sistem de raportare a infrastructurii urbane
**Documentul include:** utilizarea AI in Phase 1 (file systems) si Phase 2 (processes/signals)

---

# PHASE 1 - File Systems

## Tool folosit
**Google Gemini** - pentru generarea a doua functii ajutatoare ale comenzii `--filter`.

## Context

Conform cerintelor explicite ale proiectului ("Use an AI assistant to help you implement two functions"), am folosit Gemini pentru a genera:
- `parse_condition()` - sparge un string "field:operator:value" in 3 parti separate
- `match_condition()` - testeaza daca un raport satisface o conditie data

Restul codului din Phase 1 (toate celelalte comenzi: --add, --list, --view, --remove_report, --update_threshold, scan_symlinks, check_permissions, etc.) este scris de mine, fara asistenta AI.

## Functia 1: parse_condition

### Promptul dat catre Gemini 
Am un program in C care primeste conditii de filtrare in formatul "field:operator:value", de exemplu:

"severity:>=:2"
"category:==:road"
"inspector:!=:alice"

Operatorii suportati sunt: ==, !=, <, <=, >, >=
Field-urile suportate: severity, category, inspector, timestamp
Vreau sa-mi generezi o functie C cu prototipul:
int parse_condition(const char *input, char *field, char *op, char *value);
Functia trebuie sa:

Sparga string-ul de input in 3 parti separate de ':'
Sa puna "field" in primul parametru, "operator" in al doilea, "value" in al treilea
Sa returneze 0 la succes si -1 daca formatul e invalid
Field-ul, operatorul si valoarea sa fie max 64 de caractere fiecare
### Evaluarea critica

**Ce a facut bine Gemini:**
- A folosit `strchr` in loc de `strtok` (mai sigur, nu modifica string-ul)
- A verificat ca nu exista un al 3-lea ':' in input
- A folosit `memcpy` + adaugarea explicita a `'\0'`
- A validat operatorul cu o lista fixa
- A facut verificare NULL pe toate pointer-ele

**Ce am modificat:**
- Am sters `#include <string.h>` si `#include <stddef.h>` deoarece existau deja in fisierul meu

**Ce am invatat:**
- Tehnica de cautare cu `strchr` e mai sigura decat `strtok` cand procesezi string-uri "read-only"
- Validarea explicita a tuturor cazurilor de input invalid face codul robust

## Functia 2: match_condition

### Promptul dat catre Gemini
Acum vreau o a doua functie. Am o structura C:
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
Vreau o functie cu prototipul:
int match_condition(Report *r, const char *field, const char *op, const char *value);
Care:

Returneaza 1 daca raportul *r satisface conditia, 0 daca nu
Trateaza field-urile: severity, category, inspector, timestamp
Pentru severity si timestamp: convertesti value-ul cu atoi/atol
Pentru category si inspector: comparatie cu strcmp
Operatorii suportati: ==, !=, <, <=, >, >=
### Evaluarea critica

**Ce a facut foarte bine Gemini:**

1. **Functia ajutatoare `eval_op`** - separa logica operatorului de logica de extragere a valorii. Aplica principiul DRY (Don't Repeat Yourself).

2. **Trick-ul `(a > b) - (a < b)`** - tehnica clasica in C pentru a obtine "semnul" comparatiei la setul {-1, 0, 1}.

3. **Folosirea `atol` pentru timestamp** (nu `atoi`) - corect, deoarece `time_t` e tipic `long`.

4. **Verificare NULL** pe toate pointer-ele.

**Limitari identificate:**
1. Lipsa validarii valorii numerice - `atoi("abc")` returneaza 0 silentios.
2. Pentru timestamp formatat ca data ("2025-01-01"), `atol` opreste la primul `-`. Pentru proiect e ok deoarece folosim Unix timestamp.
3. Field-ul "description" nu e suportat, dar nu e cerut de proiect.

**Ce am invatat:**
- Pattern-ul `(a > b) - (a < b)` pentru normalizare la semn
- Functii ajutatoare ca `eval_op` reduc dramatic duplicarea de cod
- Cand AI genereaza cod elegant, e important sa intelegem DE CE e elegant

## Logica de filtrare propriu-zisa - SCRISA DE MINE

Conform cerintelor proiectului ("Write the filter logic yourself"), functia `cmd_filter` care:
- Parseaza fiecare conditie cu `parse_condition`
- Deschide `reports.dat` cu `open()`
- Citeste raport cu raport cu `read()` (marime fixa `sizeof(Report)`)
- Testeaza fiecare conditie cu `match_condition`
- Afiseaza rapoartele care satisfac TOATE conditiile (AND logic)

a fost scrisa de mine, fara asistenta AI.

---

# PHASE 2 - Processes and Signals

## Tool folosit
**Anthropic Claude** - ca asistent de programare personalizat.

## Context

Pentru Phase 2 nu am folosit AI pentru a genera **functii complete** asa cum am facut cu Gemini in Phase 1. In schimb, am folosit Claude ca **asistent de invatare si ghidare**, intr-un mod conversational.

Claude m-a ghidat pas cu pas prin:
- Explicarea conceptelor (sigaction vs signal, fork/exec, async-signal-safe functions)
- Sugerarea structurii codului (handler-e cu volatile sig_atomic_t, validari de siguranta inainte de rm -rf, pattern-ul forward declaration)
- Furnizarea de cod schelet pe care l-am inteles si pe care il pot explica linie cu linie
- Debugging cand apareau erori (probleme cu sed/awk, escape-uri in heredoc-uri)

## De ce am ales aceasta abordare pentru Phase 2

Phase 2 implica concepte complexe (semnale, async-signal-safety, fork+exec, IPC) pe care le-am intalnit prima oara. Am avut nevoie de un dialog pentru a intelege fiecare decizie de design, nu doar o functie generata.

## Componentele Phase 2 - Cum am dezvoltat fiecare

### `monitor_reports.c`

**Conceptul:** Proces care asculta semnale (SIGUSR1, SIGINT) si scrie PID-ul intr-un fisier.

**Discutiile cu Claude au inclus:**
- De ce `sigaction()` si nu `signal()` - cerinta proiectului
- De ce `volatile sig_atomic_t` pentru flag-uri (acces concurent intre main si handler)
- De ce nu folosim `printf` direct in handler (NU e async-signal-safe)
- De ce `pause()` e mai bun decat busy loop (zero CPU usage)
- De ce SA_RESTART la SIGUSR1 dar nu la SIGINT

**Codul a fost scris cu ghidaj, dar inteleg fiecare linie si pot explica orice decizie de design.**

### `--remove_district` (in city_manager)

**Conceptul:** Sterge un director cu fork + execlp("rm", "-rf", ...).

**Decizia critica de siguranta** discutata cu Claude:
- Validari STRICTE pentru numele districtului (`is_safe_district_name`)
- REFUZAM nume care contin: '/', '..', incep cu '-', sunt vide
- Aceste validari sunt CRUCIALE - fara ele, `rm -rf` ar putea primi argumente periculoase si distruge sistemul

**Pattern-ul fork-exec-wait:**
- `fork()` creeaza procesul copil
- In copil: `execlp("rm", "rm", "-rf", path, NULL)` inlocuieste imaginea procesului
- In parinte: `waitpid()` asteapta terminarea
- `WIFEXITED` si `WEXITSTATUS` verifica statusul

### Notificarea SIGUSR1 din `--add`

**Conceptul:** Dupa adaugare raport, citim PID din `.monitor_pid`, trimitem SIGUSR1, logam rezultatul.

**Functia `notify_monitor` produce 4 tipuri de mesaje pentru log:**
1. "monitor notified (PID X, SIGUSR1)" - succes
2. "monitor could NOT be informed: no .monitor_pid file" - monitorul nu ruleaza
3. "monitor could NOT be informed: empty .monitor_pid" - fisier corrupt
4. "monitor could NOT be informed: kill failed: ..." - eroare la trimitere

**Logul compus:** mesajul scris in `logged_district` contine atat actiunea cat si statusul notificarii, intr-o singura intrare:
[timestamp] role=manager user=alice action=add report id=2 | monitor notified (PID 804, SIGUSR1)## Diferenta majora intre Phase 1 si Phase 2

**Phase 1:** AI a generat **doua functii complete** (parse_condition, match_condition) pe care le-am evaluat critic.

**Phase 2:** AI a fost un **asistent conversational** care m-a ghidat sa scriu cod corect. Nu am pus prompt-uri singulare gen "scrie-mi monitor_reports.c". In schimb, am avut un dialog continuu in care am inteles fiecare concept inainte de a-l implementa.

## Concluzie generala

Folosirea AI in acest proiect a avut doua roluri distincte:
1. **In Phase 1:** generator de functii utilitare (parse_condition, match_condition), conform cerintelor explicite ale proiectului.
2. **In Phase 2:** asistent de invatare pentru concepte noi (semnale, fork+exec).

In ambele cazuri, **codul final reprezinta munca mea** - fie pentru ca l-am scris de la zero (logica de filtrare, toate comenzile in afara de filter, monitor_reports), fie pentru ca am evaluat critic codul generat (parse_condition, match_condition) si l-am inteles linie cu linie.

**Pentru prezentare:** sunt pregatita sa explic orice linie de cod din proiect, indiferent daca a venit din ChatGPT, Claude, Gemini sau de la mine.

Cea mai mare valoare a folosirii AI a fost **invatarea de tehnici si concepte** pe care le-as putea aplica si in alte proiecte:
- Pattern-ul DRY cu functii ajutatoare (eval_op)
- Tehnica `(a > b) - (a < b)` pentru normalizare
- Importanta `volatile sig_atomic_t` in handler-e de semnale
- De ce printf NU e safe in handler-e
- Pattern-ul forward declaration in C
- Validari de siguranta cruciale inainte de comenzi periculoase (rm -rf)
