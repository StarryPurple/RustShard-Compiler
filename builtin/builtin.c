#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void print(const char* s) {
  fflush(stdout);
  printf("%s", s);
}

void println(const char* s) {
  fflush(stdout);
  printf("%s\n", s);
}

void printInt(int n) {
  fflush(stdout);
  printf("%d", n);
}

void printlnInt(int n) {
  fflush(stdout);
  printf("%d\n", n);
}

char* getString() {
  static char buffer[1024];
  if (fgets(buffer, sizeof(buffer), stdin)) {
    size_t len = strlen(buffer);
    if (len > 0 && buffer[len-1] == '\n') {
      buffer[len-1] = '\0';
    }
    char* result = malloc(len + 1);
    strcpy(result, buffer);
    return result;
  }
  return NULL;
}

int getInt() {
  int n;
  scanf("%d", &n);
  return n;
}

int readInt() {
  return getInt();
}

void exit(int code) {
  fflush(stdout);
  _Exit(code);
}

char* from(const char* s) {
  size_t len = strlen(s);
  char* result = malloc(len + 1);
  strcpy(result, s);
  return result;
}

/*#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void print(const char* s) {
  printf("%s", s);
}

void println(const char* s) {
  printf("%s\n", s);
}

void printInt(int n) {
  printf("%d", n);
}

void printlnInt(int n) {
  printf("%d\n", n);
}

char* getString() {
  static char buffer[1024];
  if(scanf("%1023s", buffer) == 1) {
    size_t len = strlen(buffer);
    if(len > 0 && buffer[len - 1] == '\n') {
      buffer[len - 1] = '\0';
    }
    char* result = malloc(len + 1);
    strcpy(result, buffer);
    return result;
  }
  return NULL;
}

int getInt() {
  int n;
  scanf("%d", &n);
  return n;
}

int readInt() {
  return getInt();
}

void exit(int code) {
  // nothing
}

char* from(const char* s) {
  size_t len = strlen(s);
  char* result = malloc(len + 1);
  strcpy(result, s);
  return result;
}*/