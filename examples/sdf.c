#include <assert.h>
#include <math.h>
#include <stdio.h>

#define putch putchar
void putint(int n) { printf("%d", n); }

const int positive = 0;
const int negative = 1;

// #region Fixed-point number operation

// Fixed-point number representation
// fp[0] = integer part
// fp[1] = fractional part (out of SCALE)
// fp[2] = sign (0 for positive, 1 for negative)
// #define SCALE 10000 // 4 decimal places of precision
const int SCALE = 10000; // 4 decimal places of precision

// Print a fixed-point number
void fp_print(int a[]) {
  printf("%s%d.%04d", a[2] == 0 ? "" : "-", a[0], a[1]);
  // putint(a[0]);
  // putch(46); // '.'
  // putint(a[1]);
}

// Add two fixed-point numbers
void fp_add(int a[], int b[], int result[]) {
  int temp[3];
  if (a[2] == b[2]) {
    // both positive or both negative
    result[2] = a[2];
    result[1] = a[1] + b[1];
    result[0] = a[0] + b[0];
    if (result[1] >= SCALE) {
      result[0] = result[0] + 1;
      result[1] = result[1] - SCALE;
    }
    return;
  }

  if (b[2] == positive) {
    // swap a and b, let a be the positive number, b be the negative number
    temp[0] = a[0];
    temp[1] = a[1];
    temp[2] = a[2];
    a[0] = b[0];
    a[1] = b[1];
    a[2] = b[2];
    b[0] = temp[0];
    b[1] = temp[1];
    b[2] = temp[2];
  }
  // a is positive, b is negative
  if (a[0] > b[0] || (a[0] == b[0] && a[1] >= b[1])) {
    // a is greater than b
    result[2] = positive;
    result[0] = a[0] - b[0];
    result[1] = a[1] - b[1];
    if (result[1] < 0) {
      result[0] = result[0] - 1;
      result[1] = result[1] + SCALE;
    }
  } else {
    result[2] = negative;
    result[0] = b[0] - a[0];
    result[1] = b[1] - a[1];
    if (result[1] < 0) {
      result[0] = result[0] - 1;
      result[1] = result[1] + SCALE;
    }
  }
}

// Subtract b from a
void fp_sub(int a[], int b[], int result[]) {
  if (a[2] == positive && a[2] == b[2]) {
    // both positive
    if (a[0] > b[0] || (a[0] == b[0] && a[1] >= b[1])) {
      // a is greater than b
      result[2] = positive;
      result[0] = a[0] - b[0];
      result[1] = a[1] - b[1];
      if (result[1] < 0) {
        result[0] = result[0] - 1;
        result[1] = result[1] + SCALE;
      }
    } else {
      result[2] = negative;
      result[0] = b[0] - a[0];
      result[1] = b[1] - a[1];
      if (result[1] < 0) {
        result[0] = result[0] - 1;
        result[1] = result[1] + SCALE;
      }
    }
    return;
  }

  if (a[2] == negative && a[2] == b[2]) {
    // both negative
    if (a[0] > b[0] || (a[0] == b[0] && a[1] >= b[1])) {
      // a is less than b
      result[2] = negative;
      result[0] = a[0] - b[0];
      result[1] = a[1] - b[1];
      if (result[1] < 0) {
        result[0] = result[0] - 1;
        result[1] = result[1] + SCALE;
      }
    } else {
      result[2] = positive;
      result[0] = b[0] - a[0];
      result[1] = b[1] - a[1];
      if (result[1] < 0) {
        result[0] = result[0] - 1;
        result[1] = result[1] + SCALE;
      }
    }
    return;
  }

  result[2] = a[2];
  result[0] = a[0] + b[0];
  result[1] = a[1] + b[1];
  if (result[1] >= SCALE) {
    result[0] = result[0] + 1;
    result[1] = result[1] - SCALE;
  }
  // printf("    ");
  // fp_print(a);
  // printf(" - ");
  // fp_print(b);
  // printf(" = ");
  // fp_print(result);
  // printf("\n");
  return;
}

