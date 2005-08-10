
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

static void weed_terminals(struct layout *lay, int *order)/*{{{*/
{
  char *dead_terminal;
  int i, j;
  dead_terminal = new_array(char, lay->nc);
  memset(dead_terminal, 0, lay->nc * sizeof(char));
#if 0
  fprintf(stderr, "The following cells are terminals:\n");
  for (i=0; i<lay->nc; i++) {
    if (lay->cells[i].is_terminal) {
      fprintf(stderr, "  %4d : %s\n", order[i], lay->cells[i].name);
    }
  }
#endif
  for (i=0; i<lay->ng; i++) {
    short *base = lay->groups + lay->ns * i;
    int highest = -1;
    for (j=0; j<lay->ns; j++) {
      int jc = base[j];
      if (lay->cells[jc].is_terminal) {
        if (highest < order[jc]) {
          highest = order[jc];
        }
      }
    }
    for (j=0; j<lay->ns; j++) {
      int jc = base[j];
      if (lay->cells[jc].is_terminal) {
        if (highest > order[jc]) {
          dead_terminal[jc] = 1;
        }
      }
    }
  }
  for (i=0; i<lay->nc; i++) {
    if (dead_terminal[i]) {
#if 0
      fprintf(stderr, "Weeding terminal <%s>\n", lay->cells[i].name);
#endif
      lay->cells[i].is_terminal = 0;
    }
  }

  free(dead_terminal);
#if 0
  fprintf(stderr, "The following cells remain as terminals:\n");
  for (i=0; i<lay->nc; i++) {
    if (lay->cells[i].is_terminal) {
      fprintf(stderr, "  %4d : %s\n", order[i], lay->cells[i].name);
    }
  }
#endif
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

  read_grid(&lay, &state, options);

  if (grey_cells > 0) {
    order = new_array(int, lay->nc);
    copy = new_array(int, lay->nc);
    memcpy(copy, state, lay->nc * sizeof(int));
    memset(order, 0, lay->nc * sizeof(int));
    
    for (i=0; i<lay->nc; i++) {
      lay->cells[i].is_terminal = 1;
    }
    infer(lay, copy, order, 0, 0, OPT_SPECULATE);
    weed_terminals(lay, order);

    shade = new_array(struct intpair, lay->nc);
    for (i=0; i<lay->nc; i++) {
      shade[i].a = i;
      shade[i].b = order[i];
    }
    qsort(shade, lay->nc, sizeof(struct intpair), compare_intpair);

    for (i=0; i<lay->nc; i++) {
      int ix, ord;
      ix = shade[i].a;
      ord = shade[i].b;
      if (lay->cells[ix].is_terminal) {
        fprintf(stderr, "%4d : <%s>\n", ord, lay->cells[ix].name);
      }
    }
    exit(0);
    
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

