#ifndef CSV_PARSE_H
#define CSV_PARSE_H

#include <stdint.h>

#include "common.h"

typedef struct CsvParseState {
  StringArray strings;
  int numRows;
  int numColumns;
} CsvParseState;

// Parses a CSV - assumes that the first row of the CSV are the column names
Status csvParse(CsvParseState* state, const char* csvData, int64_t csvSize);
int csvGetColumnIndex(CsvParseState* state, const char* column);
int csvGetNumRows(CsvParseState* state);
Status csvGetCell(CsvParseState* state,
                  int row,
                  int column,
                  const char** cell,
                  int* cellLength);
void csvFree(CsvParseState* state);

#endif