int mod_mult(int a, int b, int mod) {
  int result = 0;
  while (b > 0) {
    if ((b % 2) == 1) {
      result = (result + a) % mod;
    }
    a = (a * 2) % mod;
    b = b / 2;
  }
  return result;
}

// Multiply two fixed-point numbers
void fp_mul(int a[], int b[], int result[]) {
  int part1 = a[0] * b[0];
  int int_part2 = 0;
  int frac_part2 = 0;
  int part2 = a[0] * b[1];
  // 214769 = (2 ** 32 - 1) / 9999
  if (a[0] > 214769) {
    // handle overflow
    int_part2 = a[0] / SCALE * b[1];
    frac_part2 = mod_mult(a[0], b[1], SCALE);
  } else {
    int_part2 = part2 / SCALE;
    frac_part2 = part2 % SCALE;
  }

  int int_part3 = 0;
  int frac_part3 = 0;
  int part3 = a[1] * b[0];
  if (b[0] > 214769) {
    // handle overflow
    int_part3 = b[0] / SCALE * a[1];
    frac_part3 = mod_mult(b[0], a[1], SCALE);
  } else {
    int_part3 = part3 / SCALE;
    frac_part3 = part3 % SCALE;
  }
  int part4 = a[1] * b[1] / SCALE;

  result[0] = part1 + int_part2 + int_part3;
  result[1] = frac_part2 + frac_part3 + part4;
  if (result[1] >= SCALE) {
    result[0] = result[0] + (result[1] / SCALE);
    result[1] = result[1] % SCALE;
  }

  if (a[2] == b[2]) {
    result[2] = positive;
  } else {
    result[2] = negative;
  }
}

// Divide two fixed-point numbers
void fp_div(int a[], int b[], int result[]) {
  if (a[2] == b[2]) {
    result[2] = positive;
  } else {
    result[2] = negative;
  }
  if (b[0] == 0 && b[1] == 0) {
    result[0] = 0;
    result[1] = 0;
    return;
  }
  if (b[0] == 0) {
    result[0] = a[0] * SCALE / b[1];
    result[1] = (a[0] % b[1]) * SCALE + a[1];
    if (result[1] >= SCALE) {
      result[0] = result[0] + (result[1] / SCALE);
      result[1] = result[1] % SCALE;
    }
    return;
  }

  // use absolute value for calculation
  int ta[3], tb[3];
  ta[0] = a[0];
  ta[1] = a[1];
  ta[2] = positive;
  tb[0] = b[0];
  tb[1] = b[1];
  tb[2] = positive;

  int int_part = ta[0] / (tb[0] + 1);
  int temp1[3], remaining[3];
  temp1[0] = tb[0] * int_part;
  temp1[1] = tb[1] * int_part;
  temp1[2] = positive;
  if (temp1[1] >= SCALE) {
    temp1[0] = temp1[0] + (temp1[1] / SCALE);
    temp1[1] = temp1[1] % SCALE;
  }
  fp_sub(ta, temp1, remaining);

  while (remaining[0] > tb[0] ||
         (remaining[0] == tb[0] && remaining[1] >= tb[1])) {
    int_part = int_part + 1;
    fp_sub(remaining, tb, temp1);
    remaining[0] = temp1[0];
    remaining[1] = temp1[1];
    remaining[2] = temp1[2];
  }

  result[0] = int_part;

  int frac_part = 0;
  int i = 0;
  int base[3] = {10, 0, positive};
  while (i < 4) {
    i = i + 1;
    fp_mul(remaining, base, temp1);
    remaining[0] = temp1[0];
    remaining[1] = temp1[1];
    remaining[2] = temp1[2];
    frac_part = frac_part * 10;
    while (remaining[0] > tb[0] ||
           (remaining[0] == tb[0] && remaining[1] >= b[1])) {
      frac_part = frac_part + 1;
      fp_sub(remaining, tb, temp1);
      remaining[0] = temp1[0];
      remaining[1] = temp1[1];
      remaining[2] = temp1[2];
    }
  }
  result[1] = frac_part;
}

