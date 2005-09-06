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

struct option {
  int opt_flag;
  const char *name;
};

#define N_OPTIONS 5

const struct solve_option solve_options[N_SOLVE_OPTIONS] = {/*{{{*/
  { OPT_NO_LINES,     "Lines" },
  { OPT_NO_SUBSETS,   "Subsets" },
  { OPT_NO_ONLYOPT,   "Only option" },
  { OPT_NO_SPLIT_EXT, "Split (exterior)" },
  { OPT_NO_SPLIT_INT, "Split (interior)" }
};
/*}}}*/
void grade_find_sol_reqs(struct layout *lay, int *state, int options, char *result, char *min_result)/*{{{*/
{
  int *copy;
  int limit, i, j;
  copy = new_array(int, lay->nc);
  limit = 1 << N_SOLVE_OPTIONS;
  for (i=0; i<limit; i++) {
    int flags, n_sol;
    flags = 0;
    for (j=0; j<N_SOLVE_OPTIONS; j++) {
      if (i & (1<<j)) {
        flags |= solve_options[j].opt_flag;
      }
    }
    memcpy(copy, state, lay->nc * sizeof(int));
    setup_terminals(lay);
    flags |= (options & OPT_MAKE_EASIER);
    n_sol = infer(lay, copy, NULL, NULL, flags);
    if (n_sol == 1) {
      result[i] = 1;
    } else {
      result[i] = 0;
    }
  }

  memcpy(min_result, result, limit * sizeof(char));

  /* Work out the minimal set(s) of techniques that will do. */
  for (i=0; i<limit; i++) {
    if (min_result[i]) {
      for (j=0; j<limit; j++) {
        if (((i & j) == j) && (i != j)) {
          min_result[j] = 0;
        }
      }
    }
  }
  free(copy);
}
/*}}}*/
void grade(int options)/*{{{*/
{
  struct layout *lay;
  int *state;
  int *copy;
  int limit, i, j;
  char *result, *min_result;
  int count;

  read_grid(&lay, &state, options);
  copy = new_array(int, lay->nc);
  limit = 1 << N_SOLVE_OPTIONS;
  result = new_array(char, limit);
  min_result = new_array(char, limit);
  grade_find_sol_reqs(lay, state, options, result, min_result);

  /* Print result. */
  for (i=0; i<N_SOLVE_OPTIONS; i++) {
    int mask = 1<<i;
    fprintf(stderr, "%-17s ", solve_options[i].name);
    for (j=0; j<limit; j++) {
      if (j & mask) {
        fprintf(stderr, " -");
      } else {
        fprintf(stderr, " Y");
      }
    }
    fprintf(stderr, "\n");
  }
  fprintf(stderr, "----------------------------------------------------------------------------------\n");
  fprintf(stderr, "                  ");
  for (j=0; j<limit; j++) {
    if (result[j]) {
      fprintf(stderr, " #");
    } else {
      fprintf(stderr, " -");
    }
  }
  fprintf(stderr, "\n");


  fprintf(stderr, "\nMINIMAL OPTIONS TO SOLVE:\n");
  count = 1;
  for (i=0; i<limit; i++) {
    if (min_result[i]) {
      fprintf(stderr, "%4d: ", count++);
      if (i==(limit-1)) {
        fprintf(stderr, " (no special techniques required)\n");
      } else {
        for (j=0; j<N_OPTIONS; j++) {
          int mask = 1<<j;
          if (!(i & mask)) {
            fprintf(stderr, "<%s> ", solve_options[j].name);
          }
        }
      }
      fprintf(stderr, "\n");
    }
  }

  free(result);
  free(min_result);
  free(copy);
  free(state);
  free_layout(lay);
}
/*}}}*/
