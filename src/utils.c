#include "utils.h"

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

void internal_fatalf(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  vfprintf(stderr, fmt, args);
  va_end(args);
  assert(0);
}

void internal_warnf(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  vfprintf(stderr, fmt, args);
  va_end(args);
}

void int_stack_init(IntStack *stack) {
  stack->size = 0;
  stack->capacity = 10;
  stack->data = (int *)malloc(stack->capacity * sizeof(int));
}

void int_stack_push(IntStack *stack, int v) {
  if (stack->size == stack->capacity) {
    stack->capacity *= 2;
    stack->data = (int *)realloc(stack->data, stack->capacity * sizeof(int));
  }
  stack->data[stack->size++] = v;
}

int int_stack_pop(IntStack *stack) {
  assert(stack->size > 0);
  return stack->data[--stack->size];
}

int int_stack_top(IntStack *stack) {
  assert(stack->size > 0);
  return stack->data[stack->size - 1];
}

bool int_stack_empty(IntStack *stack) { return stack->size == 0; }
