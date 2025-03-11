#include <assert.h>
#include <prompt.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static prompt_t top_prompt;

void gen_nats(void) {
  for (int i = 0; i < 10; ++i) {
    yield_result_t res = fiber_yield_to(top_prompt, (void*)(intptr_t)i);
    top_prompt = res.prompt;
  }
}

void* nest_gens(prompt_t prompt, void* arg);

void* nest_gens(prompt_t prompt, void* arg) {
  (void)prompt;
  int depth = (int)(intptr_t)arg;
  if (depth == 0) {
    gen_nats();
  } else {
    fiber_result_t status;
    fiber_t gens = fiber_alloc(nest_gens);
    fiber_resume_with(gens, (void*)(intptr_t)(depth - 1), &status);
    assert(status == FIBER_OK);
    fiber_free(gens);
  }
  return NULL;
}

void* sum_gens(prompt_t prompt, void* arg) {
  top_prompt = prompt;
  fiber_result_t status;
  fiber_t gens = fiber_alloc(nest_gens);
  fiber_resume_with(gens, arg, &status);
  if (status != FIBER_OK && status != FIBER_FORWARD) {
    abort();
  }
  return NULL;
}

int prog(int __attribute__((unused)) argc,
         char** __attribute__((unused)) argv) {
  fiber_result_t status;
  fiber_t sum_fiber = fiber_alloc(sum_gens);

  int sum = 0;
  void* ans = fiber_resume_with(sum_fiber, (void*)(intptr_t)(0), &status);
  while (status == FIBER_YIELD) {
    sum += (int)(intptr_t)ans;
    ans = fiber_resume_with(sum_fiber, NULL, &status);
  }
  assert(status == FIBER_OK);
  printf("sum was %i\n", sum);

  fiber_free(sum_fiber);
  return 0;
}

int main(int argc, char** argv) { return fiber_main(prog, argc, argv); }
