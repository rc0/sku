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

int count_bits(unsigned int a)/*{{{*/
{
  a = (a & 0x55555555) + ((a>>1) & 0x55555555);
  a = (a & 0x33333333) + ((a>>2) & 0x33333333);
  a = (a & 0x0f0f0f0f) + ((a>>4) & 0x0f0f0f0f);
  a = (a & 0x00ff00ff) + ((a>>8) & 0x00ff00ff);
  a = (a & 0x0000ffff) + ((a>>16) & 0x0000ffff);
  return a;
}
/*}}}*/
int decode(unsigned int a)/*{{{*/
{
  /* This needs optimising?? */

  int r, m;
  m = 1;
  r = 0;
  if (!a) return -1;
  while (1) {
    if (a & m) return r;
    r++;
    m <<= 1;
  }
}
/*}}}*/
char *tobin(int n, int x)/*{{{*/
{
  int i;
  int mask;
  static char buffer[64];
  for (i=0; i<n; i++) {
    mask = 1<<i;
    buffer[i] = (x & mask) ? '1' : '0';
  }
  buffer[n] = 0;
  return buffer;
}
/*}}}*/
void show_symbols_in_set(int ns, const char *symbols, int bitmap)/*{{{*/
{
  int i, mask;
  int first = 1;
  for (i=0; i<ns; i++) {
    mask = 1<<i;
    if (bitmap & mask) {
      if (!first) fprintf(stderr, ",");
      first = 0;
      fprintf(stderr, "%c", symbols[i]);
    }
  }
}
/*}}}*/
void setup_terminals(struct layout *lay)/*{{{*/
{
  int i;
  for (i=0; i<lay->nc; i++) {
    lay->cells[i].is_terminal = 1;
  }
}
/*}}}*/

/* ============================================================================ */

struct clusters *mk_clusters(int nc)/*{{{*/
{
  struct clusters *result;
  result = new(struct clusters);
  result->cells = new_array(unsigned char, nc);
  memset(result->cells, '.', nc);
  memset(result->total, 0, 256 * sizeof(short));
  return result;
}
/*}}}*/
void free_clusters(struct clusters *x)/*{{{*/
{
  free(x->cells);
  free(x);
}
/*}}}*/

/* ============================================================================ */

const struct constraint cons_all = {/*{{{*/
  .do_lines = 1,
  .do_subsets = 1,
  .do_onlyopt = 1,
  .max_partition_size = MAX_PARTITION_SIZE,
  .is_default = 1
};
/*}}}*/
const struct constraint cons_none = {/*{{{*/
  .do_lines = 0,
  .do_subsets = 0,
  .do_onlyopt = 0,
  .max_partition_size = 0,
  .is_default = 1
};
/*}}}*/

/* ============================================================================ */


