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

struct skipargs {
  int flags;
  char *label;
  int steps;
};

struct wldsargs {
  int sin, freq, amp;
};

struct labelargs {
  char *name;
};

struct instruction {
  char *op;
  union {
    struct skipargs skp;
    struct wldsargs wlds;
    struct labelargs label;
  } args;
} instr[2 * MAX_INSTRUCTIONS];
int ninstr;

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
  while (**text && space(**text))
    (*text)++;
}

void skipuntilspace(char **text) {
  while (**text && !space(**text))
    (*text)++;
}

int num(char c) { return c >= '0' && c <= '9'; }

void expectnum(char *text) {
  if (!num(*text))
    error("expected number, got %s", text);
}

void expectchar(char *text, char c) {
  if (*text != c)
    error("expected %c, got %s", c, text);
}

void eatstr(char **text, char *prefix) {
  char *p = prefix, *q = *text;

  while (*p && *q && *p == *q) {
    p++;
    q++;
  }
  if (*p)
    error("expected text starting with %s, got %s", prefix, text);

  *text = q;
}

char *Strdup(char *text) {
  char *q = strdup(text);
  if (!q)
    error("strdup failed");
  return q;
}

void parsemem(char *p, char *pend) {
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

void parseequ(char *p, char *pend) {
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

void printnone(struct instruction in) {}

void parseskp(char *p, char *pend) {
  int flags = 0;
  char *q;

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

  p++;
  skipspace(&p);
  q = p;
  skipuntilspace(&q);
  *q = 0;

  if (ninstr >= nelem(instr))
    error("too many instructions");
  instr[ninstr].op = "skp";
  instr[ninstr].args.skp.flags = flags;
  if (num(*q))
    instr[ninstr].args.skp.steps = atoi(p);
  else
    instr[ninstr].args.skp.label = Strdup(p);

  ninstr++;
}

void printskp(struct instruction in) {
  int i;

  printf("skp ");
  for (i = 0; i < nelem(skpflags); i++) {
    if (in.args.skp.flags & 1 << i) {
      if (i)
        putchar('|');
      printf("%s", skpflags[i]);
    }
  }
  printf(", ");
  if (in.args.skp.label)
    printf("%s", in.args.skp.label);
  else
    printf("%d", in.args.skp.steps);
}

void parsewlds(char *p, char *pend) {
  int x;

  skipspace(&p);
  eatstr(&p, "sin");
  if (*p != '0' && *p != '1')
    error("expected sin0 or sin1, got %s", p[-3]);

  instr[ninstr].op = "wlds";
  instr[ninstr].args.wlds.sin = *p++ - '0';

  skipspace(&p);
  eatstr(&p, ",");

  skipspace(&p);
  expectnum(p);
  x = atoi(p);
  if (x >= (1 << 9) || x < 0)
    error("expected uint9, got %s", p);
  instr[ninstr].args.wlds.freq = x;
  while (*p && num(*p))
    p++;

  skipspace(&p);
  eatstr(&p, ",");
  skipspace(&p);

  x = atoi(p);
  if (x >= (1 << 15) || x < 0)
    error("expected uint15, got %s", p);
  instr[ninstr].args.wlds.amp = x;

  ninstr++;
}

void printwlds(struct instruction in) {
  printf("wlds sin%d, %d, %d", in.args.wlds.sin, in.args.wlds.freq,
         in.args.wlds.amp);
}

void parsenone(char *p, char *pend) {}

void printlabel(struct instruction in) { printf("%s:", in.args.label.name); }

struct {
  char *op;
  void (*parse)(char *p, char *pend);
  void (*print)(struct instruction in);
} optab[] = {{"mem", parsemem, printnone},
             {"equ", parseequ, printnone},
             {"skp", parseskp, printskp},
             {"wlds", parsewlds, printwlds},
             {"label", parsenone, printlabel}};

int main(void) {
  char *p;
  int i, j;

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

    if ((q = strchr(p, ':'))) {
      *q = 0; /* TODO validate label */
      instr[ninstr].op = "label";
      instr[ninstr].args.label.name = Strdup(p);
      ninstr++;
      continue;
    }

    for (i = 0; i < nelem(optab); i++) {
      if (match(optab[i].op, &p)) {
        optab[i].parse(p, pend);
        break;
      }
    }
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

  for (i = 0; i < ninstr; i++) {
    for (j = 0; j < nelem(optab); j++) {
      if (!strcmp(instr[i].op, optab[j].op)) {
        printf("instr[%d]=", i);
        optab[j].print(instr[i]);
        putchar('\n');
        break;
      }
    }
  }
}
