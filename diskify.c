#include <assert.h>
#include <windows.h>
#include <stdio.h>
#include <stdint.h>
#include <conio.h>

#include "common.h"
#include "csvParse.h"
#include "stringSet.h"

const char* USAGE =
  "usage: diskify [-o output-file] <procmon-csv>\n";

typedef struct {
  uint64_t vcn;
  uint64_t lcn;
} MAPPING_PAIR;

typedef struct {
  uint32_t numPairs;
  int64_t startVcn;
  MAPPING_PAIR pair[1];
} GET_RETRIEVAL_DESCRIPTOR;

typedef struct _IO_STATUS_BLOCK {
  union {
    NTSTATUS status;
    PVOID    pointer;
  } DUMMYUNIONNAME;
  ULONG information;
} IO_STATUS_BLOCK, *PIO_STATUS_BLOCK;

#define STATUS_SUCCESS                   ((NTSTATUS)0x00000000L)
#define STATUS_BUFFER_OVERFLOW           ((NTSTATUS)0x80000005L)
#define STATUS_BUFFER_TOO_SMALL          ((NTSTATUS)0xC0000023L)
#define STATUS_ALREADY_COMMITTED         ((NTSTATUS)0xC0000021L)
#define STATUS_INVALID_DEVICE_REQUEST    ((NTSTATUS)0xC0000010L)
#define LLINVALID                        ((ULONGLONG) -1)
#define DEFAULT_OUTPUT_FILE              "output.diskify"

typedef VOID (*PIO_APC_ROUTINE) (
  PVOID apcContext,
  PIO_STATUS_BLOCK ioStatusBlock,
  ULONG reserved
);

NTSTATUS (__stdcall *NtFsControlFile)( 
  HANDLE fileHandle,
  HANDLE event,
  PIO_APC_ROUTINE apcRoutine,
  PVOID apcContext,
  PIO_STATUS_BLOCK ioStatusBlock, 
  ULONG fsControlCode,
  PVOID inputBuffer,
  ULONG inputBufferLength,
  PVOID outputBuffer,
  ULONG outputBufferLength
);

#define FILE_MAP_SIZE ((512 * 1024 + 2) * 8)
char gFileMapBuffer[FILE_MAP_SIZE];

Status readEntireFile(const char* filePath, const char** outData, int64_t* outSize) {
  HANDLE file = CreateFile(filePath,
                           GENERIC_READ,
                           FILE_SHARE_READ|FILE_SHARE_WRITE,
                           NULL,
                           OPEN_EXISTING,
                           0,
                           0);
  if (file == INVALID_HANDLE_VALUE) {
    fprintf(stderr, "CreateFile error opening %s: %lu\n", filePath, GetLastError());
    return Status_FAILED;
  }

  DWORD fileSize = GetFileSize(file, NULL);
  char* buffer = malloc(fileSize + 1);
  buffer[fileSize] = 0;

  DWORD totalRead = 0;
  while (totalRead < fileSize) {
    DWORD read = 0;
    if (!ReadFile(file,
                  buffer + totalRead,
                  fileSize - totalRead,
                  &read,
                  NULL)) {
      fprintf(stderr, "ReadFile error reading %s: %lu\n", filePath, GetLastError());
      free(buffer);
      CloseHandle(file);
      return Status_FAILED;
    }
    totalRead += read;
  }

  CloseHandle(file);

  *outData = buffer;
  *outSize = fileSize;
  return Status_OK;
}

Status getPathsFromCSV(const char* filePath, StringArray* outArray) {
  int64_t csvSize;
  const char* csvData;
  Status result = readEntireFile(filePath, &csvData, &csvSize);
  if (result != Status_OK) {
    return result;
  }
  CsvParseState parsed;
  result = csvParse(&parsed, csvData, csvSize);
  if (result != Status_OK) {
    return result;
  }

  int pathColumn = csvGetColumnIndex(&parsed, "Path");
  int opColumn = csvGetColumnIndex(&parsed, "Operation");
  int numRows = csvGetNumRows(&parsed);
  StringArray stringArray;
  stringArrayNew(&stringArray);

  for (int i = 0; i < numRows; ++i) {
    const char* cell;
    int cellLength;

    result = csvGetCell(&parsed, i, opColumn, &cell, &cellLength);
    if (result != Status_OK) {
      csvFree(&parsed);
      stringArrayFree(&stringArray);
      return result;
    }

    if (strncmp("ReadFile", cell, cellLength) == 0 ||
        strncmp("WriteFile", cell, cellLength) == 0 ||
        strncmp("CreateFile", cell, cellLength) == 0) {
      result = csvGetCell(&parsed, i, pathColumn, &cell, &cellLength);
      if (result != Status_OK) {
        csvFree(&parsed);
        stringArrayFree(&stringArray);
        return result;
      }

      stringArrayAddCopy(&stringArray, cell, cellLength);
    }
  }

  *outArray = stringArray;
  return Status_OK;
}

