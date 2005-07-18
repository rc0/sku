
#include "sku.h"

struct intpair {/*{{{*/
  int a;
  int b;
};
/*}}}*/
static int compare_intpair(const void *a, const void *b)/*{{{*/
{
  const struct intpair *aa = (const struct intpair *) a;
  const struct intpair *bb = (const struct intpair *) b;
  if (aa->b < bb->b) return 1;
  else if (aa->b >  bb->b) return -1;
  else return 0;
}
/*}}}*/
void mark_cells(int grey_cells, int options)/*{{{*/
{
  int *state;
  int *copy;
  struct intpair *shade = NULL;
  int *flags;
  int *order;
  int i, j, k, n;
  struct layout *lay;

  read_grid(&lay, &state);

  if (grey_cells > 0) {
    int max_order;
    order = new_array(int, lay->nc);
    copy = new_array(int, lay->nc);
    memcpy(copy, state, lay->nc * sizeof(int));
    memset(order, 0, lay->nc * sizeof(int));
    infer(lay, copy, order, 0, 0, OPT_SPECULATE);
    shade = new_array(struct intpair, lay->nc);
    for (i=0; i<lay->nc; i++) {
      shade[i].a = i;
      shade[i].b = order[i];
    }
    qsort(shade, lay->nc, sizeof(struct intpair), compare_intpair);
    flags = new_array(int, lay->nc);
    memset(flags, 0, lay->nc * sizeof(int));
    for (i=0, j=0; i<grey_cells; i++) {
      int ic;
      do {
        ic = shade[j++].a;
      } while (flags[ic]);
      state[ic] = -2;
      for (k=0; k<NDIM; k++) {
        int gx = lay->cells[ic].group[k];
        if (gx >= 0) {
          for (n=0; n<lay->ns; n++) {
            int oic = lay->groups[gx*lay->ns + n];
            flags[oic] = 1;
          }
        } else {
          break;
        }
      }
    }
    free(order);
    free(copy);
    free(flags);
  }

  display(stdout, lay, state);

  free(state);
}
/*}}}*/

