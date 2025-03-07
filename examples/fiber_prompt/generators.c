#include <assert.h>
#include <fiber_prompt.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static prompt_t top_prompt;

void gen_nats(void) {
  for (int i = 0; i < 10; ++i) {
    printf("[gen_nats] loop_ctr: %i\n", i);
    fiber_yield_to(&top_prompt, (void*)(intptr_t)i);
  }
}

void* nest_gens(prompt_t prompt, void* arg);

void* nest_gens(prompt_t prompt, void* arg) {
  (void)prompt;
  int depth = (int)(intptr_t)arg;
  if (depth == 0) {
    printf("top prompt in nest_gens: %i, prompt: %i\n", top_prompt, prompt);
    gen_nats();
  } else {
    fiber_result_t status;
    fiber_t gens = fiber_alloc(nest_gens);
    fiber_resume_with(gens, (void*)(intptr_t)(depth - 1), &status);
    printf("status in nest_gens: %i\n", status);
    assert(status == FIBER_OK);
    fiber_free(gens);
  }
  return NULL;
}

void* sum_gens(prompt_t prompt, void* arg) {
  top_prompt = prompt;
  printf("top prompt that was set in sum_gens: %i\n", top_prompt);
  fiber_result_t status;
  fiber_t gens = fiber_alloc(nest_gens);
  fiber_resume_with(gens, arg, &status);
  printf("status in sum_gens: %i\n", status);
  assert(status == FIBER_OK);
  return NULL;
}

int main(void) {
  fiber_init();

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
  fiber_finalize();
  return 0;
}
