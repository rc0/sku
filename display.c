#include "sku.h"

void display(FILE *out, struct layout *lay, int *state)/*{{{*/
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
    if (state[i] == -2) {
      grid[idx] = '*';
    } else if (state[i] == -1) {
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

  free(grid);
}
/*}}}*/

