# AI Usage Documentation - Phase 1

**Student:** Cadar Minodora
**Tool folosit:** Google Gemini

---

## Context

Proiectul SO/SO1/OS - Faza 1 implementeaza un sistem de raportare a problemelor de infrastructura urbana. Conform cerintelor explicite ale proiectului, am folosit AI pentru a genera doua functii ajutatoare in implementarea comenzii `--filter`:

- `parse_condition()` - sparge un string "field:operator:value" in 3 parti separate
- `match_condition()` - testeaza daca un raport satisface o conditie data

Restul codului (logica de filtrare, citirea fisierului, integrarea cu programul, toate celelalte comenzi) este scris de mine, fara asistenta AI.

---

## Functia 1: parse_condition

### Promptul dat catre Gemini
#include <string.h>
#include <stddef.h>

#define MAX_PART_LEN 64

int parse_condition(const char *input, char *field, char *op, char *value) {
    if (input == NULL || field == NULL || op == NULL || value == NULL) {
        return -1;
    }

    /* Primul ':' */
    const char *c1 = strchr(input, ':');
    if (c1 == NULL || c1 == input) {
        return -1;  /* lipsa ':' sau field gol */
    }

    /* Al doilea ':' */
    const char *c2 = strchr(c1 + 1, ':');
    if (c2 == NULL || c2 == c1 + 1) {
        return -1;  /* lipsa al doilea ':' sau operator gol */
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

    /* Copiere parti + terminator NUL */
    memcpy(field, input,    field_len); field[field_len] = '\0';
    memcpy(op,    c1 + 1,   op_len);    op[op_len]       = '\0';
    memcpy(value, c2 + 1,   value_len); value[value_len] = '\0';

    /* Validare operator (1 sau 2 caractere, dintr-o lista fixa) */
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
### Evaluarea mea critica

**Ce a facut bine Gemini:**
- A folosit `strchr` in loc de `strtok`. Asta e important: `strtok` modifica string-ul de intrare (inlocuieste separatorii cu `\0`), ceea ce poate produce bug-uri greu de gasit. `strchr` doar gaseste pozitia, fara a modifica nimic.
- A verificat ca nu exista un al 3-lea `:` in input - protectie impotriva formatelor ciudate (de ex. "a:b:c:d" ar fi fost acceptat de o implementare naiva).
- A folosit `memcpy` + adaugarea manuala a `'\0'` pentru copiere, mai explicit decat `strcpy` sau `strncpy`.
- A validat ca operatorul e dintr-o lista fixa de 6 operatori valizi (==, !=, <, <=, >, >=).
- A facut verificare NULL pe toate cele 4 pointer-e de intrare.
- A validat ca lungimile partilor nu sunt 0 si nu depasesc 64 de caractere.

**Ce am modificat la integrare:**
- Am sters liniile `#include <string.h>` si `#include <stddef.h>` deoarece acestea exista deja in fisierul meu principal `city_manager.c`.
- Am pastrat constanta `MAX_PART_LEN = 64` la inceputul sectiunii de filter.

**Ce am invatat:**
- Tehnica de cautare manuala a separatorilor cu `strchr` e mai sigura decat `strtok` cand procesezi string-uri "read-only".
- Validarea explicita a tuturor cazurilor de input invalid (NULL, lungimi 0, lipsa separatori, separatori in plus, operator necunoscut) face codul robust.

---

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

Returneaza 1 daca raportul *r satisface conditia (field op value), 0 daca nu
Trateaza field-urile suportate: severity, category, inspector, timestamp
Pentru severity si timestamp: convertesti value-ul din string in int (cu atoi sau atol) si compari numeric
Pentru category si inspector: comparatie de string-uri (strcmp)
Operatorii suportati: ==, !=, <, <=, >, >=

Pentru string-uri, == inseamna strcmp == 0, != inseamna strcmp != 0, iar <, <=, >, >= inseamna comparatii lexicografice (strcmp returneaza < 0, > 0 etc.).
### Codul generat de Gemini (original, neschimbat)

```c
#include <string.h>
#include <stdlib.h>
#include <time.h>
static int eval_op(int sign, const char *op) {
    if (strcmp(op, "==") == 0) return sign == 0;
    if (strcmp(op, "!=") == 0) return sign != 0;
    if (strcmp(op, "<")  == 0) return sign <  0;
    if (strcmp(op, "<=") == 0) return sign <= 0;
    if (strcmp(op, ">")  == 0) return sign >  0;
    if (strcmp(op, ">=") == 0) return sign >= 0;
    return 0;
}
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
```

### Evaluarea mea critica

**Ce a facut foarte bine Gemini:**

1. **Functia ajutatoare `eval_op`:** Aceasta separa logica operatorului de logica de extragere a valorii. Fara ea, ar fi trebuit sa repet 6 ramuri `if/else` pentru fiecare field. Codul ar fi fost de aproximativ 4x mai lung si mai greu de mentinut. Aplica principiul DRY (Don't Repeat Yourself).

2. **Trick-ul cu `sign = (a > b) - (a < b)`:** Tehnica clasica in C pentru a obtine "semnul" comparatiei:
   - daca `a > b`: `(1) - (0) = 1`
   - daca `a < b`: `(0) - (1) = -1`
   - daca `a == b`: `(0) - (0) = 0`

   Astfel `sign` are valori standardizate {-1, 0, 1}.

3. **Pentru string-uri se foloseste acelasi pattern:** `(cmp > 0) - (cmp < 0)`. Aceasta normalizeaza rezultatul `strcmp` (care poate returna orice numar pozitiv sau negativ) la setul {-1, 0, 1}.

4. **Folosirea `atol` pentru timestamp** (nu `atoi`): Corect, deoarece `time_t` este in mod tipic `long` pe sistemele moderne, nu `int`. Daca s-ar fi folosit `atoi`, am fi avut overflow pentru timestamp-uri mari (Unix epoch dupa 2038).

5. **Verificare NULL** pe toate pointer-ele de intrare - defensiv si corect.

6. **Returnarea 0 pentru field necunoscut** - comportament sigur, nu provoaca crash.

**Limitari pe care le-am identificat:**

1. **Lipsa validarii valorii numerice:** Daca utilizatorul scrie `severity:>=:abc`, `atoi("abc")` returneaza 0 silentios si comparatia merge cu 0 in loc sa raporteze eroare.

2. **Pentru timestamp:** Daca utilizatorul scrie un format de data ca "2025-01-01", `atol` opreste la primul caracter non-cifra si returneaza doar 2025. Pentru proiectul nostru e ok deoarece timestamp-ul nostru este Unix timestamp (numar epoch).

3. **Field-ul "description" nu este suportat,** dar nu este cerut de proiect.

**Ce am modificat la integrare:**
- Am sters liniile `#include`-uri (`string.h`, `stdlib.h`, `time.h`) deoarece toate exista deja in fisierul meu principal.
- Am pastrat functia `eval_op` ca `static` pentru a nu polua namespace-ul global.

**Ce am invatat:**
- Pattern-ul `(a > b) - (a < b)` este o tehnica clasica in C pentru a normaliza comparatii la semn.
- Functiile ajutatoare ca `eval_op` reduc dramatic duplicarea de cod.
- Cand un AI genereaza cod elegant, e important sa intelegem DE CE e elegant - nu doar sa-l acceptam orbeste.

---

## Logica de filtrare propriu-zisa (scrisa DE MINE, fara AI)

Conform cerintelor explicite ale proiectului ("Write the filter logic yourself"), functia `cmd_filter` care:

1. Parseaza fiecare conditie din linia de comanda apeland `parse_condition`
2. Deschide fisierul `reports.dat` cu `open()` in mod read-only
3. Citeste raport cu raport folosind `read()` cu mărimea fixa `sizeof(Report)`
4. Pentru fiecare raport, testeaza fiecare conditie cu `match_condition`
5. Afiseaza rapoartele care satisfac TOATE conditiile (AND logic, conform spec)
6. Logheaza actiunea in `logged_district`

a fost scrisa de mine, fara asistenta AI. Aceasta integreaza cele doua functii generate de Gemini, dar ea insasi reprezinta munca mea originala.

---

## Concluzie generala

**Avantajele folosirii AI pentru aceste functii:**

1. **Idei algoritmice elegante:** Gemini a sugerat tehnici (eval_op, sign trick) pe care nu le aveam in vedere initial si care reduc semnificativ complexitatea codului.

2. **Cod defensiv din start:** AI a inclus verificari NULL, validari si protectii defensive (NULL pointer checks, validare lungimi) care sunt usor de uitat la o prima implementare.

3. **Timp economisit:** Implementarea celor doua functii ar fi luat probabil 1-2 ore daca le scriam de la zero. Cu AI a durat ~30 minute, inclusiv timpul de evaluare critica a codului generat.

**Limitele AI-ului identificate:**

1. **Validare incompleta a contextului:** Codul taca silentios la input invalid (de ex. `atoi("abc")` returneaza 0) in loc sa raporteze eroarea.

2. **Duplicarea include-urilor:** Gemini a inclus `#include`-uri care erau deja in fisierul meu principal.

3. **Lipsa explicatiei design-ului:** AI nu a explicat de ce a ales un design vs altul. A trebuit sa analizez singura codul ca sa inteleg de ce `eval_op` e o solutie eleganta.

**Concluzie principala:**

Pentru o livrabila profesionala, codul AI trebuie INTOTDEAUNA verificat linie cu linie inainte de a fi incorporat in proiect. AI-ul este un asistent util pentru "schitarea" rapida a unor functii standard, dar responsabilitatea finala pentru corectitudine si securitate ramane la programator.

Cea mai mare valoare a folosirii AI in acest proiect a fost INVATAREA de noi tehnici (DRY, sign trick, eval_op pattern) pe care le voi putea aplica si in alte proiecte viitoare.
AIUSAGE_EOF
