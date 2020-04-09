#include "csvParse.h"

#include <stdio.h>

void csvParseQuoted(CsvParseState* state,
                    int64_t* i,
                    const char* csvData,
                    int64_t csvSize) {
  GrowableString cell;
  growableStringNew(&cell);
  ++*i;
  while (*i < csvSize) {
    char c = csvData[*i];
    char next = (*i + 1) < csvSize ? csvData[*i + 1] : 0;
    if (c == '"') {
      if (next == '"') {
        growableStringAppend(&cell, '"');
        *i += 2;
      } else {
        ++*i;
        break;
      }
    } else if (c == '\r') {
      ++*i;
    } else {
      growableStringAppend(&cell, c);
      ++*i;
    }
  }
  stringArrayAddCopy(&state->strings, cell.data, cell.size);
  growableStringFree(&cell);
}

void csvParseUnquoted(CsvParseState* state,
                      int64_t* i,
                      const char* csvData,
                      int64_t csvSize) {
  GrowableString cell;
  growableStringNew(&cell);
  while (*i < csvSize) {
    char c = csvData[*i];
    char next = (*i + 1) < csvSize ? csvData[*i + 1] : 0;
    if (c == ',' || c == '\n') {
      break;
    } else if (c == '\r') {
      ++*i;
    } else {
      growableStringAppend(&cell, c);
      ++*i;
    }
  }
  stringArrayAddCopy(&state->strings, cell.data, cell.size);
  growableStringFree(&cell);
}

Status csvParse(CsvParseState* state, const char* csvData, int64_t csvSize) {
  state->numRows = 0;
  state->numColumns = 0;
  stringArrayNew(&state->strings);

  int64_t i = 0;
  int columnIndex = 0;
  const uint8_t* asBytes = (const uint8_t*)csvData;
  if (csvSize >= 3 &&
      asBytes[0] == 0xef &&
      asBytes[1] == 0xbb &&
      asBytes[2] == 0xbf) {
    // Someone set us up the BOM
    i = 3;
  }

  while (i < csvSize) {
    if (csvData[i] == '\r') {
      ++i;
      continue;
    }
    char c = csvData[i];
    char next = (i + 1) < csvSize ? csvData[i + 1] : 0;
    if (c == '"') {
      csvParseQuoted(state, &i, csvData, csvSize);
    } else {
      csvParseUnquoted(state, &i, csvData, csvSize);
    }
    columnIndex++;

    if (i >= csvSize) {
      break;
    }

    if (csvData[i] == '\n') {
      if (state->numRows > 0) {
        if (columnIndex != state->numColumns) {
          fprintf(stderr,
                  "Inconsistent number of columns in CSV - failing. (row %d, total chars: %lld)\n",
                  state->numRows, i);
          csvFree(state);
          return Status_FAILED;
        }
      } else {
        state->numColumns = columnIndex;
      }
      columnIndex = 0;
      state->numRows++;
    }
    ++i;
  }

  return Status_OK;
}

int csvGetColumnIndex(CsvParseState* state, const char* column) {
  for (int i = 0; i < state->numColumns; ++i) {
    if (strcmp(stringArrayGet(&state->strings, i), column) == 0) {
      return i;
    }
  }

  return -1;
}

int csvGetNumRows(CsvParseState* state) {
  return state->numRows - 1;
}

Status csvGetCell(CsvParseState* state,
                  int row,
                  int column,
                  const char** cell,
                  int* cellLength) {
  row += 1; // strip out the csv headers.
  if (row > state->numRows || column > state->numColumns) {
    return Status_FAILED;
  }

  *cell = stringArrayGet(&state->strings, row * state->numColumns + column);
  *cellLength = strlen(*cell);
  return Status_OK;
}

void csvFree(CsvParseState* state) {
  stringArrayFree(&state->strings);
}