int main(int argc, char** argv) {
  if (argc < 2) {
    fprintf(stderr, "%s\n", USAGE);
    return 1;
  }

  const char* outputFile = NULL;
  char* csvPath = NULL;

  if (strcmp(argv[1], "-o") == 0) {
    if (argc != 4) {
      fprintf(stderr, "%s\n", USAGE);
      return 1;
    }
    outputFile = argv[2];
    csvPath = argv[3];
  } else {
    if (argc != 2) {
      fprintf(stderr, "%s\n", USAGE);
      return 1;
    }

    outputFile = DEFAULT_OUTPUT_FILE;
    csvPath = argv[1];
  }

  StringArray paths;
  Status result = getPathsFromCSV(csvPath, &paths);
  if (result != Status_OK) {
    return 1;
  }

  StringSet pathsSet;
  stringSetNew(&pathsSet);

  for (int i = 0; i < paths.size; ++i) {
    int added = stringSetAdd(&pathsSet, stringArrayGet(&paths, i));
  }

  NtFsControlFile = (void*)GetProcAddress(GetModuleHandle("ntdll.dll"), "NtFsControlFile");
  if(!NtFsControlFile) {
    printf("Failed finding NtFsControlFile\n");
    return 1;
  }

  HANDLE volume = CreateFile("\\\\.\\C:",
                             GENERIC_READ,
                             FILE_SHARE_READ|FILE_SHARE_WRITE,
                             NULL,
                             OPEN_EXISTING,
                             0,
                             0);
  if (volume == INVALID_HANDLE_VALUE) {
    fprintf(stderr, "CreateFile error: %lu\n", GetLastError());
    return 1;
  }

  FILE* output;
  errno_t err = fopen_s(&output, outputFile, "w");
  if (err) {
    fprintf(stderr, "fopen error: %d\n", err);
    return 1;
  }

  for (int i = 0; i < pathsSet.strings.size; ++i) {
    const char* filePath = stringSetGetString(&pathsSet, i);

    HANDLE file = CreateFile(filePath,
                             GENERIC_READ,
                             FILE_SHARE_READ|FILE_SHARE_WRITE,
                             NULL,
                             OPEN_EXISTING,
                             FILE_FLAG_NO_BUFFERING|FILE_FLAG_BACKUP_SEMANTICS,
                             0);
    if (file == INVALID_HANDLE_VALUE) {
      fprintf(output, "%s\n  BAD %lu\n", filePath, GetLastError());
      continue;
    } else {
      fprintf(output, "%s\n  OK\n", filePath);
    }

    IO_STATUS_BLOCK ioStatus;
    uint64_t startVcn = 0;
    GET_RETRIEVAL_DESCRIPTOR* fileMappings = (GET_RETRIEVAL_DESCRIPTOR*)&gFileMapBuffer;
    NTSTATUS status = NtFsControlFile(file,
                                      NULL,
                                      NULL,
                                      0,
                                      &ioStatus,
                                      FSCTL_GET_RETRIEVAL_POINTERS,
                                      &startVcn, sizeof(startVcn),
                                      fileMappings, FILE_MAP_SIZE);

    uint64_t totalLen = 0;
    while(status == STATUS_SUCCESS ||
          status == STATUS_BUFFER_OVERFLOW ||
          status == STATUS_PENDING) {

      if(status == STATUS_PENDING) {
        WaitForSingleObject(file, INFINITE); 
        if(ioStatus.status != STATUS_SUCCESS && 
           ioStatus.status != STATUS_BUFFER_OVERFLOW ) {
          fprintf(stderr, "Error enumerating file clusters for %s: %lu\n",
                  filePath, ioStatus.status);
          break;
        }
      }

      startVcn = fileMappings->startVcn;
      for(uint32_t i = 0; i < fileMappings->numPairs; i++) {
        uint64_t len = 0;
        len = fileMappings->pair[i].vcn - startVcn;
        if (fileMappings->pair[i].lcn == LLINVALID) {
          fprintf(output, "    -1,%llu\n", len);
        } else {
          fprintf(output, "    %llu,%llu\n", fileMappings->pair[i].lcn, len);
        }
        totalLen += len;
        startVcn = fileMappings->pair[i].vcn;
      }

      if (status == STATUS_SUCCESS) {
        break;
      }
    }

    CloseHandle(file);
  }

  return 0;
}