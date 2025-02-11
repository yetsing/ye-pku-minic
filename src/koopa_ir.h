#ifndef SRC_CODEGEN_H_
#define SRC_CODEGEN_H_

#include "ast.h"

void koopa_ir_codegen(AstCompUnit *comp_unit, const char *output_file);

#endif // SRC_CODEGEN_H_