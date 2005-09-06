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

static void chomp(char *x)/*{{{*/
{
  int len;
  len = strlen(x);
  if ((len > 0) && (x[len-1] == '\n')) {
    x[len-1] = '\0';
  }
}
/*}}}*/
void read_grid(struct layout **lay, int **state, int options)/*{{{*/
{
  int rmap[256];
  int valid[256];
  int i, c;
  char buffer[256];
  struct layout *my_lay;

  fgets(buffer, sizeof(buffer), stdin);
  chomp(buffer);
  if (strncmp(buffer, "#layout: ", 9)) {
    fprintf(stderr, "Input does not start with '#layout: ', giving up.\n");
    exit(1);
  }

  my_lay = genlayout(buffer + 9, options);
  *state = new_array(int, my_lay->nc);
  

  for (i=0; i<256; i++) {
    rmap[i] = CELL_EMPTY;
    valid[i] = 0;
  }
  rmap['#'] = CELL_MARKED;
  rmap['*'] = CELL_BARRED;
  for (i=0; i<my_lay->ns; i++) {
    rmap[(int) my_lay->symbols[i]] = i;
    valid[(int) my_lay->symbols[i]] = 1;
  }
  valid['.'] = 1;
  valid['*'] = 1;
  valid['#'] = 1;

  for (i=0; i<my_lay->nc; i++) {
    do {
      c = getchar();
      if (c == EOF) {
        fprintf(stderr, "Ran out of input data!\n");
        exit(1);
      }
      if (valid[c]) {
        (*state)[i] = rmap[c];
      }
    } while (!valid[c]);
  }
  *lay = my_lay;
}
/*}}}*/
