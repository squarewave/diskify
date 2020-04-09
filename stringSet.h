#ifndef STRING_HASH_SET_H
#define STRING_HASH_SET_H

#include <stdbool.h>

#include "common.h"

// Fibonacci hashing, per Knuth
#define GOLDEN_RATIO_U32 0x9E3779B9U

typedef struct StringSet {
  StringArray strings;
  uint32_t* indicesTable;
  int size;
  int capacity;
} StringSet;

inline void stringSetNew(StringSet* s) {
  stringArrayNew(&s->strings);
  s->size = 0;
  s->capacity = 16;
  s->indicesTable = malloc(s->capacity * sizeof(*s->indicesTable));
  memset(s->indicesTable, 0, s->capacity * sizeof(*s->indicesTable));
}

inline uint32_t rotateLeft(uint32_t x, uint32_t amount) {
  return (x << amount) | (x >> (32 - amount));
}

inline uint32_t _stringSetHashString(const char* str) {
  uint32_t h = 0;
  while (*str) {
    uint32_t asU32 = *str;
    h = GOLDEN_RATIO_U32 * rotateLeft(h, 11) ^ asU32; 
    ++str;
  }
  return h;
}

inline bool _stringSetTakeSlot(StringSet* s,
                                  uint32_t hash,
                                  const char* str,
                                  uint32_t* outIndex) {
  uint32_t capacityMask = s->capacity - 1;
  uint32_t index = hash & capacityMask;
  for (int i = 0; i < s->capacity; ++i) {
    if (!s->indicesTable[index]) {
      *outIndex = index;
      return false;
    }
    if (!strcmp(str,
                stringArrayGet(&s->strings, s->indicesTable[index] - 1))) {
      *outIndex = index;
      return true;
    }
    index = (index + 1) & capacityMask;
  }
  assert(false);
  return true;
}

inline void _stringSetRehash(StringSet* s) {
  for (int i = 0; i < s->strings.size; ++i) {
    const char* str = stringArrayGet(&s->strings, i);
    uint32_t h = _stringSetHashString(str);
    uint32_t index;
    bool exists = _stringSetTakeSlot(s, h, str, &index);
    assert(!exists);
    s->indicesTable[index] = i + 1;
  }
}

inline uint32_t stringSetAdd(StringSet* s, const char* str) {
  if (s->size > s->capacity / 2) {
    s->capacity <<= 1;
    s->indicesTable = realloc(s->indicesTable,
                              s->capacity * sizeof(*s->indicesTable));
    memset(s->indicesTable, 0, s->capacity * sizeof(*s->indicesTable));
    _stringSetRehash(s);
  }

  uint32_t h = _stringSetHashString(str);
  uint32_t index;
  bool exists = _stringSetTakeSlot(s, h, str, &index);
  if (!exists) {
    s->size++;
    uint32_t stringIndex = stringArrayAddCopy(&s->strings, str, strlen(str));
    s->indicesTable[index] = stringIndex + 1;
  }
  return s->indicesTable[index] - 1;
}

inline const char* stringSetGetString(StringSet* s, int i) {
  return stringArrayGet(&s->strings, i);
}

#endif