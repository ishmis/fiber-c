// Tree traversal; a recursive variation of `itersum.c`
#include <prompt.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct node {
  enum { LEAF, FORK } tag;
  union {
    // Leaf.
    int32_t val;
    // Fork
    struct {
      struct node *left;
      struct node *right;
    };
  };
} node_t;

node_t *build_tree(int32_t depth, int32_t val) {
  node_t *node = (node_t *)malloc(sizeof(node_t));
  if (depth == 0) {
    node->tag = LEAF;
    node->val = val;
  } else {
    node->tag = FORK;
    node_t *subtree = build_tree(depth - 1, val + 1);
    node->left = subtree;
    node->right = subtree;
  }
  return node;
}

void free_tree(node_t *node) {
  enum { LEAF, FORK } tag = node->tag;
  if (tag == FORK) {
    free_tree(node->left);
    free_tree(node->right);
  }
  free(node);
}

void walk_tree(prompt_t *p, node_t *node) {
  if (node->tag == LEAF) {
    yield_result_t res = fiber_yield_to(*p, (void *)(intptr_t)node->val);
    *p = res.prompt;
  } else {
    walk_tree(p, node->left);
    walk_tree(p, node->right);
  }
}

void *tree_walker(prompt_t p, void *node) {
  walk_tree(&p, (node_t *)node);
  return NULL;
}

int32_t run(node_t *tree) {
  fiber_result_t status;
  int32_t sum = 0;
  fiber_t walker = fiber_alloc(tree_walker);

  void *val = fiber_resume_with(walker, (void *)tree, &status);

  // / printf("status after first resume: %i\n", status);

  while (status == FIBER_YIELD) {
    // printf("resume!\n");
    sum += (int32_t)(intptr_t)val;
    val = fiber_resume_with(walker, NULL, &status);
    // / printf("status after resume: %i\n", status);
  }

  fiber_free(walker);
  return sum;
}

int prog(int __attribute__((unused)) argc, char **argv) {
  int i = atoi(argv[1]);
  node_t *tree = build_tree((int32_t)i, 0);
  int32_t result = run(tree);
  free_tree(tree);
  printf("%d\n", result);
  return 0;
}

int main(int argc, char **argv) {
  if (argc != 2) {
    fprintf(stderr, "Wrong number of arguments. Expected: 1");
    return -1;
  }
  return fiber_main(prog, argc, argv);
}
