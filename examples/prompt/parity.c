// a mutually recursive parity checker
#include <assert.h>
#include <ctype.h>
#include <prompt.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

/**
Based on the standard mutually recursion example:
let rec even n =
  match n with
    | 0 -> true
    | x -> odd (x-1)
and odd n =
  match n with
    | 0 -> false
    | x -> even (x-1);;
**/

static bool sanitise_input_number(const char* s) {
  for (; *s != '\0'; s++) {
    if (!isdigit(*s)) return false;
  }
  return true;
}

struct odd_args {
  int32_t n;
  prompt_t prompt;
};

void* is_odd(prompt_t p, void* arg) {
  struct odd_args* odd_args = (struct odd_args*)arg;

  while (odd_args->n != 0) {
    yield_result_t res = fiber_yield_to(p, (void*)(intptr_t)(odd_args->n - 1));
    p = res.prompt;
    odd_args = (struct odd_args*)res.value;
  }

  // suspending directly to run
  (void)fiber_yield_to(odd_args->prompt, (void*)(intptr_t)0);
  return NULL;
}

void* is_even(prompt_t p, void* arg) {
  fiber_result_t status;
  fiber_t odd_checker = fiber_alloc(is_odd);
  int32_t n = (int32_t)(intptr_t)arg;

  while (n != 0) {
    struct odd_args args = (struct odd_args){n - 1, p};
    void* val = fiber_resume_with(odd_checker, (void*)&args, &status);
    assert(status != FIBER_ERROR);
    n = (int32_t)(intptr_t)val;
  }
  (void)fiber_yield_to(p, (void*)(intptr_t)1);
  return NULL;
}

int32_t run(int32_t n) {
  fiber_result_t status;
  fiber_t even_checker = fiber_alloc(is_even);

  void* val = fiber_resume_with(even_checker, (void*)(intptr_t)n, &status);
  assert(status != FIBER_ERROR);

  fiber_free(even_checker);
  return (int32_t)(intptr_t)val;
}

int prog(int __attribute__((unused)) argc, char** argv) {
  if (!sanitise_input_number(argv[1])) {
    fprintf(stderr, "error: input must be a positive integer!\n");
    exit(1);
  }
  int i = atoi(argv[1]);
  int32_t is_even = run(i);
  if (is_even == 1) {
    printf("%i is an even number!\n", i);
  } else {
    printf("%i is an odd number!\n", i);
  }
  return 0;
}

int main(int argc, char** argv) {
  if (argc != 2) {
    fprintf(stderr, "Wrong number of arguments. Expected: 1");
    return -1;
  }
  return fiber_main(prog, argc, argv);
}
