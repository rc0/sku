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

static int count_marked_cells(struct layout *lay, int *state)/*{{{*/
{
  int nc = lay->nc;
  int result = 0;
  int i;
  for (i=0; i<nc; i++) {
    if (state[i] == CELL_MARKED) {
      result++;
    }
  }
  return result;
}
/*}}}*/
void solve_minimal(struct layout *lay, int *state, const struct constraint *simplify_cons, int options)/*{{{*/
{
  int n_solutions;
  int *copy, *copy0;
  char *barred;
  int next_to_bar;

  copy = new_array(int, lay->nc);
  copy0 = new_array(int, lay->nc);
  barred = new_array(char, lay->nc);
  memset(barred, 0, lay->nc);

  memcpy(copy, state, lay->nc * sizeof(int));
  n_solutions = infer(lay, copy, NULL, NULL, simplify_cons, options);
  if (n_solutions != 1) {
    fprintf(stderr, "Cannot produce minimal solution unless puzzle has a unique solution\n");
    goto get_out;
  }

  memcpy(copy0, state, lay->nc * sizeof(int));
  for (next_to_bar = 0; next_to_bar < lay->nc; next_to_bar++) {
    if (state[next_to_bar] >= 0) continue;
    if (state[next_to_bar] == CELL_MARKED) continue;
    memcpy(copy, copy0, lay->nc * sizeof(int));
    copy[next_to_bar] = CELL_BARRED;
    n_solutions = infer(lay, copy, NULL, NULL, simplify_cons, options);
    if (n_solutions == 1) {
      barred[next_to_bar] = 1;
      copy0[next_to_bar] = CELL_BARRED;
    }
  }
  display(stdout, lay, copy0);
  memcpy(state, copy0, lay->nc * sizeof(int));

get_out:
  free(copy);
  free(copy0);
  free(barred);
  return;
}
/*}}}*/
void solve(const struct constraint *simplify_cons, int options)/*{{{*/
{
  int *state;
  int n_solutions;
  struct layout *lay;
  int n_marked;

  read_grid(&lay, &state, options);
  n_marked = count_marked_cells(lay, state);
  if (n_marked > 0) {
    fprintf(stderr, "Found %d marked cell%s to solve for\n",
        n_marked,
        n_marked==1 ? "" : "s");
    options |= OPT_SOLVE_MARKED;
  }
  setup_terminals(lay);
  if (options & OPT_SOLVE_MINIMAL) {
    if (n_marked) {
      solve_minimal(lay, state, simplify_cons, options);
    } else {
      fprintf(stderr, "-M specified but puzzle is not marked\n");
      exit(1);
    }
  } else {
    n_solutions = infer(lay, state, NULL, NULL, simplify_cons, options);

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
  n_solutions = infer(lay, state, NULL, NULL, &cons_all, OPT_SPECULATE | OPT_FIRST_ONLY | options);

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

