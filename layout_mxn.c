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
void layout_MxN(int M, int N, int x_layout, struct layout *lay, int options) /*{{{*/
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
  int NC, NG, NS;
  char buffer[32];

  lay->ns = NS = MN;
  NG = 3*MN;
  if (x_layout) NG += 2;
  lay->ng = NG;
  lay->nc = NC = MN*MN;
  lay->prows = MN + (N-1);
  lay->pcols = MN + (M-1);
  if (MN <= 9) {
    lay->symbols = symbols_9;
  } else if (MN <= 16) {
    lay->symbols = symbols_16;
  } else if (MN == 25) {
    lay->symbols = symbols_25;
  } else {
    fprintf(stderr, "No symbol table for MxN=%d\n", MN);
    exit(1);
  }
  lay->cells = new_array(struct cell, lay->nc);
  lay->groups = new_array(short, NG * NS);
  lay->is_block = new_array(char, NG);
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
          for (k=3; k<NDIM; k++) {
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
          lay->cells[ic].is_overlap = 0;
        }
      }
    }
  }
  lay->group_names = new_array(char *, NG);
  for (i=0; i<MN; i++) {
    char buffer[32];
    sprintf(buffer, "row-%c", 'A' + i);
    lay->group_names[i] = strdup(buffer);
    lay->is_block[i] = 0;
    sprintf(buffer, "col-%d", 1 + i);
    lay->group_names[i+MN] = strdup(buffer);
    lay->is_block[i+MN] = 0;
    sprintf(buffer, "blk-%c%d", 'A' + M*(i/M), 1 + N*(i%M));
    lay->group_names[i+2*MN] = strdup(buffer);
    lay->is_block[i+2*MN] = 1;
  }

  if (x_layout) {
    short *base0, *base1;
    int ci0, ci1;
    lay->group_names[NG-2] = strdup("diag-\\");
    lay->group_names[NG-1] = strdup("diag-/");
    base0 = lay->groups + NS*(NG-2);
    base1 = lay->groups + NS*(NG-1);
    ci0 = 0;
    ci1 = NS - 1;
    for (i=0; i<NS; i++) {
      lay->cells[ci0].group[3] = NG - 2;
      if (ci0 == ci1) {
        /* Central square if MN is odd */
        lay->cells[ci1].group[4] = NG - 1;
      } else {
        lay->cells[ci1].group[3] = NG - 1;
      }
      base0[i] = ci0;
      base1[i] = ci1;
      ci0 += (NS + 1);
      ci1 += (NS - 1);
    }
  }

  lay->n_thinlines = (MN - M) + (MN - N);
  lay->n_mediumlines = (M + 1) + (N + 1) - 4;
  lay->n_thicklines = 4;
  lay->thinlines = new_array(struct dline, lay->n_thinlines);
  lay->mediumlines = new_array(struct dline, lay->n_mediumlines);
  lay->thicklines = new_array(struct dline, lay->n_thicklines);
  j = m = n = 0;
  for (i=0; i<MN+1; i++) {
    struct dline *d;
    /* horizontals */
    if ((i==0) || (i==MN)) {
      d = lay->thicklines + j;
      j++;
    } else if ((i%M) == 0) {
      d = lay->mediumlines + n;
      n++;
    } else {
      d = lay->thinlines + m;
      m++;
    }
    d->x0 = 0, d->x1 = MN;
    d->y0 = d->y1 = i;
    /* verticals */
    if ((i==0) || (i==MN)) {
      d = lay->thicklines + j;
      j++;
    } else if ((i%N) == 0) {
      d = lay->mediumlines + n;
      n++;
    } else {
      d = lay->thinlines + m;
      m++;
    }
    d->x0 = d->x1 = i;
    d->y0 = 0, d->y1 = MN;
  }

  find_symmetries(lay, options);
}
/*}}}*/

void free_layout_lite(struct layout *lay)/*{{{*/
{
  free(lay->thinlines);
  free(lay->mediumlines);
  free(lay->thicklines);
  free(lay->groups);
  free(lay->group_names);
  free(lay->cells);
  if (lay->name) free(lay->name);
  free(lay->is_block);
}
/*}}}*/
void free_layout(struct layout *lay)/*{{{*/
{
  int i;
  free(lay->thinlines);
  free(lay->mediumlines);
  free(lay->thicklines);
  free(lay->groups);
  for (i=0; i<lay->ng; i++) {
    free(lay->group_names[i]);
  }
  free(lay->group_names);
  for (i=0; i<lay->nc; i++) {
    free(lay->cells[i].name);
  }
  free(lay->cells);
  free(lay->name);
  free(lay->is_block);
  free(lay);
}
/*}}}*/
