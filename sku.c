
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

int main (int argc, char **argv)/*{{{*/
{
  int options;
  int seed;
  int N = 3;
  int iters_for_min = 0;
  int grey_cells = 0;
  enum format {
    LAY_3x3,
    LAY_4x4,
    LAY_5x5,
    LAY_3x3x5,
    LAY_3x3x8,
    LAY_3x3x11,
    LAY_3x3x9,
    LAY_4x4x5
  } format;
  enum operation {
    OP_BLANK,     /* Generate a blank grid */
    OP_ANY,       /* Generate any solution to a partial grid */
    OP_REDUCE,    /* Remove givens until it's no longer possible without
                     leaving an ambiguous puzzle. */
    OP_POSE,
    OP_SOLVE,
    OP_DISCOVER,
    OP_MARK,
    OP_FORMAT
  } operation;
  struct super_layout superlay;
  struct layout lay;
  
  format = LAY_3x3;
  operation = OP_SOLVE;

  options = 0;
  while (++argv, --argc) {
    if (!strcmp(*argv, "-v")) {
      options |= OPT_VERBOSE;
    } else if (!strcmp(*argv, "-f")) {
      options |= OPT_FIRST_ONLY;
    } else if (!strcmp(*argv, "-p")) {
      operation = OP_POSE;
    } else if (!strcmp(*argv, "-b")) {
      operation = OP_BLANK;
    } else if (!strcmp(*argv, "-a")) {
      operation = OP_ANY;
    } else if (!strcmp(*argv, "-r")) {
      operation = OP_REDUCE;
    } else if (!strcmp(*argv, "-k")) {
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
    } else if (!strcmp(*argv, "-d")) {
      operation = OP_DISCOVER;
    } else if (!strcmp(*argv, "-4")) {
      format = LAY_4x4;
    } else if (!strcmp(*argv, "-5")) {
      format = LAY_5x5;
    } else if (!strcmp(*argv, "-3/5")) {
      format = LAY_3x3x5;
    } else if (!strcmp(*argv, "-3/8")) {
      format = LAY_3x3x8;
    } else if (!strcmp(*argv, "-3/11")) {
      format = LAY_3x3x11;
    } else if (!strcmp(*argv, "-3/9")) {
      format = LAY_3x3x9;
    } else if (!strcmp(*argv, "-4/5")) {
      format = LAY_4x4x5;
    }
  }

  seed = time(NULL) ^ getpid();
  if (options & OPT_VERBOSE) {
    fprintf(stderr, "Seed=%d\n", seed);
  }
  srand48(seed);
  switch (format) {
    case LAY_3x3:
      layout_NxN(3, &lay);
      break;
    case LAY_4x4:
      layout_NxN(4, &lay);
      break;
    case LAY_5x5:
      layout_NxN(5, &lay);
      break;
    case LAY_3x3x5:
      superlayout_5(&superlay);
      layout_N_superlay(3, &superlay, &lay);
      break;
    case LAY_3x3x8:
      superlayout_8(&superlay);
      layout_N_superlay(3, &superlay, &lay);
      break;
    case LAY_3x3x11:
      superlayout_11(&superlay);
      layout_N_superlay(3, &superlay, &lay);
      break;
    case LAY_3x3x9:
      superlayout_9(&superlay);
      layout_N_superlay(3, &superlay, &lay);
      break;
    case LAY_4x4x5:
      superlayout_5(&superlay);
      layout_N_superlay(4, &superlay, &lay);
      break;
  }
  switch (operation) {
    case OP_SOLVE:
      solve(&lay, options);
      break;
    case OP_POSE:
      pose(&lay);
      break;
    case OP_ANY:
      solve_any(&lay, options);
      break;
    case OP_REDUCE:
      reduce(&lay, iters_for_min, options);
      break;
    case OP_BLANK:
      blank(&lay);
      break;
    case OP_DISCOVER:
      discover(options);
      break;
    case OP_MARK:
      mark_cells(&lay, grey_cells, options);
      break;
    case OP_FORMAT:
      format_output(&lay, options);
      break;
  }
  return 0;
}
/*}}}*/

/* ============================================================================ */