/* Calculates x^y where x is fixed-point and y is a regular integer */
void fp_pow(int x[], int y, int result[]) {
  // Special cases
  if (x[0] == 0 && x[1] == 0) {
    // 0^y = 0, except 0^0 which is undefined (returning 1 in this case)
    if (y == 0) {
      result[0] = 1;
      result[1] = 0;
    } else {
      result[0] = 0;
      result[1] = 0;
    }
    return;
  }

  if (y == 0) {
    // Anything^0 = 1
    result[0] = 1;
    result[1] = 0;
    return;
  }

  // For positive exponents: Exponentiation by squaring algorithm
  result[0] = 1;
  result[1] = 0;

  int base[3] = {x[0], x[1], positive};
  int power = y;
  int temp[3];

  while (power > 0) {
    if (power % 2 == 1) {
      // If power is odd, multiply result by base
      fp_mul(result, base, temp);
      result[0] = temp[0];
      result[1] = temp[1];
    }

    // Square the base
    fp_mul(base, base, temp);
    base[0] = temp[0];
    base[1] = temp[1];

    power = power / 2;
  }

  if (y % 2 == 0) {
    result[2] = positive;
  } else {
    result[2] = x[2];
  }
}

/* Calculate square root of a fixed-point number using Newton-Raphson method */
void fp_sqrt(int x[], int result[]) {
  result[2] = positive;
  // Handle special cases
  if (x[0] < 0 || (x[0] == 0 && x[1] < 0)) {
    // Cannot take square root of negative number
    // Setting result to 0 to indicate error
    result[0] = 0;
    result[1] = 0;
    return;
  }

  if (x[0] == 0 && x[1] == 0) {
    // sqrt(0) = 0
    result[0] = 0;
    result[1] = 0;
    return;
  }

  // Initial guess: a reasonable approximation to sqrt(x)
  // For simplicity, we can use x/2 if x > 1, or x if x < 1
  int guess[3] = {x[0], x[1], positive};
  int two[3] = {2, 0, positive};
  int n_exp = 10;
  if (x[0] > SCALE) {
    while ((n_exp * n_exp <= x[0]) && (n_exp < 100000)) {
      n_exp = 10 * n_exp;
    }
    guess[0] = n_exp;
    guess[1] = 0;
  } else if (x[0] > 1 || (x[0] == 1 && x[1] > 0)) {
    // x > 1: start with x/2
    fp_div(x, two, guess);
  }

  // Newton-Raphson iteration: next = (guess + x/guess) / 2
  // We'll perform several iterations for accuracy
  int temp1[3], temp2[3], next[3];

  // printf("  fp_sqrt  ");
  // fp_print(x);
  // printf("\n");
  // printf("    init = ");
  // fp_print(guess);
  // printf("\n");

  // Usually 5-10 iterations is enough for good precision
  int i = 0;
  while (i < 10) {
    i = i + 1;
    // Calculate x/guess
    fp_div(x, guess, temp1);

    // Calculate guess + x/guess
    fp_add(guess, temp1, temp2);

    // Calculate (guess + x/guess)/2
    // int two[2] = {2, 0};
    // fp_div(temp2, two, next);
    int two[3] = {0, 5000, positive};
    fp_mul(temp2, two, next);

    // Update guess for next iteration
    guess[0] = next[0];
    guess[1] = next[1];
    guess[2] = next[2];

    // printf("    iteration %d\n", i);
    // printf("      ");
    // fp_print(guess);
    // printf("\n");
  }

  result[0] = guess[0];
  result[1] = guess[1];
}

// Define a function to compare two fixed-point numbers
int fp_cmp(int a[], int b[]) {
  if (a[0] < b[0]) {
    return -1;
  } else if (a[0] > b[0]) {
    return 1;
  } else {
    if (a[1] < b[1]) {
      return -1;
    } else if (a[1] > b[1]) {
      return 1;
    } else {
      return 0;
    }
  }
}

