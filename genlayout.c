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
static void seek_and_insert(struct layout *lay, int i, int oy, int ox)/*{{{*/
{
  int isym = find_cell_by_yx(lay->cells, lay->nc, oy, ox);
  if (isym >= 0) {
    struct cell *c, *ci;
    c = lay->cells + i;
    ci = lay->cells + isym;
    if (c->isym < ci->isym) ci->isym = c->isym;
    else                    c->isym = ci->isym;
  }
}
/*}}}*/
void find_symmetries(struct layout *lay, int options)/*{{{*/
{
  int maxy, maxx;
  int i;
  int NC;
  NC = lay->nc;
  maxy = maxx = 0;
  for (i=0; i<NC; i++) {
    int y, x;
    y = lay->cells[i].rrow;
    x = lay->cells[i].rcol;
    if (y > maxy) maxy = y;
    if (x > maxx) maxx = x;
  }

  /* For searching, we know the cell array is sorted in y-major x-minor order. */
  for (i=0; i<lay->nc; i++) {
    lay->cells[i].isym = i; /* initial value. */
  }

  /* TODO : other logic in here to deal with other symmetry modes */
  for (i=0; i<NC; i++) {
    int y, x, oy, ox;
    struct cell *c = lay->cells + i;
    y = c->rrow;
    x = c->rcol;
    /* 180 degree symmetry */

    if (options & OPT_SYM_180) {
      oy = maxy - y;
      ox = maxx - x;
      seek_and_insert(lay, i, oy, ox);
    }

    if (options & OPT_SYM_90) {
      if (maxy == maxx) {
        /* 90 degree symmetry : nonsensical unless the grid's bounding box is square! */
        oy = x;
        ox = maxx - y;
        seek_and_insert(lay, i, oy, ox);
      }
    }

    if (options & OPT_SYM_HORIZ) {
      oy = y;
      ox = maxx - x;
      seek_and_insert(lay, i, oy, ox);
    }
      
    if (options & OPT_SYM_VERT) {
      oy = maxy - y;
      ox = x;
      seek_and_insert(lay, i, oy, ox);
    }
      
  }

  /* Now find equivalence classes : by working upwards, everything gets locked
   * to the lowest index in the same class. */
  for (i=0; i<NC; i++) {
    lay->cells[i].isym = lay->cells[lay->cells[i].isym].isym;
  }

  /* Now generate rings. */
  for (i=0; i<NC; i++) {
    if (lay->cells[i].isym == i) { /* root of this class. */
      int j;
      int prev = i;
      for (j=i+1; j<NC; j++) {
        if (lay->cells[j].isym == i) {
          lay->cells[prev].isym = j;
          prev = j;
        }
      }
      lay->cells[prev].isym = i;
    }
  }
}
/*}}}*/
void debug_layout(struct layout *lay)/*{{{*/
{
  int i, j;
  int count;
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
    j = lay->cells[i].isym;
    fprintf(stderr, "  SYM :");
    count = 0;
    while (j != i) {
      fprintf(stderr, " %s", lay->cells[j].name);
      j = lay->cells[j].isym;
      count++;
      if (count > 8) {
        fprintf(stderr, "\nINFINITE LOOP!\n");
        exit(1);
      }
    }
    fprintf(stderr, "\n");
  }
}
/*}}}*/

static void parse_mn(const char *x, int len, int *M, int *N)/*{{{*/
{
  switch (len) {
    case 1:
      *M = *N = x[0] - '0';
      break;
    case 2:
      *M = x[0] - '0';
      *N = x[1] - '0';
      break;
    default:
      fprintf(stderr, "Can't parse rows and columns from %s\n", x);
      exit(1);
      break;
  }
}
/*}}}*/
static void make_super(const char *x, struct super_layout *superlay)/*{{{*/
{
  if (!strcmp(x, "5")) {
    superlayout_5(superlay);
  } else if (!strcmp(x, "8")) {
    superlayout_8(superlay);
  } else if (!strcmp(x, "9")) {
    superlayout_9(superlay);
  } else if (!strcmp(x, "11")) {
    superlayout_11(superlay);
  } else {
    fprintf(stderr, "Unknown superlayout %s\n", x);
    exit(1);
  }
}
/*}}}*/
struct layout *genlayout(const char *name, int options)/*{{{*/
{
  struct layout *result;
  struct super_layout superlay;
  const char *slash;
  int M, N;

  result = new(struct layout);

  slash = strchr(name, '/');
  if (slash) {
    parse_mn(name, slash-name, &M, &N);
    make_super(slash+1, &superlay);
    layout_MxN_superlay(M, N, &superlay, result, options);
  } else {
    parse_mn(name, strlen(name), &M, &N);
    layout_MxN(M, N, result, options);
  }
  result->name = strdup(name);
  return result;
}
/*}}}*/
