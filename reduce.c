/*
 *  sku - analysis tool for Sudoku puzzles
 *  Copyright (C) 2005  Richard P. Curnow
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA
 */

#include "sku.h"

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
/*{{{ inner_reduce_check_solvable() */
static int inner_reduce_check_solvable(struct layout *lay,
    int *state, const struct constraint *simplify_cons, int options)
{
  int *copy;
  int n_solutions, result;
  copy = new_array(int, lay->nc);
  memcpy(copy, state, lay->nc * sizeof(int));
  setup_terminals(lay);
  n_solutions = infer(lay, copy, NULL, NULL, simplify_cons, OPT_STOP_ON_2 | (options & OPT_SPECULATE));
  if (n_solutions == 1) {
    result = 1;
  } else {
    result = 0;
  }
  free(copy);
  return result;
}
/*}}}*/
static int trivial_p(struct layout *lay, int *state)/*{{{*/
{
  int ng, gi, ns;
  
  ng = lay->ng;
  ns = lay->ns;
  for (gi=0; gi<ng; gi++) {
    short *base = lay->groups + ns * gi;
    int n_givens, n_unknowns;
    int j;
    n_givens = 0;
    for (j=0; j<ns; j++) {
      int ci = base[j];
      if (state[ci] >= 0) {
        n_givens++;
      }
    }
    n_unknowns = ns - n_givens;
    if (n_unknowns < 2) {
      return 1;
    }
  }
  return 0;
}
/*}}}*/
/*{{{ puzzle_meets_requirements_p() */
static int puzzle_meets_requirements_p(struct layout *lay, int *state,
    const struct constraint *simplify_cons,
    const struct constraint *required_cons,
    int options)
{
  int *copy;
  int result;
  struct constraint temp_cons;
  int n_sol;

  copy = new_array(int, lay->nc);
  result = 1;

  if (required_cons->do_subsets) {/*{{{*/
    temp_cons = *simplify_cons;
    temp_cons.do_subsets = 0;
    memcpy(copy, state, lay->nc * sizeof(int));
    n_sol = infer(lay, copy, NULL, NULL, &temp_cons, options);
    if (n_sol == 1) {
      result = 0;
      goto get_out;
    }
  }
/*}}}*/
  if (required_cons->do_onlyopt) {/*{{{*/
    temp_cons = *simplify_cons;
    temp_cons.do_onlyopt = 0;
    memcpy(copy, state, lay->nc * sizeof(int));
    n_sol = infer(lay, copy, NULL, NULL, &temp_cons, options);
    if (n_sol == 1) {
      result = 0;
      goto get_out;
    }
  }
/*}}}*/
  if (required_cons->do_lines) {/*{{{*/
    temp_cons = *simplify_cons;
    temp_cons.do_lines = 0;
    memcpy(copy, state, lay->nc * sizeof(int));
    n_sol = infer(lay, copy, NULL, NULL, &temp_cons, options);
    if (n_sol == 1) {
      result = 0;
      goto get_out;
    }
  }
/*}}}*/
  if (required_cons->max_partition_size) {/*{{{*/
    temp_cons = *simplify_cons;
    temp_cons.max_partition_size = 
      (required_cons->max_partition_size == 2) ? 0 : (required_cons->max_partition_size - 1);
    memcpy(copy, state, lay->nc * sizeof(int));
    n_sol = infer(lay, copy, NULL, NULL, &temp_cons, options);
    if (n_sol == 1) {
      result = 0;
      goto get_out;
    }
  }
/*}}}*/

get_out:
  free(copy);
  return result;

}
/*}}}*/

int inner_reduce(struct layout *lay, int *state, const struct constraint *simplify_cons, int options)/*{{{*/
{
  int *copy, *answer;
  int *keep;
  int i;
  int ok;
  int tally;
  int kept_givens = 0;
  int is_trivial;

  inner_reduce_symmetrify_blanks(lay, state, options);
  if (!inner_reduce_check_solvable(lay, state, simplify_cons, options)) {
    fprintf(stderr, "Cannot reduce the puzzle, it doesn't have a unique solution\n");
  }

  copy = new_array(int, lay->nc);
  answer = new_array(int, lay->nc);
  keep = new_array(int, lay->nc);

  do {
  
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
        if (answer[ii] < 0) continue;
        if (keep[ii] == 1) continue;

        memcpy(copy, answer, lay->nc * sizeof(int));

        /* Remove given from 'ii' and all its symmetry group. */
        copy[ii] = -1;
        for (j = SYM(ii); j != ii; j = SYM(j)) {
          copy[j] = -1;
        }

        if (options & OPT_SPECULATE) {
          setup_terminals(lay);
          n_sol = infer(lay, copy, NULL, NULL, simplify_cons, OPT_SPECULATE);
        } else {
          setup_terminals(lay);
          n_sol = infer(lay, copy, NULL, NULL, simplify_cons, OPT_STOP_ON_2);
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
        answer[ok] = -1;
        if (options & OPT_VERBOSE) {
          fprintf(stderr, "%4d : Removing given from <%s>\n", tally, lay->cells[ok].name);
        }

        for (j=SYM(ok); j != ok; j = SYM(j)) {
          answer[j] = -1;
          if (options & OPT_VERBOSE) {
            fprintf(stderr, "%4d : Removing given from <%s> (sym)\n", tally, lay->cells[j].name);
          }
          tally--;
        }
      }
    } while (ok >= 0);

    is_trivial = trivial_p(lay, answer);
  } while (is_trivial && !(options & OPT_ALLOW_TRIVIAL));

  memcpy(state, answer, lay->nc * sizeof(int));

  free(copy);
  free(answer);
  free(keep);

  return kept_givens;
}
/*}}}*/

/*{{{ reduce() */
void reduce(int iters_for_min,
    const struct constraint *simplify_cons, const struct constraint *required_cons,
    int options)
{
  int *state;
  int *result;
  int kept_givens = 0;
  struct layout *lay;

  /* Sanity checks. */
  if ((required_cons->max_partition_size > simplify_cons->max_partition_size) ||
      (required_cons->do_subsets && !simplify_cons->do_subsets) ||
      (required_cons->do_onlyopt && !simplify_cons->do_onlyopt) ||
      (required_cons->do_lines && !simplify_cons->do_lines)) {
    fprintf(stderr, "-E options remove rules required by -R options\nGiving up\n");
    exit(1);
  }
    
  read_grid(&lay, &state, options);
  result = new_array(int, lay->nc);

  if (!required_cons->is_default) {
    int *copy, *copy2;
    int found;

    found = 0;
    copy = new_array(int, lay->nc);
    copy2 = new_array(int, lay->nc);
    do {
      memcpy(copy, state, lay->nc * sizeof(int));
      kept_givens = inner_reduce(lay, copy, simplify_cons, (options & ~OPT_VERBOSE));
      found = 0;
      memcpy(copy2, copy, lay->nc * sizeof(int));

      if (puzzle_meets_requirements_p(lay, copy2, simplify_cons, required_cons, options & ~OPT_VERBOSE)) {
        found = 1;
      }
    } while (!found);
    display(stdout, lay, copy);
    free(copy2);
    free(copy);
  } else if (iters_for_min == 0) {
    kept_givens = inner_reduce(lay, state, simplify_cons, options);

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
      kept_givens = inner_reduce(lay, copy, simplify_cons, (options & ~OPT_VERBOSE));
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

  free(result);
  free(state);
  free_layout(lay);
  return;
}
/*}}}*/
