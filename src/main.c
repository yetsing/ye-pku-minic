#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "koopa_ir.h"
#include "parse.h"
#include "riscv.h"

typedef enum {
  CODEGEN_TARGET_RISCV,
  CODEGEN_TARGET_KOOPA,
} CodegenTarget;

static CodegenTarget target;

// handle cli arguments like "-koopa 输入文件 -o 输出文件"
void handle_cli_arguments(int argc, char *argv[], char **input_file,
                          char **output_file) {
  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "-koopa") == 0) {
      target = CODEGEN_TARGET_KOOPA;
    } else if (strcmp(argv[i], "-riscv") == 0) {
      target = CODEGEN_TARGET_RISCV;
    } else if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
      *output_file = argv[i + 1];
      i++;
    } else {
      *input_file = argv[i];
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
  // if program crashes, buffer will not be flushed. So disable buffering.
  setvbuf(stdout, NULL, _IONBF, 0);
  setvbuf(stderr, NULL, _IONBF, 0);

  char *input_file = NULL;
  char *output_file = NULL;

  handle_cli_arguments(argc, argv, &input_file, &output_file);

  if (input_file == NULL || output_file == NULL) {
    printf("Usage: %s -koopa <input_file> -o <output_file>\n", argv[0]);
    exit(1);
  }

  const char *input = read_from_file(input_file);
  printf("=== Input ===\n");
  printf("%s\n", input);
  AstCompUnit *comp_unit = parse(input);
  if (target == CODEGEN_TARGET_RISCV) {
    printf("=== Koopa IR codegen result ===\n");
    koopa_ir_codegen(comp_unit, output_file);
    const char *ir = read_from_file(output_file);
    printf("%s\n", ir);
    riscv_codegen(ir, output_file);
    free((void *)ir);
    const char *riscv = read_from_file(output_file);
    printf("=== RISC-V codegen result ===\n");
    printf("%s\n", riscv);
    free((void *)riscv);
  } else if (target == CODEGEN_TARGET_KOOPA) {
    printf("=== AST dump ===\n");
    comp_unit->base.dump((AstBase *)comp_unit, 0);
    printf("=== Koopa IR codegen result ===\n");
    koopa_ir_codegen(comp_unit, output_file);
    const char *ir = read_from_file(output_file);
    printf("%s\n", ir);
    free((void *)ir);
  } else {
    fprintf(stderr, "未指定目标\n");
    exit(1);
  }
  fflush(stdout);
  return 0;
}