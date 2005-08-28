
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
  int *order;
  int i, j;
  struct layout *lay;
  int score;

  read_grid(&lay, &state, options);

  if (grey_cells > 0) {
    order = new_array(int, lay->nc);
    copy = new_array(int, lay->nc);
    memcpy(copy, state, lay->nc * sizeof(int));
    memset(order, 0, lay->nc * sizeof(int));
    
    setup_terminals(lay);
    infer(lay, copy, order, &score, OPT_SPECULATE | OPT_VERBOSE);
    weed_terminals(lay, order);

    shade = new_array(struct intpair, lay->nc);
    for (i=0; i<lay->nc; i++) {
      shade[i].a = i;
      shade[i].b = order[i];
    }
    qsort(shade, lay->nc, sizeof(struct intpair), compare_intpair);

    if (options & OPT_VERBOSE) {
      int pos = 1;
      fprintf(stderr, "Cells which provide no clues to solving others:\n");
      fprintf(stderr, "    N :  Ord :  Cell\n");
      for (i=0; i<lay->nc; i++) {
        int ix, ord;
        ix = shade[i].a;
        ord = shade[i].b;
        if (lay->cells[ix].is_terminal) {
          fprintf(stderr, "  %3d : %4d : <%s>\n", pos++, ord, lay->cells[ix].name);
        }
      }
    }
    
    for (i=0, j=0; i<grey_cells; i++) {
      int ic;
      do {
        ic = shade[j++].a;
      } while ((j < lay->nc) && (!lay->cells[ic].is_terminal));
      if (j >= lay->nc) {
        fprintf(stderr, "Didn't have enough terminal cells to allocate %d greys (allocated %d)\n", grey_cells, i);
        break;
      }
      state[ic] = -2;
    }
    free(order);
    free(copy);
    free(shade);
  }

  display(stdout, lay, state);

  free(state);
  free_layout(lay);
}
/*}}}*/

