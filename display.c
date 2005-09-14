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

void display(FILE *out, struct layout *lay, int *state, struct clusters *clus)/*{{{*/
{
  int mn, i, j;
  char *grid;
  mn = lay->prows * lay->pcols;
  grid = new_array(char, mn);
  memset(grid, ' ', mn);
  for (i=0; i<lay->nc; i++) {
    int row = lay->cells[i].prow;
    int col = lay->cells[i].pcol;
    int idx = row * lay->pcols + col;
    if (state[i] == CELL_BARRED) {
      grid[idx] = '*';
    } else if (state[i] == CELL_MARKED) {
      grid[idx] = '#';
    } else if (state[i] == CELL_EMPTY) {
      grid[idx] = '.';
    } else {
      grid[idx] = lay->symbols[state[i]];
    }
  }

  fprintf(out, "#layout: %s\n", lay->name);
  for (i=0, j=0; i<mn; i++) {
    fputc(grid[i], out);
    j++;
    if (j == lay->pcols) {
      j = 0;
      fputc('\n', out);
    }
  }

  if (lay->is_additive && (clus != NULL)) {
    /* The caller may want not to display the cluster info */
    memset(grid, ' ', mn);
    for (i=0; i<lay->nc; i++) {
      int row = lay->cells[i].prow;
      int col = lay->cells[i].pcol;
      int idx = row * lay->pcols + col;
      grid[idx] = clus->cells[i];
    }

    fprintf(out, "\n");
    for (i=0, j=0; i<mn; i++) {
      fputc(grid[i], out);
      j++;
      if (j == lay->pcols) {
        j = 0;
        fputc('\n', out);
      }
    }
    for (i=0; i<256; i++) {
      if (clus->total[i] > 0) {
        printf("%1c %d\n", i, clus->total[i]);
      }
    }
  }

  free(grid);
}
/*}}}*/

