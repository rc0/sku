
/* TODO:
 * symmetrical removal of givens in pose mode
 * tidy up verbose output
 * hint mode : advance to the next allocate in a part-solved puzzle
 * 5-gattai puzzles and generalisations thereof
 * analyze where pose mode spends its time.
 */

#include <time.h>

#include "sku.h"

/* ============================================================================ */
/* ============================================================================ */

static void pose(struct layout *lay)/*{{{*/
{
  int *state, *copy, *answer;
  int *keep;
  int i;
  int ok;
  int tally;

  state = new_array(int, lay->nc);
  copy = new_array(int, lay->nc);
  answer = new_array(int, lay->nc);
  keep = new_array(int, lay->nc);

  for (i=0; i<lay->nc; i++) {
    state[i] = -1;
    keep[i] = 0;
  }

  if (1 != infer(lay, state, NULL, 0, 0, OPT_SPECULATE | OPT_FIRST_ONLY)) {
    fprintf(stderr, "??? FAILED TO GENERATE AN INITIAL GRID!!\n");
    exit(1);
  }
  /* Must be at least 1 solution if the solver tries hard enough! */

  /* Now remove givens one at a time until we find a minimum number that leaves
   * a puzzle with a unique solution. */
  memcpy(answer, state, lay->nc * sizeof(int));

  tally = lay->nc;
  do {
    int start_point;
    start_point = lrand48() % lay->nc;
    ok = -1;
    for (i=0; i<lay->nc; i++) {
      int ii;
      int n_sol;
      ii = (i + start_point) % lay->nc;
      if (state[ii] < 0) continue;
      if (keep[ii] == 1) continue;
      memcpy(copy, state, lay->nc * sizeof(int));
      copy[ii] = -1;
      n_sol = infer(lay, copy, NULL, 0, 0, OPT_STOP_ON_2);
      tally--;
      if (n_sol == 1) {
        ok = ii;
        break;
      } else {
        /* If it's no good removing this given now, it won't be any better to try
         * removing it again later... */
        fprintf(stderr, "%4d :  (can't remove given from <%s>)\n", tally, lay->cells[ii].name);
        keep[ii] = 1;
      }
    }

    if (ok >= 0) {
      fprintf(stderr, "%4d : Removing given from <%s>\n", tally, lay->cells[ok].name);
      state[ok] = -1;
    }
  } while (ok >= 0);

  display(stdout, lay, state);

  return;
}
/*}}}*/

#if 0
static void discover(int options)/*{{{*/
{
  /* Experimental code...  Look for cases that have unambiguous solutions, but
   * where the solver still has to speculate to find the solution.  Print out
   * the solution as far as it can get, so we can work out what inference
   * approach it is lacking.  */

  struct layout lay;
  int *state, *copy, *copy2;
  int i;
  int n_solutions;
  int min_n_givens;

  layout_NxN(3, &lay);
  state = new_array(int, lay.nc);
  copy = new_array(int, lay.nc);
  copy2 = new_array(int, lay.nc);
  for (i=0; i<lay.nc; i++) {
    state[i] = -1;
  }
  n_solutions = infer(&lay, state, NULL, 0, 0, OPT_SPECULATE | OPT_FIRST_ONLY | options);
  if (n_solutions == 0) {
    fprintf(stderr, "oops, couldn't find a solution at the start!!\n");
    exit(1);
  }
  fprintf(stderr, "Complete puzzle:\n");
  display(stderr, &lay, state);

  min_n_givens = 81;
  do {
    int n_givens1, n_givens2;
    memcpy(copy, state, lay.nc * sizeof(int));
    n_givens1 = inner_reduce(&lay, copy, 0);
    memcpy(copy2, copy, lay.nc * sizeof(int));
    n_givens2 = inner_reduce(&lay, copy, OPT_SPECULATE);
    if (n_givens1 > n_givens2) {
      fprintf(stderr, "Puzzle is :\n");
      display(stderr, &lay, copy);
      fprintf(stderr, "Solvable puzzle is :\n");
      display(stderr, &lay, copy2);
      n_solutions = infer(&lay, copy, NULL, 0, 0, OPT_VERBOSE);
      fprintf(stderr, "Can get this far without guessing:\n");
      display(stdout, &lay, copy);
      exit(0);
    }
    if (n_givens1 < min_n_givens) {
      min_n_givens = n_givens1;
    }
    fprintf(stderr, "n_givens = %d   min = %d\n", n_givens1, min_n_givens);
  } while (1);
}
/*}}}*/
#endif

int main (int argc, char **argv)/*{{{*/
{
  int options;
  int seed;
  int N = 3;
  int iters_for_min = 0;
  int grey_cells = 0;
  enum operation {
    OP_BLANK,     /* Generate a blank grid */
    OP_ANY,       /* Generate any solution to a partial grid */
    OP_REDUCE,    /* Remove givens until it's no longer possible without
                     leaving an ambiguous puzzle. */
    OP_SOLVE,
#if 0
    OP_DISCOVER,
#endif
    OP_MARK,
    OP_FORMAT
  } operation;
  char *layout_name = NULL;
  
  operation = OP_SOLVE;

  options = 0;
  while (++argv, --argc) {
    if (!strcmp(*argv, "-v")) {
      options |= OPT_VERBOSE;
    } else if (!strcmp(*argv, "-f")) {
      options |= OPT_FIRST_ONLY;
    } else if (!strcmp(*argv, "-b")) {
      operation = OP_BLANK;
    } else if (!strcmp(*argv, "-a")) {
      operation = OP_ANY;
    } else if (!strcmp(*argv, "-r")) {
      operation = OP_REDUCE;
    } else if (!strncmp(*argv, "-k", 2)) {
      operation = OP_MARK;
      if ((*argv)[2] == 0) {
        grey_cells = 4;
      } else {
        grey_cells = atoi(*argv + 2);
      }
    } else if (!strcmp(*argv, "-s")) {
      options |= OPT_SPECULATE;
    } else if (!strcmp(*argv, "-F")) {
      operation = OP_FORMAT;
    } else if (!strcmp(*argv, "-y")) {
      options |= OPT_SYM_180;
    } else if (!strcmp(*argv, "-yy")) {
      options |= OPT_SYM_180 | OPT_SYM_90;
    } else if (!strncmp(*argv, "-m", 2)) {
      iters_for_min = atoi(*argv + 2);
#if 0
    } else if (!strcmp(*argv, "-d")) {
      operation = OP_DISCOVER;
#endif
    } else if (!strncmp(*argv, "-L", 2)) {
      /* Only needed for 'blank' mode now. */
      layout_name = *argv + 2;
    } else {
      fprintf(stderr, "Unrecognized argument <%s>\n", *argv);
      exit(1);
    }
  }

  seed = time(NULL) ^ getpid();
  if (options & OPT_VERBOSE) {
    fprintf(stderr, "Seed=%d\n", seed);
  }
  srand48(seed);
  switch (operation) {
    case OP_SOLVE:
      solve(options);
      break;
#if 0
    case OP_POSE:
      pose(&lay);
      break;
#endif
    case OP_ANY:
      solve_any(options);
      break;
    case OP_REDUCE:
      reduce(iters_for_min, options);
      break;
    case OP_BLANK:
      {
        struct layout *lay;
        lay = genlayout(layout_name ? layout_name : "3");
        blank(lay);
        break;
      }
#if 0
    case OP_DISCOVER:
      discover(options);
      break;
#endif
    case OP_MARK:
      mark_cells(grey_cells, options);
      break;
    case OP_FORMAT:
      format_output(options);
      break;
  }
  return 0;
}
/*}}}*/

/* ============================================================================ */

