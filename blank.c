#include "sku.h"

void blank(struct layout *lay)/*{{{*/
{
  int *state;
  int i;
  state = new_array(int, lay->nc);
  for (i=0; i<lay->nc; i++) {
    state[i] = -1;
  }
  display(stdout, lay, state);
}
/*}}}*/
