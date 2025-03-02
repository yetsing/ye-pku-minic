#ifndef SRC_UTILS_H_
#define SRC_UTILS_H_

#include <stdbool.h>

#define fatalf(fmt, ...)                                                       \
  internal_fatalf("%s:%d " fmt, __FILE__, __LINE__, ##__VA_ARGS__)
void internal_fatalf(const char *fmt, ...)
    __attribute__((format(printf, 1, 2)));
#define warnf(fmt, ...) internal_warnf("Warning: " fmt, ##__VA_ARGS__)
void internal_warnf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

/**
 * @struct IntStack
 * @brief A structure to represent a stack of integers.
 *
 * This structure provides a simple implementation of a stack data structure
 * that holds integers. It includes a dynamic array to store the stack elements,
 * the current size of the stack, and the capacity of the stack.
 *
 * @var IntStack::data
 * Pointer to the dynamic array that holds the stack elements.
 *
 * @var IntStack::size
 * The current number of elements in the stack.
 *
 * @var IntStack::capacity
 * The maximum number of elements that the stack can hold before needing to
 * resize.
 */
typedef struct IntStack {
  int *data;
  int size;
  int capacity;
} IntStack;
void int_stack_init(IntStack *stack);
void int_stack_push(IntStack *stack, int v);
int int_stack_pop(IntStack *stack);
int int_stack_top(IntStack *stack);
bool int_stack_empty(IntStack *stack);

#endif // SRC_UTILS_H_