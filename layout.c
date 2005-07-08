#include "sku.h"

static int find_cell_by_yx(struct cell *c, int n, int y, int x)/*{{{*/
{
  int h, m, l;
  int mx, my, lx, ly;
  h = n;
  l = 0;
  while (h - l > 1) {
    m = (h + l) >> 1;
    mx = c[m].rcol;
    my = c[m].rrow;
    if ((my > y) || ((my == y) && (mx > x))) {
      h = m;
    } else if ((my <= y) || ((my == y) && (mx <= x))) {
      l = m;
    }
  }
  lx = c[l].rcol;
  ly = c[l].rrow;
  if ((lx == x) && (ly == y)) return l;
  else return -1;
}
/*}}}*/
void find_symmetries(struct layout *lay)/*{{{*/
{
  int maxy, maxx;
  int i;
  maxy = maxx = 0;
  for (i=0; i<lay->nc; i++) {
    int y, x;
    y = lay->cells[i].rrow;
    x = lay->cells[i].rcol;
    if (y > maxy) maxy = y;
    if (x > maxx) maxx = x;
  }

  /* For searching, we know the cell array is sorted in y-major x-minor order. */
  for (i=0; i<lay->nc; i++) {
    int y, x, oy, ox;
    y = lay->cells[i].rrow;
    x = lay->cells[i].rcol;
    /* 180 degree symmetry */
    oy = maxy - y;
    ox = maxx - x;
    lay->cells[i].sy180 = find_cell_by_yx(lay->cells, lay->nc, oy, ox);
    if (maxy == maxx) {
      /* 90 degree symmetry : nonsensical unless the grid's bounding box is square! */
      oy = x;
      ox = maxx - y;
      lay->cells[i].sy90 = find_cell_by_yx(lay->cells, lay->nc, oy, ox);
    }
  }
}
/*}}}*/
const static char symbols_9[9] = {/*{{{*/
  '1', '2', '3', '4', '5', '6', '7', '8', '9'
};
/*}}}*/
const static char symbols_16[16] = {/*{{{*/
  '0', '1', '2', '3', '4', '5', '6', '7',
  '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'
};
/*}}}*/
const static char symbols_25[25] = {/*{{{*/
  'A', 'B', 'C', 'D', 'E',
  'F', 'G', 'H', 'J', 'K',
  'L', 'M', 'N', 'O', 'P',
  'Q', 'R', 'S', 'T', 'U',
  'V', 'W', 'X', 'Y', 'Z'
};
/*}}}*/
void layout_NxN(int N, struct layout *lay) /*{{{*/
{
  /* This function is REQUIRED to return the cells in raster scan order.
   * Obviously this is necessary for the grid reader, but it's also
   * required for fusing the sub-grids together when setting up a
   * 5-gattai layout and similar horrors. */

  int i, j, m, n;
  int k;
  int N2 = N*N;
  char buffer[32];

  lay->n  = N;
  lay->ns = N2;
  lay->ng = 3*N2;
  lay->nc = N2*N2;
  lay->prows = lay->pcols = N2 + (N-1);
  if (N == 3) {
    lay->symbols = symbols_9;
  } else if (N == 4) {
    lay->symbols = symbols_16;
  } else if (N == 5) {
    lay->symbols = symbols_25;
  } else {
    fprintf(stderr, "No symbol table for N=%d\n", N);
    exit(1);
  }
  lay->cells = new_array(struct cell, N2*N2);
  lay->groups = new_array(short, (3*N2)*N2);
  for (i=0; i<N; i++) {
    for (j=0; j<N; j++) {
      int row = N*i+j;
      for (m=0; m<N; m++) {
        for (n=0; n<N; n++) {
          int col = N*m+n;
          int block = N*i+m;
          int ic = N2*row + col;
          sprintf(buffer, "%c%d", 'A'+row, 1+col);
          lay->cells[ic].name = strdup(buffer);
          lay->cells[ic].group[0] = row;
          lay->cells[ic].group[1] = N2 + col;
          lay->cells[ic].group[2] = 2*N2 + block;
          for (k=3; k<6; k++) {
            lay->cells[ic].group[k] = -1;
          }
          /* Put spacers every N rows/cols in the printout. */
          lay->cells[ic].prow = row + (row / N);
          lay->cells[ic].pcol = col + (col / N);
          lay->cells[ic].rrow = row;
          lay->cells[ic].rcol = col;
          lay->groups[N2*(row) + col] = ic;
          lay->groups[N2*(N2+col) + row] = ic;
          lay->groups[N2*(2*N2+block) + N*j + n] = ic;
        }
      }
    }
  }
  lay->group_names = new_array(char *, 3*N2);
  for (i=0; i<N2; i++) {
    char buffer[32];
    sprintf(buffer, "row-%c", 'A' + i);
    lay->group_names[i] = strdup(buffer);
    sprintf(buffer, "col-%d", 1 + i);
    lay->group_names[i+N2] = strdup(buffer);
    sprintf(buffer, "blk-%c%d", 'A' + N*(i/N), 1 + N*(i%N));
    lay->group_names[i+2*N2] = strdup(buffer);
  }

  lay->n_thinlines = 2 * (N2 - N);
  lay->n_thicklines = 2 * (N + 1);
  lay->thinlines = new_array(struct dline, lay->n_thinlines);
  lay->thicklines = new_array(struct dline, lay->n_thicklines);
  m = n = 0;
  for (i=0; i<N2+1; i++) {
    struct dline *d;
    if ((i%N) == 0) {
      d = lay->thicklines + n;
      n+=2;
    } else {
      d = lay->thinlines + m;
      m+=2;
    }
    d->x0 = 0, d->x1 = N2;
    d->y0 = d->y1 = i;
    d++;
    d->x0 = d->x1 = i;
    d->y0 = 0, d->y1 = N2;
  }

  find_symmetries(lay);
}
/*}}}*/

void debug_layout(struct layout *lay)/*{{{*/
{
  int i, j;
  for (i=0; i<lay->nc; i++) {
    fprintf(stderr, "%3d : %4d %4d %4d : %-16s ", 
        i,
        lay->cells[i].index,
        lay->cells[i].prow,
        lay->cells[i].pcol,
        lay->cells[i].name);
    for (j=0; j<NDIM; j++) {
      int kk = lay->cells[i].group[j];
      if (kk >= 0) {
        fprintf(stderr, " %d", kk);
      } else {
        break;
      }
    }
    fprintf(stderr, "\n");
  }
}
/*}}}*/
