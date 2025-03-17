// Iterative sum; an iterative variation of `treesum.c`
#include <prompt.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

void* sum(prompt_t p, void* arg) {
  int32_t max = (int32_t)(intptr_t)arg;
  for (int32_t i = 0; i < max; i++) {
    yield_result_t res = fiber_yield_to(p, (void*)(intptr_t)i);
    p = res.prompt;
  }
  return NULL;
}

int32_t run(int32_t max) {
  fiber_result_t status;
  fiber_t gen = fiber_alloc(sum);
  int32_t result = 0, i = 0;

  void* val = fiber_resume_with(gen, (void*)(intptr_t)max, &status);

  while (status == FIBER_YIELD && i++ < max) {
    result += (int32_t)(intptr_t)val;
    val = fiber_resume_with(gen, NULL, &status);
  }

  fiber_free(gen);
  return result;
}

int prog(int __attribute__((unused)) argc, char** argv) {
  int i = atoi(argv[1]);
  int32_t result = run(i);
  printf("%d\n", result);
  return 0;
}

int main(int argc, char** argv) {
  if (argc != 2) {
    fprintf(stderr, "Wrong number of arguments. Expected: 1");
    return -1;
  }
  return fiber_main(prog, argc, argv);
}
