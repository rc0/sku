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

#define EDGE_OFFSET 0.15
#define NUMBER_OFFSET_L 0.05
#define NUMBER_OFFSET_C 0.25
#define NUMBER_OFFSET_R 0.45
#define NUMBER_OFFSET_V 0.15

static void get_coords(int idx, int dirn, const struct layout *lay, double *y, double *x)/*{{{*/
{
  /* dirn is the direction of the neighbour from which we have entered the cell. */
  double yy, xx;
  yy = (double) lay->cells[idx].rrow;
  xx = (double) lay->cells[idx].rcol;
  switch (dirn) {
    case 0: /* From the N : want NE corner */
      *y = yy + EDGE_OFFSET;
      *x = xx + 1.0 - EDGE_OFFSET;
      break;
    case 1: /* From the E : want SE corner */
      *y = yy + 1.0 - EDGE_OFFSET;
      *x = xx + 1.0 - EDGE_OFFSET;
      break;
    case 2: /* From the S : want SW corner */
      *y = yy + 1.0 - EDGE_OFFSET;
      *x = xx + EDGE_OFFSET;
      break;
    case 3: /* From the W : want NW corner */
      *y = yy + EDGE_OFFSET;
      *x = xx + EDGE_OFFSET;
      break;
    default:
      fprintf(stderr, "Bad dirn in get_coords\n");
      exit(1);
  }
}
/*}}}*/
static int fully_internal_p(int idx, const struct layout *lay, const struct clusters *clus)/*{{{*/
{
  /* Return true if the cell is surrounded by 8 neighbours all in the same
   * cluster. */
  int index_n, index_e, index_s, index_w;
  int index_nw, index_ne, index_se, index_sw;
  struct cell *cell;
  struct cell *cell_n;
  struct cell *cell_s;

  cell = lay->cells + idx;
  index_n = cell->nbr[0];
  index_e = cell->nbr[1];
  index_s = cell->nbr[2];
  index_w = cell->nbr[3];

  if ((index_n < 0) || (clus->cells[idx] != clus->cells[index_n])) return 0;
  if ((index_e < 0) || (clus->cells[idx] != clus->cells[index_e])) return 0;
  if ((index_s < 0) || (clus->cells[idx] != clus->cells[index_s])) return 0;
  if ((index_w < 0) || (clus->cells[idx] != clus->cells[index_w])) return 0;
  cell_n = lay->cells + cell->nbr[0];
  cell_s = lay->cells + cell->nbr[2];
  index_nw = cell_n->nbr[3];
  index_ne = cell_n->nbr[1];
  index_se = cell_s->nbr[1];
  index_sw = cell_s->nbr[3];
  if ((index_nw < 0) || (clus->cells[idx] != clus->cells[index_nw])) return 0;
  if ((index_ne < 0) || (clus->cells[idx] != clus->cells[index_ne])) return 0;
  if ((index_se < 0) || (clus->cells[idx] != clus->cells[index_se])) return 0;
  if ((index_sw < 0) || (clus->cells[idx] != clus->cells[index_sw])) return 0;
  return 1;
}
/*}}}*/
/*{{{ do_cell() */
static void do_cell(int start_idx,
        unsigned char *done,
        const struct layout *lay,
        const struct clusters *clus,
        struct cluster_coords *r)
{
  int n, nn;
  int dirn;
  double y, x;
  int idx;
  int got_new_cell;
  int i;
  int d1;
  int nidx;
  int rdirn;

  done[start_idx] = 1;
  if (clus->cells[start_idx] == '.') return;
  if (fully_internal_p(start_idx, lay, clus)) return;

  /* Must be looking at a NW corner of the region (due to the order in which we
   * scan the grid). */

  n = r->n_points;
  idx = start_idx;
  dirn = 3;

  if      (lay->cells[start_idx].nbr[2] >= 0) rdirn = 2;
  else if (lay->cells[start_idx].nbr[1] >= 0) rdirn = 1;
  else if (lay->cells[start_idx].nbr[0]) {
    fprintf(stderr, "do_cell(%s) : found an unscanned north neighbour!\n",
        lay->cells[start_idx].name);
    exit(1);
  } else if (lay->cells[start_idx].nbr[3]) {
    fprintf(stderr, "do_cell(%s) : found an unscanned west neighbour!\n",
        lay->cells[start_idx].name);
    exit(1);
  }
  else rdirn = -1; /* Must be a singleton; */

  get_coords(idx, dirn, lay, &y, &x);
  r->points[n].y = y, r->points[n].x = x + NUMBER_OFFSET_R, r->type[n] = 1;

  n++;

  nn = r->n_numbers;
  r->numbers[nn].y = y + NUMBER_OFFSET_V;
  r->numbers[nn].x = x + NUMBER_OFFSET_C;
  r->values[nn] = clus->total[clus->cells[start_idx]];
  r->n_numbers = nn + 1;

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
        dirn = -1; /* Special case to match rdirn */
        break;
    }
    if ((idx == start_idx) && (dirn == rdirn)) { /* got back to the start */
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
  result->n_numbers = 0;
  /* Max size is if each cluster is a singleton. */
  result->numbers = new_array(struct point, NC);
  result->values = new_array(int, NC);

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


