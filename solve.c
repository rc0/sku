#include "sku.h"

void solve(int options)/*{{{*/
{
  int *state;
  int n_solutions;
  struct layout *lay;

  read_grid(&lay, &state, options);
  setup_terminals(lay);
  n_solutions = infer(lay, state, NULL, 0, 0, options);

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
  n_solutions = infer(lay, state, NULL, 0, 0, OPT_SPECULATE | OPT_FIRST_ONLY | options);

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

