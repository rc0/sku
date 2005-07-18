#include "sku.h"

const static char symbols_8[8] = {/*{{{*/
  '1', '2', '3', '4', '5', '6', '7', '8'
};
/*}}}*/
const static char symbols_9[9] = {/*{{{*/
  '1', '2', '3', '4', '5', '6', '7', '8', '9'
};
/*}}}*/
const static char symbols_12[12] = {/*{{{*/
  '0', '1', '2', '3', '4', '5', '6', '7',
  '8', '9', 'A', 'B',
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
void layout_MxN(int M, int N, struct layout *lay) /*{{{*/
{
  /* This function is REQUIRED to return the cells in raster scan order.
   * Obviously this is necessary for the grid reader, but it's also
   * required for fusing the sub-grids together when setting up a
   * 5-gattai layout and similar horrors.
   
   M is the no. of rows in a block
   N is the no. of cols in a block
   */

  int i, j, m, n;
  int k;
  int MN = M*N;
  char buffer[32];

  lay->ns = MN;
  lay->ng = 3*MN;
  lay->nc = MN*MN;
  lay->prows = MN + (N-1);
  lay->pcols = MN + (M-1);
  if (MN == 8) {
    lay->symbols = symbols_8;
  } else if (MN == 9) {
    lay->symbols = symbols_9;
  } else if (MN == 12) {
    lay->symbols = symbols_12;
  } else if (MN == 16) {
    lay->symbols = symbols_16;
  } else if (MN == 5) {
    lay->symbols = symbols_25;
  } else {
    fprintf(stderr, "No symbol table for MxN=%d\n", MN);
    exit(1);
  }
  lay->cells = new_array(struct cell, lay->nc);
  lay->groups = new_array(short, (3*MN)*MN);
  for (i=0; i<N; i++) {
    for (j=0; j<M; j++) {
      int row = M*i+j;
      for (m=0; m<M; m++) {
        for (n=0; n<N; n++) {
          int col = N*m+n;
          int block = M*i+m;
          int ic = MN*row + col;
          sprintf(buffer, "%c%d", 'A'+row, 1+col);
          lay->cells[ic].name = strdup(buffer);
          lay->cells[ic].group[0] = row;
          lay->cells[ic].group[1] = MN + col;
          lay->cells[ic].group[2] = 2*MN + block;
          for (k=3; k<6; k++) {
            lay->cells[ic].group[k] = -1;
          }
          /* Put spacers every N rows/cols in the printout. */
          lay->cells[ic].prow = row + (row / M);
          lay->cells[ic].pcol = col + (col / N);
          lay->cells[ic].rrow = row;
          lay->cells[ic].rcol = col;
          lay->groups[MN*(row) + col] = ic;
          lay->groups[MN*(MN+col) + row] = ic;
          lay->groups[MN*(2*MN+block) + N*j + n] = ic;
        }
      }
    }
  }
  lay->group_names = new_array(char *, 3*MN);
  for (i=0; i<MN; i++) {
    char buffer[32];
    sprintf(buffer, "row-%c", 'A' + i);
    lay->group_names[i] = strdup(buffer);
    sprintf(buffer, "col-%d", 1 + i);
    lay->group_names[i+MN] = strdup(buffer);
    sprintf(buffer, "blk-%c%d", 'A' + M*(i/M), 1 + N*(i%M));
    lay->group_names[i+2*MN] = strdup(buffer);
  }

  lay->n_thinlines = (MN - M) + (MN - N);
  lay->n_thicklines = (M + 1) + (N + 1);
  lay->thinlines = new_array(struct dline, lay->n_thinlines);
  lay->thicklines = new_array(struct dline, lay->n_thicklines);
  m = n = 0;
  for (i=0; i<MN+1; i++) {
    struct dline *d;
    /* horizontals */
    if ((i%M) == 0) {
      d = lay->thicklines + n;
      n++;
    } else {
      d = lay->thinlines + m;
      m++;
    }
    d->x0 = 0, d->x1 = MN;
    d->y0 = d->y1 = i;
    /* verticals */
    if ((i%N) == 0) {
      d = lay->thicklines + n;
      n++;
    } else {
      d = lay->thinlines + m;
      m++;
    }
    d->x0 = d->x1 = i;
    d->y0 = 0, d->y1 = MN;
  }

  find_symmetries(lay);
}
/*}}}*/