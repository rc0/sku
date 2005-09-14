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

void blank(struct layout *lay)/*{{{*/
{
  int *state;
  struct clusters *clus;
  int i;
  state = new_array(int, lay->nc);
  for (i=0; i<lay->nc; i++) {
    state[i] = -1;
  }
  if (lay->is_additive) {
    clus = mk_clusters(lay->nc);
  } else {
    clus = NULL;
  }
  display(stdout, lay, state, clus);
  free(state);
  if (clus) free_clusters(clus);
}
/*}}}*/