/* Calculate sine of a fixed-point number using Taylor series approximation */
void fp_sin(int x[], int result[]) {
  // printf("\n  fp_sin ");
  // fp_print(x);
  // printf("\n");
  // Normalize angle to [-π, π] range
  // First, create a 2π constant
  int two_pi[3] = {6, 2832, positive}; // 6.2832 is approximately 2π

  // Create temporary variables
  int temp1[3], temp2[3], normalized[3];

  // Copy input to normalized
  normalized[0] = x[0];
  normalized[1] = x[1];
  normalized[2] = x[2];

  // If abs(x) > π, subtract 2π until abs(x) <= π
  int pi[2] = {3, 1416}; // 3.1416 is approximately π
  while (normalized[0] > pi[0] ||
         (normalized[0] == pi[0] && normalized[1] > pi[1])) {
    fp_sub(normalized, two_pi, temp1);
    normalized[0] = temp1[0];
    normalized[1] = temp1[1];
    normalized[2] = temp1[2];
  }

  // printf("  ");
  // fp_print(x);
  // printf(" normalized to ");
  // fp_print(normalized);
  // printf("\n");

  // Taylor series for sin(x) = x - x^3/3! + x^5/5! - x^7/7! + ...
  // We'll use the first 4 terms of the series for a reasonable approximation

  // First term: x
  result[0] = normalized[0];
  result[1] = normalized[1];
  result[2] = normalized[2];

  // x^3/3!
  int x3[3], term[3];
  fp_pow(normalized, 3, x3);
  // printf("  x^3 = ");
  // fp_print(x3);
  // printf("\n");
  int factorial3[3] = {6, 0, positive}; // 3! = 6
  fp_div(x3, factorial3, term);
  // printf("  x^3/3! = ");
  // fp_print(term);
  // printf("\n");
  fp_sub(result, term, temp1);
  // printf("  x - x^3/3! = ");
  // fp_print(temp1);
  // printf("\n");

  // x^5/5!
  int x5[3];
  fp_pow(normalized, 5, x5);
  // printf("  x^5 = ");
  // fp_print(x5);
  // printf("\n");
  int factorial5[3] = {120, 0, positive}; // 5! = 120
  fp_div(x5, factorial5, term);
  // printf("  x^5/5! = ");
  // fp_print(term);
  // printf("\n");
  fp_add(temp1, term, temp2);
  // printf("  x - x^3/3! + x^5/5! = ");
  // fp_print(temp2);
  // printf("\n");

  // x^7/7!
  int x7[3];
  fp_pow(normalized, 7, x7);
  // printf("  x^7 = ");
  // fp_print(x7);
  // printf("\n");
  int factorial7[3] = {5040, 0, positive}; // 7! = 5040
  fp_div(x7, factorial7, term);
  // printf("  x^7/7! = ");
  // fp_print(term);
  // printf("\n");
  fp_sub(temp2, term, result);
  // printf("  x - x^3/3! + x^5/5! - x^7/7! = ");
  // fp_print(result);
  // printf("\n");
}

/* Calculate cosine of a fixed-point number using Taylor series approximation */
void fp_cos(int x[], int result[]) {
  // cos(x) = sin(x + π/2)
  int pi_over_2[3] = {1, 5708, positive}; // 1.5708 is approximately π/2
  int temp[3];
  fp_add(x, pi_over_2, temp);
  fp_sin(temp, result);
}

// Initialize a fixed-point number
void fp_init(int a[], int whole, int frac) {
  a[0] = whole;
  a[1] = frac;
  a[2] = positive;
}

// #endregion

