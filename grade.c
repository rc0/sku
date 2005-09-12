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

void grade(int options)/*{{{*/
{
  struct layout *lay;
  int *state;
  int *copy;
  struct constraint cons;
  int xl, xs, xo, xp, rxp;

  read_grid(&lay, &state, options);
  copy = new_array(int, lay->nc);

  printf("Available methods             Reqd partition size\n");
  printf("-----------------             -------------------\n");
  for (xl=0; xl<=1; xl++) {
    for (xs=0; xs<=1; xs++) {
      for (xo=0; xo<=1; xo++) {
        rxp = -1;   
        for (xp=0; xp<=MAX_PARTITION_SIZE; xp++) {
          if (xp == 1) continue;
          int n_sol;
          memcpy(copy, state, lay->nc * sizeof(int));
          cons.do_lines = xl;
          cons.do_subsets = xs;
          cons.do_onlyopt = xo;
          cons.max_partition_size = xp;
          n_sol = infer(lay, copy, NULL, NULL, &cons, options);
          if (n_sol == 1) {
            rxp = xp;
            break;
          }
        }
        printf("%s ", xl ? "Lines   " : "        ");
        printf("%s ", xs ? "Subsets " : "        ");
        printf("%s ", xo ? "Onlyopt " : "        ");
        if (rxp < 0) {
          printf(" : NO SOLUTION\n");
        } else if (rxp == 0) {
          printf(" : 0\n");
        } else {
          printf(" : >= %d\n", rxp);
        }
      }
    }
  }

  free(copy);
  free(state);
  free_layout(lay);
}
/*}}}*/
