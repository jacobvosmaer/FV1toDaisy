#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_INSTRUCTIONS 128
#define MAX_MEM_SAMPLES 32768
#define nelem(x) (sizeof(x) / sizeof(*x))

struct mem {
  char *label;
  int offset;
} mem[2 * MAX_INSTRUCTIONS];
int nmem = 0, memtop = 0;

struct equ {
  char *label;
  int isreg;
  union {
    float f;
    int r;
  } value;
} equ[2 * MAX_INSTRUCTIONS];
int nequ = 0;

char *skpflags[] = {"run", "zrc", "zro", "gez", "neg"};

char line[1024];

void error(char *fmt, ...) {
  va_list ap;

  fflush(stdout);

  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  fputc('\n', stderr);
  va_end(ap);

  exit(1);
}

int space(char c) { return c == ' ' || c == '\t'; }

int match(char *expect, char **text) {
  char *p;
  for (p = *text; *expect && (*expect == *p || *expect == (*p + ('a' - 'A')));
       expect++, p++)
    ;
  if (!*expect && (!*p || space(*p))) {
    *text = p;
    return 1;
  } else {
    return 0;
  }
}

void skipspace(char **text) {
  while (space(**text))
    (*text)++;
}

void skipuntilspace(char **text) {
  while (!space(**text))
    (*text)++;
}

void expectnum(char *text) {
  if (*text < '0' || *text > '9')
    error("expected number, got %s", text);
}

char *Strdup(char *text) {
  char *q = strdup(text);
  if (!q)
    error("strdup failed");
  return q;
}

void handlemem(char *p, char *pend) {
  char *q;

  if (nmem >= nelem(mem) - 1)
    error("too many mem declarations");

  skipspace(&p);
  q = p;
  skipuntilspace(&q);
  *q = 0;
  mem[nmem].label = Strdup(p);
  p = q + 1;
  skipspace(&p);
  expectnum(p);
  mem[nmem].offset = memtop;
  memtop += atoi(p);
  if (memtop > MAX_MEM_SAMPLES)
    error("mem declarations use too much memory: %d", memtop);

  nmem++;
}

void handleequ(char *p, char *pend) {
  char *q;

  if (nequ >= nelem(equ))
    error("too many equ declarations");

  skipspace(&p);
  q = p;
  skipuntilspace(&q);
  *q = 0;
  equ[nequ].label = Strdup(p);
  p = q + 1;
  skipspace(&p);
  if (pend - p > 3 && !memcmp(p, "reg", 3)) {
    equ[nequ].isreg = 1;
    p += 3;
    expectnum(p);
    equ[nequ].value.r = atoi(p);
  } else {
    /* TODO handle equ constant */
  }
  nequ++;
}

void handleskp(char *p, char *pend) {
  int flags = 0;
  skipspace(&p);
  while (*p != ',') {
    int i;
    for (i = 0; i < nelem(skpflags); i++) {
      int n = strlen(skpflags[i]);
      if (pend - p >= n + 1 && !memcmp(p, skpflags[i], n) &&
          (p[n] == ',' || p[n] == '|')) {
        flags |= 1 << i;
        p += n + (p[n] == '|');
        break;
      }
    }
    if (i == nelem(skpflags))
      error("expected flag at %s", p);
  }
  if (!flags)
    error("expected nonzero flags bitmap at %s", p);
}

int main(void) {
  char *p;
  int i;

  while ((p = fgets(line, sizeof(line) - 1, stdin))) {
    char *q, *pend = strchr(p, '\n');
    if (!pend)
      error("input line longer than %d bytes", sizeof(line) - 1);
    if (pend > p && pend[-1] == '\r')
      pend--;
    if ((q = strchr(p, ';')))
      pend = q;
    while (p < pend && space(pend[-1]))
      pend--;
    *pend = 0;

    skipspace(&p);

    if (match("mem", &p)) /* TODO: handle "name mem value" syntax */
      handlemem(p, pend);
    else if (match("equ", &p)) /* TODO: handle "name equ value" syntax */
      handleequ(p, pend);
    else if (match("skp", &p))
      handleskp(p, pend);
  }

  /* Add a sentinel element at the end so that for all i<nmem, size(i) ==
   * nmem[i+1].offset - nmem[i].offset. */
  mem[nmem].label = 0;
  mem[nmem].offset = memtop;

  for (i = 0; i < nmem + 1; i++)
    printf("mem[%d]={\"%s\",%d}\n", i, mem[i].label, mem[i].offset);

  for (i = 0; i < nequ; i++)
    if (equ[i].isreg)
      printf("equ[%d]={\"%s\",reg%d}\n", i, equ[i].label, equ[i].value.r);
}