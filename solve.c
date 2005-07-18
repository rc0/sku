#include "sku.h"

void solve(int options)/*{{{*/
{
  int *state;
  int i;
  int n_solutions;
  struct layout *lay;

  read_grid(&lay, &state);
  n_solutions = infer(lay, state, NULL, 0, 0, options);

  if (n_solutions == 0) {
    fprintf(stderr, "The puzzle had no solutions\n");
    return;
  } else if (n_solutions == 1) {
    fprintf(stderr, "The puzzle had precisely 1 solution\n");

  } else {
    fprintf(stderr, "The puzzle had %d solutions (one is shown)\n", n_solutions);
  }

  display(stdout, lay, state);

  return;
}
/*}}}*/
void solve_any(int options)/*{{{*/
{
  int *state;
  int i;
  int n_solutions;
  struct layout *lay;

  read_grid(&lay, &state);
  n_solutions = infer(lay, state, NULL, 0, 0, OPT_SPECULATE | OPT_FIRST_ONLY | options);

  if (n_solutions == 0) {
    fprintf(stderr, "The puzzle had no solutions\n");
    return;
  }

  display(stdout, lay, state);

  return;
}
/*}}}*/

/* ============================================================================ */

