#ifndef COMMON_H
#define COMMON_H

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef enum Status {
  Status_OK = 0,
  Status_FAILED,
} Status;

typedef struct StringArray {
  int64_t size;
  int64_t capacity;
  char** strings;
} StringArray;

inline void stringArrayNew(StringArray* a) {
  a->capacity = 16;
  a->size = 0;
  a->strings = malloc(a->capacity * sizeof(*a->strings));
}

inline uint32_t stringArrayAdd(StringArray* a, char* str) {
  assert(a->capacity > 0);
  a->size++;
  if (a->size > a->capacity) {
    a->capacity = a->capacity * 2;
    a->strings = realloc(a->strings, a->capacity * sizeof(*a->strings));
  }

  uint32_t index = a->size - 1;
  a->strings[index] = str;
  return index;
}

inline const char* stringArrayGet(StringArray* s, int i) {
  assert(i < s->size);
  return s->strings[i];
}

inline void stringArrayFree(StringArray* a) {
  for (int i = 0; i < a->size; ++i) {
    free(a->strings[i]);
    a->strings[i] = NULL;
  }
  free(a->strings);
  a->strings = NULL;
  a->capacity = 0;
  a->size = 0;
}

inline uint32_t stringArrayAddCopy(StringArray* a, const char* str, int length) {
  char* newStr = malloc(length + 1);
  newStr[length] = 0;
  memcpy(newStr, str, length);
  return stringArrayAdd(a, newStr);
}

typedef struct GrowableString {
  int size;
  int capacity;
  char* data;
} GrowableString;

inline void growableStringNew(GrowableString* s) {
  s->capacity = 16;
  s->size = 0;
  s->data = malloc(s->capacity);
}

inline void growableStringFree(GrowableString* s) {
  free(s->data);
  s->data = NULL;
  s->capacity = 0;
  s->size = 0;
}

inline void growableStringAppend(GrowableString* s, char c) {
  assert(s->capacity > 0);
  s->size++;
  if (s->size + 1 > s->capacity) {
    s->capacity = s->capacity * 2;
    s->data = realloc(s->data, s->capacity);
  }
  s->data[s->size - 1] = c;
  s->data[s->size] = 0;
}

#endif