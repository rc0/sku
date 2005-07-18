#include "sku.h"

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
