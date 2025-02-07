#include <stdio.h>
#include <string.h>

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

int main(int argc, char *argv[]) {
  char *input_file = NULL;
  char *output_file = NULL;

  handle_cli_arguments(argc, argv, &input_file, &output_file);

  if (input_file != NULL) {
    printf("Input file: %s\n", input_file);
  } else {
    printf("No input file provided.\n");
  }

  if (output_file != NULL) {
    printf("Output file: %s\n", output_file);
  } else {
    printf("No output file provided.\n");
  }

  printf("Hello, World!\n");
  return 0;
}