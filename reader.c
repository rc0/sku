#include "sku.h"

void read_grid(struct layout *lay, int *state)/*{{{*/
{
  int rmap[256];
  int valid[256];
  int i, c;

  for (i=0; i<256; i++) {
    rmap[i] = -1;
    valid[i] = 0;
  }
  rmap['*'] = -2;
  for (i=0; i<lay->ns; i++) {
    rmap[lay->symbols[i]] = i;
    valid[lay->symbols[i]] = 1;
  }
  valid['.'] = 1;
  valid['*'] = 1;

  for (i=0; i<lay->nc; i++) {
    do {
      c = getchar();
      if (c == EOF) {
        fprintf(stderr, "Ran out of input data!\n");
        exit(1);
      }
      if (valid[c]) {
        state[i] = rmap[c];
      }
    } while (!valid[c]);
  }
}
/*}}}*/
