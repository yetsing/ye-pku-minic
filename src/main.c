#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "codegen.h"
#include "parse.h"

// handle cli arguments like "-koopa 输入文件 -o 输出文件"
void handle_cli_arguments(int argc, char *argv[], char **input_file,
                          char **output_file) {
  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "-koopa") == 0 && i + 1 < argc) {
      *input_file = argv[i + 1];
      i++;
    } else if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
      *output_file = argv[i + 1];
      i++;
    }
  }
}

static char *read_from_file(const char *filename) {
  FILE *file = fopen(filename, "r");
  if (file == NULL) {
    fprintf(stderr, "无法打开文件 %s\n", filename);
    exit(1);
  }

  fseek(file, 0, SEEK_END);
  long length = ftell(file);
  fseek(file, 0, SEEK_SET);

  char *buffer = malloc(length + 1);
  if (buffer == NULL) {
    fprintf(stderr, "无法分配内存\n");
    exit(1);
  }

  fread(buffer, 1, length, file);
  buffer[length] = '\0';

  fclose(file);
  return buffer;
}

int main(int argc, char *argv[]) {
  char *input_file = NULL;
  char *output_file = NULL;

  handle_cli_arguments(argc, argv, &input_file, &output_file);

  if (input_file == NULL || output_file == NULL) {
    printf("Usage: %s -koopa <input_file> -o <output_file>\n", argv[0]);
    exit(1);
  }

  const char *input = read_from_file(input_file);
  AstCompUnit *comp_unit = parse(input);
  // comp_unit->base.dump((AstBase *)comp_unit, 0);
  codegen(comp_unit, output_file);
  return 0;
}