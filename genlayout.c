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
    } else {
      lay->cells[i].sy90 = -1;
    }
  }
}
/*}}}*/
void debug_layout(struct layout *lay)/*{{{*/
{
  int i, j;
  for (i=0; i<lay->nc; i++) {
    fprintf(stderr, "%3d : %4d P=(%4d,%4d) R=(%4d,%4d) : %-16s ", 
        i,
        lay->cells[i].index,
        lay->cells[i].prow,
        lay->cells[i].pcol,
        lay->cells[i].rrow,
        lay->cells[i].rcol,
        lay->cells[i].name);
    for (j=0; j<NDIM; j++) {
      int kk = lay->cells[i].group[j];
      if (kk >= 0) {
        fprintf(stderr, " %d", kk);
      } else {
        break;
      }
    }
    if (lay->cells[i].sy180 >= 0) {
      fprintf(stderr, "  180:%d", lay->cells[i].sy180);
    }
    if (lay->cells[i].sy90 >= 0) {
      fprintf(stderr, "  90:%d", lay->cells[i].sy90);
    }
    fprintf(stderr, "\n");
  }
}
/*}}}*/
struct layout *genlayout(const char *name)/*{{{*/
{
  struct layout *result;
  struct super_layout superlay;
  result = new(struct layout);

  if (!strcmp(name, "2")) {
    layout_MxN(2, 2, result);
  } else if (!strcmp(name, "3")) {
    layout_MxN(3, 3, result);
  } else if (!strcmp(name, "4")) {
    layout_MxN(4, 4, result);
  } else if (!strcmp(name, "5")) {
    layout_MxN(5, 5, result);
  } else if (!strcmp(name, "23")) {
    layout_MxN(2, 3, result);
  } else if (!strcmp(name, "24")) {
    layout_MxN(2, 4, result);
  } else if (!strcmp(name, "26")) {
    layout_MxN(2, 6, result);
  } else if (!strcmp(name, "34")) {
    layout_MxN(3, 4, result);
  } else if (!strcmp(name, "3/5")) {
    superlayout_5(&superlay);
    layout_MxN_superlay(3, 3, &superlay, result);
  } else if (!strcmp(name, "3/8")) {
    superlayout_8(&superlay);
    layout_MxN_superlay(3, 3, &superlay, result);
  } else if (!strcmp(name, "3/9")) {
    superlayout_9(&superlay);
    layout_MxN_superlay(3, 3, &superlay, result);
  } else if (!strcmp(name, "3/11")) {
    superlayout_11(&superlay);
    layout_MxN_superlay(3, 3, &superlay, result);
  } else if (!strcmp(name, "4/5")) {
    superlayout_5(&superlay);
    layout_MxN_superlay(4, 4, &superlay, result);
  } else if (!strcmp(name, "24/5")) {
    superlayout_5(&superlay);
    layout_MxN_superlay(2, 4, &superlay, result);
  } else if (!strcmp(name, "34/5")) {
    superlayout_5(&superlay);
    layout_MxN_superlay(3, 4, &superlay, result);
  } else {
    fprintf(stderr, "Unrecognized layout name <%s>\n", name);
    exit(1);
  }

  result->name = strdup(name);
  return result;
}
/*}}}*/
