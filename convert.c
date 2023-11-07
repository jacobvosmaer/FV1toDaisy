#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char line[1024];

void error(char *fmt, ...) {
  va_list ap;

  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  fputc('\n', stderr);
  va_end(ap);

  exit(1);
}

int space(char c) { return c == ' ' || c == '\t'; }

int match(char *expect, char *actual) {
  while (*expect && (*expect++ == *actual++))
    ;
  return !*expect && (!*actual || space(*actual));
}

int main(void) {
  char *p, *pend;

  while ((p = fgets(line, sizeof(line) - 1, stdin))) {
    pend = strchr(p, '\n');
    if (!pend)
      error("input line longer than %d bytes", sizeof(line) - 1);
    while (space(*p))
      p++;

    if (pend[-1] == '\r')
      pend[-1] = 0;

    if (*p == ';') /* skip comment lines */
      continue;

    if (match("mem", p)) {
      puts(p);
    } else if (match("equ", p)) {
      puts(p);
    }
  }
}
