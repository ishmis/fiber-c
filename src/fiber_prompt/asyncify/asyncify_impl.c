// An asyncify implementation of the basic fiber interface.

#include <assert.h>
#include <stdint.h>
#include <stdio.h>  // TODO(ishmis): REMOVE
#include <stdlib.h>

#include "fiber_prompt.h"
#define import(NAME) \
  __attribute__((import_module("asyncify"), import_name(NAME)))

/** Asyncify imports **/
// The following functions are asyncify primitives:
// * asyncify_start_unwind(iptr): initiates a continuation
//   capture. The argument `iptr` is a pointer to an asyncify stack.
// * asyncify_stop_unwind(): delimits a capture continuations.
// * asyncify_start_rewind(iptr): initiates a continuation
//   reinstatement. The argument `iptr` is a pointer to an asyncify
//   stack.
// * asyncfiy_stop_rewind(): delimits the extent of a continuation
//  reinstatement.
extern import("start_unwind") void asyncify_start_unwind(void *);

extern import("stop_unwind") void asyncify_stop_unwind(void);

extern import("start_rewind") void asyncify_start_rewind(void *);

extern import("stop_rewind") void asyncify_stop_rewind(void);

// The default stack size is 2MB.
static const size_t default_stack_size = ASYNCIFY_DEFAULT_STACK_SIZE;

// We track the currently active fiber via this global variable.
static volatile fiber_t active_fiber = NULL;

// We keep a global prompt_id for generating new ids
static prompt_t global_prompt = 0;

// Fiber states:
// * ACTIVE: the fiber is actively executing.
// * YIELDING: the fiber is suspended.
// * NAME_MISMATCH: the fiber was suspended incorrectly
// * DONE: the fiber is finished (i.e. run to completion).
typedef enum { ACTIVE, YIELDING, FORWARDING, DONE } fiber_state_t;

struct resume_args {
  prompt_t p;
  void *arg;
};

// A fiber stack is an asyncify stack, i.e. a reserved area of memory
// for asyncify to store the call chain and locals. Note: asyncify
// assumes `end` is at offset 4. Moreover, asyncify stacks grow
// upwards, so it must be that top <= end.
struct __attribute__((packed)) fiber_stack {
  uint8_t *top;
  uint8_t *end;
  uint8_t *buffer;
};
static_assert(sizeof(uint8_t *) == 4, "sizeof(uint8_t*) != 4");
static_assert(sizeof(struct fiber_stack) == 12,
              "struct fiber_stack: No padding allowed");

// The fiber structure embeds the asyncify stack (struct fiber_stack),
// its state, an entry point, and two buffers for communicating
// payloads and managing fiber local data, respectively.
struct fiber {
  /** The underlying asyncify stack. */
  struct fiber_stack stack;
  // Fiber state.
  fiber_state_t state;
  // Initial function to run on the fiber.
  fiber_entry_point_t entry;
  prompt_t prompt;
  struct resume_args arg;
};

// Allocates a fiber stack of size stack_size.
static struct fiber_stack fiber_stack_alloc(size_t stack_size) {
  uint8_t *buffer = malloc(sizeof(uint8_t) * stack_size);
  uint8_t *top = buffer;
  uint8_t *end = buffer + stack_size;
  struct fiber_stack stack = (struct fiber_stack){top, end, /* NULL, */ buffer};
  return stack;
}

// Frees an allocated fiber_stack.
static void fiber_stack_free(struct fiber_stack fiber_stack) {
  free(fiber_stack.buffer);
}

#if defined STACK_POOL_SIZE && STACK_POOL_SIZE > 0
// Fiber stack pool
struct stack_pool {
  int32_t next;
  struct fiber_stack stacks[STACK_POOL_SIZE];
};

static struct fiber_stack stack_pool_next(volatile struct stack_pool *pool) {
  assert(pool->next >= 0 && pool->next < STACK_POOL_SIZE);
  return pool->stacks[pool->next++];
}

static void stack_pool_reclaim(volatile struct stack_pool *pool,
                               struct fiber_stack stack) {
  assert(pool->next > 0 && pool->next <= STACK_POOL_SIZE);
  pool->stacks[--pool->next] = stack;
  return;
}

static volatile struct stack_pool pool;
#endif

// Allocates a fiber object.
// NOTE: the entry point `fn` should be careful about uses of `printf`
// and related functions, as they can cause asyncify to corrupt its
// own state. See `wasi-io.h` for asyncify-safe printing functions.
fiber_t fiber_sized_alloc(size_t stack_size, fiber_entry_point_t entry) {
  fiber_t fiber = (fiber_t)malloc(sizeof(struct fiber));
#if defined STACK_POOL_SIZE && STACK_POOL_SIZE > 0
  (void)stack_size;
  fiber->stack = stack_pool_next(&pool);
  // TODO(dhil): It may be necessary to reset the top pointer.  I'd
  // need to test on a larger example.
  fiber->stack.top = fiber->stack.buffer;
#else
  fiber->stack = fiber_stack_alloc(stack_size);
#endif
  fiber->state = ACTIVE;
  fiber->prompt = 0;
  fiber->entry = entry;
  fiber->arg = (struct resume_args){0, 0};
  return fiber;
}