void donut(int x[], int y[], int z[], int result[]) {
  int radius[3] = {0, 4000, positive};
  int thickness[3] = {0, 1500, positive};
  int temp1[3];
  int temp2[3];

  // math.sqrt(x**2 + y**2)
  fp_mul(x, x, temp1);
  fp_mul(y, y, temp2);
  fp_add(temp1, temp2, result);
  fp_sqrt(result, temp1);
  // printf("math.sqrt(x**2 + y**2) = ");
  // fp_print(temp1);
  // printf("\n");
  // (t - radius) ** 2
  fp_sub(temp1, radius, result);
  temp1[0] = result[0];
  temp1[1] = result[1];
  fp_mul(temp1, temp1, result);
  temp1[0] = result[0];
  temp1[1] = result[1];
  // printf("(t - radius) ** 2 = ");
  // fp_print(temp1);
  // printf("\n");
  // math.sqrt(t + z**2)
  fp_mul(z, z, temp2);
  // printf("z ** 2 = ");
  // fp_print(temp2);
  // printf("\n");
  fp_add(temp1, temp2, result);
  // printf("t + z ** 2 = ");
  // fp_print(result);
  // printf("\n");
  fp_sqrt(result, temp1);
  // printf("math.sqrt(t + z**2) = ");
  // fp_print(temp1);
  // printf("\n");

  fp_sub(temp1, thickness, result);
}

