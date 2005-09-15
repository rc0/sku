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

static void get_coords(int idx, int dirn, const struct layout *lay, double *y, double *x)/*{{{*/
{
  const double offset = 0.1;
  /* dirn is the direction of the neighbour from which we have entered the cell. */
  double yy, xx;
  yy = (double) lay->cells[idx].rrow;
  xx = (double) lay->cells[idx].rcol;
  switch (dirn) {
    case 0: /* From the N : want NE corner */
      *y = yy + offset;
      *x = xx + 1.0 - offset;
      break;
    case 1: /* From the E : want SE corner */
      *y = yy + 1.0 - offset;
      *x = xx + 1.0 - offset;
      break;
    case 2: /* From the S : want SW corner */
      *y = yy + 1.0 - offset;
      *x = xx + offset;
      break;
    case 3: /* From the W : want NW corner */
      *y = yy + offset;
      *x = xx + offset;
      break;
    default:
      fprintf(stderr, "Bad dirn in get_coords\n");
      exit(1);
  }
}
/*}}}*/
/*{{{ do_cell() */
static void do_cell(int start_idx,
        unsigned char *done,
        const struct layout *lay,
        const struct clusters *clus,
        struct cluster_coords *r)
{
  int n;
  int dirn;
  double y, x;
  int idx;
  int got_new_cell;
  int i;
  int d1;
  int nidx;

  done[start_idx] = 1;
  if (clus->cells[start_idx] == '.') return;
    
  /* Must be looking at a NW corner of the region (due to the order in which we
   * scan the grid). */

  n = r->n_points;
  idx = start_idx;
  dirn = 3;
  
  get_coords(idx, dirn, lay, &y, &x);
  r->points[n].y = y, r->points[n].x = x, r->type[n] = 1;
  n++;
  
  do {
    got_new_cell = 0;
    d1 = -1;
    nidx = -1;
    for (i=0; i<4; i++) {
      d1 = (dirn + i + 1) & 3;
      nidx = lay->cells[idx].nbr[d1];
      if ((nidx >= 0) && 
          (clus->cells[nidx] == clus->cells[idx])) {
        /* There's a neighbour in that direction. */
        got_new_cell = 1;
        done[nidx] = 1;
        break;
      }
    }
    switch (i) {
      case 0: /* left turn */
        get_coords(idx, dirn, lay, &y, &x);
        r->points[n].y = y, r->points[n].x = x, r->type[n] = 0;
        n++;
        idx = nidx;
        dirn = d1 ^ 2;
        break;
      case 1: /* straight on */
        /* Don't have to add any coords and dirn stays the same */
        idx = nidx;
        break;
      case 2: /* right turn */
        get_coords(idx, (dirn + 1) & 3, lay, &y, &x);
        r->points[n].y = y, r->points[n].x = x, r->type[n] = 0;
        n++;
        idx = nidx;
        dirn = (dirn + 1) & 3;
        break;
      case 3: /* about turn */
        get_coords(idx, (dirn + 1) & 3, lay, &y, &x);
        r->points[n].y = y, r->points[n].x = x, r->type[n] = 0;
        n++;
        get_coords(idx, (dirn + 2) & 3, lay, &y, &x);
        r->points[n].y = y, r->points[n].x = x, r->type[n] = 0;
        n++;
        idx = nidx;
        dirn ^= 2;
        break;
      case 4: /* singleton */
        get_coords(idx, (dirn + 1) & 3, lay, &y, &x);
        r->points[n].y = y, r->points[n].x = x, r->type[n] = 0;
        n++;
        get_coords(idx, (dirn + 2) & 3, lay, &y, &x);
        r->points[n].y = y, r->points[n].x = x, r->type[n] = 0;
        n++;
        get_coords(idx, (dirn + 3) & 3, lay, &y, &x);
        r->points[n].y = y, r->points[n].x = x, r->type[n] = 0;
        n++;
        get_coords(idx, dirn, lay, &y, &x);
        r->points[n].y = y, r->points[n].x = x, r->type[n] = 2;
        n++;
        break;
    }
    if ((idx == start_idx) && (dirn == 2)) { /* got back to the start */
      get_coords(idx, (dirn + 1) & 3, lay, &y, &x);
      r->points[n].y = y, r->points[n].x = x, r->type[n] = 2;
      n++;
      got_new_cell = 0;
    }
  } while (got_new_cell);

  r->n_points = n;
}
/*}}}*/
struct cluster_coords *mk_cluster_coords(const struct layout *lay, const struct clusters *clus)/*{{{*/
{
  struct cluster_coords *result;
  int max_points;
  unsigned char *cell_done;
  int NC, i;
  NC = lay->nc;
  result = new(struct cluster_coords);
  result->n_points = 0;
  max_points = 5*NC;
  result->points = new_array(struct point, max_points);
  result->type = new_array(unsigned char, max_points);

  cell_done = new_array(unsigned char, NC);
  memset(cell_done, 0, NC);
  for (i=0; i<NC; i++) {
    if (!cell_done[i]) {
      do_cell(i, cell_done, lay, clus, result);
    }
  }

  free(cell_done);
  return result;
}
/*}}}*/
void free_cluster_coords(struct cluster_coords *x)/*{{{*/
{
  free(x->points);
  free(x->type);
  free(x);
}
/*}}}*/


