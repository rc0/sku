#include "sku.h"

#define SYM(x) (lay->cells[(x)].isym)

static void inner_reduce_symmetrify_blanks(struct layout *lay, int *state, int options)/*{{{*/
{
  int i, j;
  int NC = lay->nc;
  for (i=0; i<NC; i++) {
    if (state[i] < 0) {
      for (j = SYM(i); j != i; j = SYM(j)) {
        state[j] = -1;
      }
    }
  }
}
/*}}}*/
static int inner_reduce_check_solvable(struct layout *lay, int *state, int options)/*{{{*/
{
  int *copy;
  int n_solutions, result;
  copy = new_array(int, lay->nc);
  memcpy(copy, state, lay->nc * sizeof(int));
  n_solutions = infer(lay, copy, NULL, 0, 0, OPT_STOP_ON_2 | (options & OPT_SPECULATE));
  if (n_solutions == 1) {
    result = 1;
  } else {
    result = 0;
  }
  free(copy);
  return result;
}
/*}}}*/
int inner_reduce(struct layout *lay, int *state, int options)/*{{{*/
{
  int *copy, *answer;
  int *keep;
  int i;
  int ok;
  int tally;
  int kept_givens = 0;

  inner_reduce_symmetrify_blanks(lay, state, options);
  if (!inner_reduce_check_solvable(lay, state, options)) {
    fprintf(stderr, "Cannot reduce the puzzle, it doesn't have a unique solution\n");
  }

  copy = new_array(int, lay->nc);
  answer = new_array(int, lay->nc);
  keep = new_array(int, lay->nc);

  memset(keep, 0, lay->nc * sizeof(int));
  tally = 0;
  for (i=0; i<lay->nc; i++) {
    if (state[i] >= 0) tally++;
  }

  /* Now remove givens one at a time until we find a minimum number that leaves
   * a puzzle with a unique solution. */
  memcpy(answer, state, lay->nc * sizeof(int));

  do {
    int start_point;
    int j;
    start_point = lrand48() % lay->nc;
    ok = -1;
    for (i=0; i<lay->nc; i++) {
      int ii;
      int n_sol;

      ii = (i + start_point) % lay->nc;
      if (state[ii] < 0) continue;
      if (keep[ii] == 1) continue;

      memcpy(copy, state, lay->nc * sizeof(int));

      /* Remove given from 'ii' and all its symmetry group. */
      copy[ii] = -1;
      for (j = SYM(ii); j != ii; j = SYM(j)) {
        copy[j] = -1;
      }

      if (options & OPT_SPECULATE) {
        n_sol = infer(lay, copy, NULL, 0, 0, OPT_SPECULATE);
      } else {
        n_sol = infer(lay, copy, NULL, 0, 0, (options & OPT_MAKE_EASIER) | OPT_STOP_ON_2);
      }
      tally--;
      if (n_sol == 1) {
        ok = ii;
        break;
      } else {
        /* If it's no good removing this given now, it won't be any better to try
         * removing it again later... */
        if (options & OPT_VERBOSE) {
          fprintf(stderr, "%4d :  (can't remove given from <%s>)\n", tally, lay->cells[ii].name);
        }
        keep[ii] = 1;
        ++kept_givens;
        for (j = SYM(ii); j != ii; j = SYM(j)) {
          keep[j] = 1;
          ++kept_givens;
          if (options & OPT_VERBOSE) {
            fprintf(stderr, "%4d :  (can't remove given from <%s> (sym))\n", tally, lay->cells[j].name);
          }
          tally--;
        }
      }
    }

    if (ok >= 0) {
      state[ok] = -1;
      if (options & OPT_VERBOSE) {
        fprintf(stderr, "%4d : Removing given from <%s>\n", tally, lay->cells[ok].name);
      }

      for (j=SYM(ok); j != ok; j = SYM(j)) {
        state[j] = -1;
        if (options & OPT_VERBOSE) {
          fprintf(stderr, "%4d : Removing given from <%s> (sym)\n", tally, lay->cells[j].name);
        }
        tally--;
      }
    }
  } while (ok >= 0);

  free(copy);
  free(answer);
  free(keep);

  return kept_givens;
}
/*}}}*/

void reduce(int iters_for_min, int options, int req_n)/*{{{*/
{
  int *state;
  int *result;
  int kept_givens = 0;
  struct layout *lay;

  read_grid(&lay, &state, options);
  result = new_array(int, lay->nc);

  if (req_n) {
    int *copy, *copy2;
    char *grade_req, *min_grade_req;
    int i;
    int limit;
    int found;
    int mask;

    found = 0;
    copy = new_array(int, lay->nc);
    copy2 = new_array(int, lay->nc);
    limit = 1 << N_SOLVE_OPTIONS;
    grade_req = new_array(char, limit);
    min_grade_req = new_array(char, limit);
    mask = 0;
    for (i=0; i<N_SOLVE_OPTIONS; i++) {
      if (req_n & solve_options[i].opt_flag) {
        mask |= (1<<i);
      }
    }
    do {
      memcpy(copy, state, lay->nc * sizeof(int));
      kept_givens = inner_reduce(lay, copy, (options & ~OPT_VERBOSE));
      found = 1;
      memcpy(copy2, copy, lay->nc * sizeof(int));
      grade_find_sol_reqs(lay, copy2, options & ~OPT_VERBOSE, grade_req, min_grade_req);
      for (i=0; i<limit; i++) {
        if (grade_req[i]) {
          if (i & mask) {
            found = 0;
            break;
          }
        }
      }
    } while (!found);
    display(stdout, lay, copy);
    free(copy2);
    free(copy);
  } else if (iters_for_min == 0) {
    kept_givens = inner_reduce(lay, state, options);

    if (options & OPT_VERBOSE) {
      fprintf(stderr, "%d givens kept\n", kept_givens);
    }
    display(stdout, lay, state);
  } else {
    int i;
    int min_givens;
    int *copy;
    min_givens = lay->nc;
    copy = new_array(int, lay->nc);
    for (i=0; i<iters_for_min; i++) {
      memcpy(copy, state, lay->nc * sizeof(int));
      kept_givens = inner_reduce(lay, copy, (options & ~OPT_VERBOSE));
      if (kept_givens < min_givens) {
        min_givens = kept_givens;
        if (options & OPT_VERBOSE) {
          fprintf(stderr, "Found a layout with %d givens\n", kept_givens);
          display(stderr, lay, copy);
        }
        /* The result always has to have the minimum number of givens found so
         * far. */
        memcpy(result, copy, lay->nc * sizeof(int));
      }
    }
    display(stdout, lay, result);
  }

  return;
}
/*}}}*/
