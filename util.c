/*
 * util.c - Funções comumente usadas
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "util.h"

static const int BUFSIZE = 1024;

void *checked_malloc(int len)
{void *p = malloc(len);
 if (!p) {
    fprintf(stderr,"\nRan out of memory!\n");
    exit(1);
 }
 return p;
}

void *checked_realloc(void *p, size_t size)
{
	void *ptr = realloc(p, size);
	if (!ptr) {
		fprintf(stderr, "\nRand out of memory (realloc)!\n");
		exit(1);
	}
	return ptr;
}

string String_from_int(int n)
{
	char *str = checked_malloc(sizeof(*str) * (BUFSIZE + 1));
	snprintf(str, BUFSIZE + 1, "%d", n);
	return str;
}

string String(char *s)
{string p = checked_malloc(strlen(s)+1);
 strcpy(p,s);
 return p;
}

U_boolList U_BoolList(bool head, U_boolList tail)
{ U_boolList list = checked_malloc(sizeof(*list));
  list->head = head;
  list->tail = tail;
  return list;
}

string String_format(const char *s, ...)
{
	char buffer[BUFSIZE], *result = NULL;
	const char *p = s;
	const char *str = NULL;
	int len = 0;
	int i = 0;
	int n = 0;
	bool isDigit = FALSE;
	va_list ap;
	va_start(ap, s);
	for (; *p; p++) {
		if (*p == '%') {
			switch (*++p) {
			case 's':
					str = va_arg(ap, const char *);
					break;
			case 'd':
					str = String_from_int(va_arg(ap, int));
					isDigit = TRUE;
					break;
			default:
					assert(0);
			}
			n = strlen(str);
		} else {
			OK:
			if (i < BUFSIZE - 1) {
				buffer[i++] = *p; continue;
			} else {
				str = p;
				n = 1;
			}
		}
		if (i + n > BUFSIZE) {
			result = checked_realloc(result, sizeof(*result) * (len + i + 1));
			if (len > 0) strncat(result, buffer, i);
			else strncpy(result, buffer, i);
			len += i;
			i = 0;
		}
		strncpy(buffer + i, str, n);
		i += n;
		if (isDigit) { free((void *)str); str = NULL; isDigit = FALSE; }
	}
	if (i > 0) {
		result = checked_realloc(result, sizeof(*result) * (len + i + 1));
		if (len > 0) strncat(result, buffer, i);
		else strncpy(result, buffer, i);
	}
	va_end(ap);
	return result;
}

int needStaticLink(char *name) {
    if (!strcmp(name, "getchar")) return 0;
    if (!strcmp(name, "ord"))     return 0;
    if (!strcmp(name, "print"))   return 0;
    if (!strcmp(name, "printi"))  return 0;
    if (!strcmp(name, "chr"))     return 0;
    return 1;
}

string String_toPut(char *s) {
  int len = strlen(s);
  char *ret = malloc(2*len+1);
  int i = 0, p = 0;

  for (i=0; i<len; i++) {

    if (s[i]!='\n' && s[i]!='\t') {
      ret[p]=s[i];
      p++;
    }

    else if (s[i]=='\n') {
      ret[p]='\\';
      ret[p+1]='n';
      p+=2;
    }

    else {
      ret[p]='\\';
      ret[p+1]='t';
      p+=2;
    }

  }

  ret[p]='\0';
  return ret;
}