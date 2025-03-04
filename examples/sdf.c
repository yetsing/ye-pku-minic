#include <math.h>
#include <stdio.h>

// Fixed-point number representation
// fp[0] = integer part
// fp[1] = fractional part (out of SCALE)
#define SCALE 10000 // 4 decimal places of precision

// Print a fixed-point number
void fp_print(int a[2]) { printf("%d.%04d", a[0], a[1]); }

// Add two fixed-point numbers
void fp_add(int a[], int b[], int result[]) {
  result[0] = a[0] + b[0];
  result[1] = a[1] + b[1];

  // Handle carry
  if (result[1] >= SCALE) {
    result[0]++;
    result[1] -= SCALE;
  } else if (result[1] < 0) {
    result[0]--;
    result[1] += SCALE;
  }
}

// Subtract b from a
void fp_sub(int a[], int b[], int result[]) {
  if (a[0] > b[0]) {
    result[1] = a[1] - b[1];
    if (result[1] < 0) {
      result[0] = a[0] - b[0] - 1;
      result[1] += SCALE;
    } else {
      result[0] = a[0] - b[0];
    }
  } else if (a[0] == b[0]) {
    result[0] = 0;
    result[1] = a[1] - b[1];
  } else {
    result[1] = b[1] - a[1];
    if (result[1] < 0) {
      result[0] = b[0] - a[0] - 1;
      result[1] += SCALE;
    } else {
      result[0] = b[0] - a[0];
    }
    result[0] = -result[0];
    result[1] = -result[1];
  }
}

// Multiply two fixed-point numbers
void fp_mul(int a[], int b[], int result[]) {
  int part1 = a[0] * b[0];
  int part2 = a[0] * b[1];
  int part3 = a[1] * b[0];
  int part4 = a[1] * b[1] / SCALE;

  result[0] = part1 + part2 / SCALE + part3 / SCALE;
  result[1] = part2 % SCALE + part3 % SCALE + part4;
  if (result[1] >= SCALE) {
    result[0] += result[1] / SCALE;
    result[1] %= SCALE;
  }
}

// Calculate reciprocal of a fixed-point number (1/x)
// using Newton-Raphson method
void fp_recip(int x[], int result[]) {
  // printf("Calculating reciprocal of ");
  // fp_print(x);
  // printf("\n");
  // Check for division by zero
  if (x[0] == 0 && x[1] == 0) {
    result[0] = 0;
    result[1] = 0;
    return; // Cannot calculate reciprocal of zero
  }

  // Handle negative numbers separately to avoid confusion with signs
  int sign = 1;
  int x_abs[2] = {x[0], x[1]};

  if ((x[0] < 0) || (x[0] == 0 && x[1] < 0)) {
    sign = -1;
    // Make x positive
    if (x[1] != 0) {
      x_abs[0] = -x[0] - 1;
      x_abs[1] = SCALE - x[1];
    } else {
      x_abs[0] = -x[0];
    }
  }

  // Initial guess for reciprocal
  // For numbers >= 1, use 1/x_int as initial guess
  // For numbers < 1, use 1 as initial guess
  int guess[2];
  if (x_abs[0] > 1) {
    guess[0] = 0; // Integer division for initial guess
    guess[1] = SCALE;
    int tn = x_abs[0];
    while (tn > 0) {
      tn /= 10;
      guess[1] /= 10;
    }
  } else {
    guess[0] = 1;
    guess[1] = 0;
  }

  // Newton-Raphson method: r_next = r * (2 - x * r)
  // This converges to 1/x
  int temp1[2], temp2[2], next[2];
  int two[2] = {2, 0};

  // Usually 5-10 iterations is enough for good precision
  for (int i = 0; i < 10; i++) {
    // Calculate x * guess
    fp_mul(x_abs, guess, temp1);

    // Calculate 2 - x * guess
    fp_sub(two, temp1, temp2);

    // Calculate guess * (2 - x * guess)
    fp_mul(guess, temp2, next);

    // Check for convergence
    int diff[2];
    fp_sub(next, guess, diff);
    // If the absolute difference is very small, we've converged
    // if ((diff[0] == 0 && diff[1] == 0) ||
    //     (diff[0] == 0 && diff[1] < 0 && -diff[1] < SCALE / 10000) ||
    //     (diff[0] == 0 && diff[1] > 0 && diff[1] < SCALE / 10000)) {
    //   break;
    // }

    // Update guess for next iteration
    guess[0] = next[0];
    guess[1] = next[1];
    // printf("  Iteration %d: ", i);
    // fp_print(guess);
    // printf("\n");
  }

  // Apply sign
  if (sign < 0) {
    if (guess[1] != 0) {
      result[0] = -guess[0] - 1;
      result[1] = SCALE - guess[1];
    } else {
      result[0] = -guess[0];
      result[1] = 0;
    }
  } else {
    result[0] = guess[0];
    result[1] = guess[1];
  }
}