// Test fixed-point arithmetic
void test_base() {
  int a[3], b[3], c[3];

  // Initialize a and b
  float an = 1.50;
  float bn = 2.25;
  fp_init(a, 1, 5 * SCALE / 10);
  fp_init(b, 2, 25 * SCALE / 100);

  // Add a and b
  fp_add(a, b, c);
  fp_print(a);
  // printf(" + ");
  putch(32); // ' '
  putch(43); // '+'
  putch(32); // ' '
  fp_print(b);
  // printf(" = ");
  putch(32); // ' '
  putch(61); // '='
  putch(32); // ' '
  fp_print(c);
  printf(" %f\n", an + bn);
  putch(10); // '\n'

  // Subtract b from a
  fp_sub(a, b, c);
  fp_print(a);
  // printf(" - ");
  putch(32); // ' '
  putch(45); // '-'
  putch(32); // ' '
  fp_print(b);
  // printf(" = ");
  putch(32); // ' '
  putch(61); // '='
  putch(32); // ' '
  fp_print(c);
  printf(" %f\n", an - bn);
  putch(10); // '\n'

  // Multiply a and b
  fp_mul(a, b, c);
  fp_print(a);
  // printf(" * ");
  putch(32); // ' '
  putch(42); // '*'
  putch(32); // ' '
  fp_print(b);
  // printf(" = ");
  putch(32); // ' '
  putch(61); // '='
  putch(32); // ' '
  fp_print(c);
  printf(" %f\n", an * bn);
  putch(10); // '\n'

  // Divide a by b
  fp_div(a, b, c);
  fp_print(a);
  // printf(" / ");
  putch(32); // ' '
  putch(47); // '/'
  putch(32); // ' '
  fp_print(b);
  // printf(" = ");
  putch(32); // ' '
  putch(61); // '='
  putch(32); // ' '
  fp_print(c);
  printf(" %f\n", an / bn);
  putch(10); // '\n'

  // Calculate a^3
  fp_pow(a, 3, c);
  fp_print(a);
  // printf(" ^ 3 = ");
  putch(32); // ' '
  putch(94); // '^'
  putch(32); // ' '
  putch(51); // '3'
  putch(32); // ' '
  putch(61); // '='
  putch(32); // ' '
  fp_print(c);
  printf(" %f\n", pow(an, 3));
  putch(10); // '\n'

  // Calculate sqrt(a)
  fp_sqrt(a, c);
  // printf("sqrt(");
  putch(115); // 's'
  putch(113); // 'q'
  putch(114); // 'r'
  putch(116); // 't'
  putch(40);  // '('
  fp_print(a);
  // printf(") = ");
  putch(41); // ')'
  putch(32); // ' '
  putch(61); // '='
  putch(32); // ' '
  fp_print(c);
  printf(" %f\n", sqrt(an));
  putch(10); // '\n'

  // Test trigonometric functions
  // printf("\n--- Testing Trigonometric Functions ---\n");
  putch(10); // '\n'
  putch(10); // '\n'

  // Test sine function
  int angle[2];
  fp_init(angle, 1, 5708); // Approximately π/2 (1.5708)
  fp_sin(angle, c);
  // printf("sin(");
  putch(115); // 's'
  putch(105); // 'i'
  putch(110); // 'n'
  putch(40);  // '('
  fp_print(angle);
  // printf(") = ");
  putch(41); // ')'
  putch(32); // ' '
  putch(61); // '='
  putch(32); // ' '
  fp_print(c);
  printf(" (should be close to 1.0000)\n");
  putch(10); // '\n'

  // Test cosine function
  fp_cos(angle, c);
  // printf("cos(");
  putch(99);  // 'c'
  putch(111); // 'o'
  putch(115); // 's'
  putch(40);  // '('
  fp_print(angle);
  // printf(") = ");
  putch(41); // ')'
  putch(32); // ' '
  putch(61); // '='
  putch(32); // ' '
  fp_print(c);
  printf(" (should be close to 0.0000)\n");
  putch(10); // '\n'

  // Test zero angle
  fp_init(angle, 0, 0);
  fp_sin(angle, c);
  // printf("sin(");
  putch(115); // 's'
  putch(105); // 'i'
  putch(110); // 'n'
  putch(40);  // '('
  fp_print(angle);
  // printf(") = ");
  putch(41); // ')'
  putch(32); // ' '
  putch(61); // '='
  putch(32); // ' '
  fp_print(c);
  printf(" (should be 0.0000)\n");
  putch(10); // '\n'

  fp_cos(angle, c);
  // printf("cos(");
  putch(99);  // 'c'
  putch(111); // 'o'
  putch(115); // 's'
  putch(40);  // '('
  fp_print(angle);
  // printf(") = ");
  putch(41); // ')'
  putch(32); // ' '
  putch(61); // '='
  putch(32); // ' '
  fp_print(c);
  printf(" (should be 1.0000)\n");
  putch(10); // '\n'

  // Test pi
  fp_init(angle, 3, 1416); // Approximately π
  fp_sin(angle, c);
  // printf("sin(");
  putch(115); // 's'
  putch(105); // 'i'
  putch(110); // 'n'
  putch(40);  // '('
  fp_print(angle);
  // printf(") = ");
  putch(41); // ')'
  putch(32); // ' '
  putch(61); // '='
  putch(32); // ' '
  fp_print(c);
  printf(" (should be close to 0.0000)\n");
  putch(10); // '\n'

  fp_cos(angle, c);
  // printf("cos(");
  putch(99);  // 'c'
  putch(111); // 'o'
  putch(115); // 's'
  putch(40);  // '('
  fp_print(angle);
  // printf(") = ");
  putch(41); // ')'
  putch(32); // ' '
  putch(61); // '='
  putch(32); // ' '
  fp_print(c);
  printf(" (should be close to -1.0000)\n");
  putch(10); // '\n'
}

void test_donut() {
  struct {
    int x[3];
    int y[3];
    int z[3];
    int want[3];
  } testcases[] = {

      {
          {66503, 8449, 1},
          {0, 5000, 1},
          {30437, 867, 1},
          {73137, 5587, 0},
      },
  };
  int got[2];
  size_t n = sizeof(testcases) / sizeof(testcases[0]);
  for (size_t i = 0; i < n; i++) {
    donut(testcases[i].x, testcases[i].y, testcases[i].z, got);
    printf("donut(");
    fp_print(testcases[i].x);
    printf(", ");
    fp_print(testcases[i].y);
    printf(", ");
    fp_print(testcases[i].z);
    printf(") = ");
    fp_print(got);
    printf(" (");
    fp_print(testcases[i].want);
    printf(")\n");
  }
}

int main() {
  // test_base();
  test_donut();
  return 0;
}