// Allocates a fiber object with the default stack size.
__attribute__((noinline)) fiber_t fiber_alloc(fiber_entry_point_t entry) {
  return fiber_sized_alloc(default_stack_size, entry);
}

// Frees a fiber object.
__attribute__((noinline)) void fiber_free(fiber_t fiber) {
#if defined STACK_POOL_SIZE && STACK_POOL_SIZE > 0
  stack_pool_reclaim(&pool, fiber->stack);
#else
  fiber_stack_free(fiber->stack);
#endif
  free(fiber);
}

// Yields control from within a fiber computation to whichever point
// originally resumed the fiber.
__attribute__((noinline)) void *fiber_yield_to(prompt_t *prompt, void *arg) {
  assert(active_fiber->state != FORWARDING);
  printf("yielding with prompt: %i\n", *prompt);
  if (active_fiber->state == YIELDING) {
    printf("active_prompt: %i\n", active_fiber->prompt);
    asyncify_stop_rewind();
    *prompt = active_fiber->arg.p;
    active_fiber->state = ACTIVE;
    return active_fiber->arg.arg;
  } else {
    active_fiber->arg.arg = arg;
    active_fiber->arg.p = *prompt;
    active_fiber->state = YIELDING;
    asyncify_start_unwind(&active_fiber->stack);
    return NULL;  // dummy value; this statement never gets executed.
  }
}

// Resumes a given fiber. Control is transferred to the fiber.
__attribute__((noinline)) void *fiber_resume_with(fiber_t fiber, void *arg,
                                                  fiber_result_t *result) {
  // If we are done, signal error and return.
  if (fiber->state == DONE) {
    *result = FIBER_ERROR;
    return NULL;
  }

  // Remember the currently executing fiber.
  volatile fiber_t prev = active_fiber;
  // child for forwarding
  // volatile fiber_t child = fiber;

  // prev prompt
  // prompt_t curr_prompt = fiber->prompt;

  // Set the given fiber as the actively executing fiber.
  active_fiber = fiber;

  // If we are resuming a suspended fiber...
  if (fiber->state == FORWARDING) {
    printf("in forwarding, fiber_prompt: %i\n", fiber->prompt);
    fiber->arg = prev->arg;
    fiber->state = YIELDING;
    asyncify_start_rewind(&fiber->stack);
  } else if (fiber->state == YIELDING) {
    // update prompt
    fiber->arg.p = global_prompt++;
    fiber->prompt = fiber->arg.p;
    // ... then update the argument buffer.
    fiber->arg.arg = arg;
    // ... and initiate the stack rewind.
    asyncify_start_rewind(&fiber->stack);
  } else {
    fiber->prompt = global_prompt++;
  }

  // Run the entry function.
  // Note: the entry function must be run first both when the fiber is started
  // and resumed!
  void *fiber_result = fiber->entry(fiber->prompt, arg);
  // The following function delimits the effects of fiber_yield.
  asyncify_stop_unwind();
  // printf("curr_prompt = %i, arg.p = %i\n", curr_prompt, fiber->arg.p);
  // Try next enclosing handler
  if (fiber->prompt != fiber->arg.p) {
    if (prev == NULL) {
      printf("going to abort!\n");
      abort();
    }
    printf("going to forward with prompt: %i, fiber_prompt is: %i, prev_prompt is: %i\n",
           active_fiber->arg.p, fiber->prompt, prev->prompt);
    // printf("going into forwarding now!\n");
    prompt_t p = prev->arg.p;
    active_fiber->state = FORWARDING;
    fiber = active_fiber;
    prev->arg = active_fiber->arg;
    active_fiber = prev;
    fiber_yield_to(&p, prev->arg.arg);
  }
  // Check whether the fiber finished or suspended.
  if (fiber->state != YIELDING && fiber->state != FORWARDING)
    fiber->state = DONE;

  // Restore the previously executing fiber.
  active_fiber = prev;
  assert(fiber->state != FORWARDING);
  // Signal success.
  if (fiber->state == YIELDING) {
    *result = FIBER_YIELD;
    return fiber->arg.arg;
  } else {
    *result = FIBER_OK;
    return fiber_result;
  }
}

// Noop when stack pooling is disabled.
void fiber_init(void) {
#if defined STACK_POOL_SIZE && STACK_POOL_SIZE > 0
  pool.next = STACK_POOL_SIZE;
  for (uint32_t i = 0; i < STACK_POOL_SIZE; i++) {
    stack_pool_reclaim(&pool, fiber_stack_alloc(default_stack_size));
  }
  assert(pool.next == 0);
#endif
}

// Noop when stack pooling is disabled.
void fiber_finalize(void) {
#if defined STACK_POOL_SIZE && STACK_POOL_SIZE > 0
  assert(pool.next == 0);
  for (uint32_t i = 0; i < STACK_POOL_SIZE; i++) {
    fiber_stack_free(stack_pool_next(&pool));
  }
  assert(pool.next == STACK_POOL_SIZE);
#endif
}

#undef import