// Divide a by b using reciprocal (a/b = a * (1/b))
void fp_div(int a[], int b[], int result[]) {
  // Check for division by zero
  if ((b[0] == 0) && (b[1] == 0)) {
    result[0] = 0;
    result[1] = 0;
    return; // Division by zero
  }

  // Calculate reciprocal of b
  int recip_b[2];
  fp_recip(b, recip_b);

  // Multiply a by the reciprocal of b
  fp_mul(a, recip_b, result);
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

  int base[2] = {x[0], x[1]};
  int power = y;
  int temp[2];

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

    power /= 2;
  }
}

/* Calculate square root of a fixed-point number using Newton-Raphson method */
void fp_sqrt(int x[], int result[]) {
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
  // For simplicity, we can use x/2 if x > 1, or x*2 if x < 1
  int guess[2];
  if (x[0] > 1 || (x[0] == 1 && x[1] > 0)) {
    // x > 1: start with x/2
    int two[2] = {2, 0};
    fp_div(x, two, guess);
  } else {
    // x < 1: start with x*2
    int two[2] = {2, 0};
    fp_mul(x, two, guess);
  }

  // Newton-Raphson iteration: next = (guess + x/guess) / 2
  // We'll perform several iterations for accuracy
  int temp1[2], temp2[2], next[2];

  // Usually 5-10 iterations is enough for good precision
  int i = 0;
  while (i < 10) {
    i++;
    // Calculate x/guess
    fp_div(x, guess, temp1);

    // Calculate guess + x/guess
    fp_add(guess, temp1, temp2);

    // Calculate (guess + x/guess)/2
    int two[2] = {2, 0};
    fp_div(temp2, two, next);

    // Check for convergence
    int diff[2];
    fp_sub(next, guess, diff);
    // If the absolute difference is very small, we've converged
    if ((diff[0] == 0 && diff[1] == 0) ||
        (diff[0] == 0 && diff[1] < 0 && -diff[1] < SCALE / 10000) ||
        (diff[0] == 0 && diff[1] > 0 && diff[1] < SCALE / 10000)) {
      break;
    }

    // Update guess for next iteration
    guess[0] = next[0];
    guess[1] = next[1];
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
  // Normalize angle to [-π, π] range
  // First, create a 2π constant
  int two_pi[2] = {6, 2832}; // 6.2832 is approximately 2π

  // Create temporary variables
  int temp1[2], temp2[2], normalized[2];

  // Copy input to normalized
  normalized[0] = x[0];
  normalized[1] = x[1];

  // If x > π, subtract 2π until x <= π
  int pi[2] = {3, 1416}; // 3.1416 is approximately π
  while (normalized[0] > pi[0] ||
         (normalized[0] == pi[0] && normalized[1] > pi[1])) {
    fp_sub(normalized, two_pi, temp1);
    normalized[0] = temp1[0];
    normalized[1] = temp1[1];
  }

  // If x < -π, add 2π until x >= -π
  int neg_pi[2] = {-3, 1416}; // -3.1416
  while (normalized[0] < neg_pi[0] ||
         (normalized[0] == neg_pi[0] && normalized[1] < neg_pi[1])) {
    fp_add(normalized, two_pi, temp1);
    normalized[0] = temp1[0];
    normalized[1] = temp1[1];
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

  // x^3/3!
  int x3[2], term[2];
  fp_pow(normalized, 3, x3);
  // printf("  x^3 = ");
  // fp_print(x3);
  // printf("\n");
  int factorial3[2] = {6, 0}; // 3! = 6
  fp_div(x3, factorial3, term);
  // printf("  x^3/3! = ");
  // fp_print(term);
  // printf("\n");
  fp_sub(result, term, temp1);
  // printf("  x - x^3/3! = ");
  // fp_print(temp1);
  // printf("\n");

  // x^5/5!
  int x5[2];
  fp_pow(normalized, 5, x5);
  // printf("  x^5 = ");
  // fp_print(x5);
  // printf("\n");
  int factorial5[2] = {120, 0}; // 5! = 120
  fp_div(x5, factorial5, term);
  fp_add(temp1, term, temp2);
  // printf("  x - x^3/3! + x^5/5! = ");
  // fp_print(temp2);
  // printf("\n");

  // x^7/7!
  int x7[2];
  fp_pow(normalized, 7, x7);
  // printf("  x^7 = ");
  // fp_print(x7);
  // printf("\n");
  int factorial7[2] = {5040, 0}; // 7! = 5040
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
  int pi_over_2[2] = {1, 5708}; // 1.5708 is approximately π/2
  int temp[2];
  fp_add(x, pi_over_2, temp);
  fp_sin(temp, result);
}

// Initialize a fixed-point number
void fp_init(int a[], int whole, int frac) {
  a[0] = whole;
  a[1] = frac;
}

// Test fixed-point arithmetic
int main() {
  int a[2], b[2], c[2];

  // Initialize a and b
  float an = 1.50;
  float bn = 2.25;
  fp_init(a, 1, 5 * SCALE / 10);
  fp_init(b, 2, 25 * SCALE / 100);

  // Add a and b
  fp_add(a, b, c);
  fp_print(a);
  printf(" + ");
  fp_print(b);
  printf(" = ");
  fp_print(c);
  printf(" %f\n", an + bn);

  // Subtract b from a
  fp_sub(a, b, c);
  fp_print(a);
  printf(" - ");
  fp_print(b);
  printf(" = ");
  fp_print(c);
  printf(" %f\n", an - bn);

  // Multiply a and b
  fp_mul(a, b, c);
  fp_print(a);
  printf(" * ");
  fp_print(b);
  printf(" = ");
  fp_print(c);
  printf(" %f\n", an * bn);

  // Divide a by b
  fp_div(a, b, c);
  fp_print(a);
  printf(" / ");
  fp_print(b);
  printf(" = ");
  fp_print(c);
  printf(" %f\n", an / bn);

  // Calculate a^3
  fp_pow(a, 3, c);
  fp_print(a);
  printf(" ^ 3 = ");
  fp_print(c);
  printf(" %f\n", pow(an, 3));

  // Calculate sqrt(a)
  fp_sqrt(a, c);
  printf("sqrt(");
  fp_print(a);
  printf(") = ");
  fp_print(c);
  printf(" %f\n", sqrt(an));

  // Test trigonometric functions
  printf("\n--- Testing Trigonometric Functions ---\n");

  // Test sine function
  int angle[2];
  fp_init(angle, 1, 5708); // Approximately π/2 (1.5708)
  fp_sin(angle, c);
  printf("sin(");
  fp_print(angle);
  printf(") = ");
  fp_print(c);
  printf(" (should be close to 1.0000)\n");

  // Test cosine function
  fp_cos(angle, c);
  printf("cos(");
  fp_print(angle);
  printf(") = ");
  fp_print(c);
  printf(" (should be close to 0.0000)\n");

  // Test zero angle
  fp_init(angle, 0, 0);
  fp_sin(angle, c);
  printf("sin(");
  fp_print(angle);
  printf(") = ");
  fp_print(c);
  printf(" (should be 0.0000)\n");

  fp_cos(angle, c);
  printf("cos(");
  fp_print(angle);
  printf(") = ");
  fp_print(c);
  printf(" (should be 1.0000)\n");

  // Test pi
  fp_init(angle, 3, 1416); // Approximately π
  fp_sin(angle, c);
  printf("sin(");
  fp_print(angle);
  printf(") = ");
  fp_print(c);
  printf(" (should be close to 0.0000)\n");

  // fp_cos(angle, c);
  // printf("cos(");
  // fp_print(angle);
  // printf(") = ");
  // fp_print(c);
  // printf(" (should be close to -1.0000)\n");

  return 0;
}