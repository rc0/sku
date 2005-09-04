#include "sku.h"

static int detect_marked_puzzle(struct layout *lay, int *state)/*{{{*/
{
  int nc = lay->nc;
  int result = 0;
  int i;
  for (i=0; i<nc; i++) {
    if (state[i] == CELL_MARKED) {
      result = 1;
      break;
    }
  }
  return result;
}
/*}}}*/
void solve(int options)/*{{{*/
{
  int *state;
  int n_solutions;
  struct layout *lay;
  int is_marked;

  read_grid(&lay, &state, options);
  is_marked = detect_marked_puzzle(lay, state);
  if (is_marked) {
    options |= OPT_SOLVE_MARKED;
  }
  setup_terminals(lay);
  n_solutions = infer(lay, state, NULL, NULL, options);

  if (n_solutions == 0) {
    fprintf(stderr, "The puzzle had no solutions.\n"
        "Showing how far the solver got before becoming stuck.\n");
    display(stdout, lay, state);
  } else if (n_solutions == 1) {
    fprintf(stderr, "The puzzle had precisely 1 solution\n");
    display(stdout, lay, state);
  } else {
    if (options & OPT_SHOW_ALL) {
      fprintf(stderr, "The puzzle had %d solutions\n", n_solutions);
    } else {
      fprintf(stderr, "The puzzle had %d solutions (one is shown)\n", n_solutions);
      display(stdout, lay, state);
    }
  }

  free(state);
  free_layout(lay);
  return;
}
/*}}}*/
void solve_any(int options)/*{{{*/
{
  int *state;
  int n_solutions;
  struct layout *lay;

  read_grid(&lay, &state, options);
  setup_terminals(lay);
  n_solutions = infer(lay, state, NULL, NULL, OPT_SPECULATE | OPT_FIRST_ONLY | options);

  if (n_solutions == 0) {
    fprintf(stderr, "The puzzle had no solutions.\n"
        "Showing how far the solver got before becoming stuck.\n");
  }

  display(stdout, lay, state);

  free(state);
  free_layout(lay);
  return;
}
/*}}}*/

/* ============================================================================ */

