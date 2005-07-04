
/* TODO:
 * symmetrical removal of givens in pose mode
 * tidy up verbose output
 * hint mode : advance to the next allocate in a part-solved puzzle
 * 5-gattai puzzles and generalisations thereof
 * analyze where pose mode spends its time.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>


/* ============================================================================ */

/* To cope with interlocked puzzles where some cells may be in 6 logical groups. */
#define NDIM 6

struct cell {
  char *name;           /* cell name for verbose + debug output. */
  int index;            /* self-index (to track reordering during geographical sort.) */
  short prow, pcol;     /* coordinates for printing to text output */
  short rrow, rcol;     /* raw coordinates for printing to formatted output (SVG etc) */
  short sy180, sy90;    /* indices of cells that are 180/90 deg away clockwise (-1 for none) */
  short group[NDIM];    /* table of groups the cell is in (-1 for unused dimensions) */
};

struct dline
{
  /* Start and end of a line for formatted output. */
  short x0, y0;
  short x1, y1;
};

/* examples are for the regular 9x9 puzzle. */
struct layout {
  int n;                /* sqrt(ns) */
  int ns;               /* number of symbols, e.g. 9 (=groupsize)*/
  int nc;               /* number of cells, e.g. 81 */
  int ng;               /* number of groups, e.g. 27 */
  int prows;            /* #rows for generating print grid */
  int pcols;            /* #cols for generating print grid */
  
  /* For generating the lines on the formatted output. */
  int n_thinlines;
  struct dline *thinlines;
  int n_thicklines;
  struct dline *thicklines;

  const char *symbols;        /* [ns] table of the symbols */
  struct cell *cells;   /* [nc] table of cell definitions */
  short *groups;        /* [ng*ns] table of cell indices in each of the groups */
  char **group_names;    /* [ng] array of strings. */
};

struct subgrid {
  int yoff;
  int xoff;
  char *name;
};

enum corner {NW, NE, SE, SW};

struct subgrid_link {
  int index0;
  enum corner corner0;
  int index1;
  enum corner corner1;
};

struct super_layout {
  int n_subgrids;
  struct subgrid *subgrids;
  int n_links;
  struct subgrid_link *links;
};

/* ============================================================================ */

#define new_array(T, n) (T *) malloc((n) * sizeof(T))

#define OPT_VERBOSE (1<<0)
#define OPT_FIRST_ONLY (1<<1)
#define OPT_STOP_ON_2 (1<<2)
#define OPT_SPECULATE (1<<3)
#define OPT_SYM_180 (1<<4)
#define OPT_SYM_90 (1<<5)

/* ============================================================================ */

static void display(FILE *out, struct layout *lay, int *state);

/* ============================================================================ */

/* Routines that are generic. */

static int count_bits(unsigned int a)/*{{{*/
{
  a = (a & 0x55555555) + ((a>>1) & 0x55555555);
  a = (a & 0x33333333) + ((a>>2) & 0x33333333);
  a = (a & 0x0f0f0f0f) + ((a>>4) & 0x0f0f0f0f);
  a = (a & 0x00ff00ff) + ((a>>8) & 0x00ff00ff);
  a = (a & 0x0000ffff) + ((a>>16) & 0x0000ffff);
  return a;
}
/*}}}*/
static int decode(unsigned int a)/*{{{*/
{
  /* This needs optimising?? */

  int r, m;
  m = 1;
  r = 0;
  if (!a) return -1;
  while (1) {
    if (a & m) return r;
    r++;
    m <<= 1;
  }
}
/*}}}*/

static inline void allocate(struct layout *lay, int *state, int *poss, int *todo, int ic, int val)/*{{{*/
{
  int mask;
  int k;

  mask = 1<<val;
  state[ic] = val;
  poss[ic] = 0;

  for (k=0; k<NDIM; k++) {
    int gi = lay->cells[ic].group[k];
    if (gi >= 0) {
      todo[gi] &= ~mask;
    } else {
      break;
    }
  }
}
/*}}}*/
static char *tobin(int n, int x)/*{{{*/
{
  int i;
  int mask;
  static char buffer[64];
  for (i=0; i<n; i++) {
    mask = 1<<i;
    buffer[i] = (x & mask) ? '1' : '0';
  }
  buffer[n] = 0;
  return buffer;
}
/*}}}*/
static void show_symbols_in_set(int ns, const char *symbols, int bitmap)/*{{{*/
{
  int i, mask;
  int first = 1;
  for (i=0; i<ns; i++) {
    mask = 1<<i;
    if (bitmap & mask) {
      if (!first) fprintf(stderr, ",");
      first = 0;
      fprintf(stderr, "%c", symbols[i]);
    }
  }
}
/*}}}*/
/* ============================================================================ */
static int infer(struct layout *lay, int *state, int iter, int options)/*{{{*/
{
  /*
   * n : number of symbols to solve for (=size of each cell group)
   * nt : total number of cells (not necessarily n^2, e.g. for interlocked grid puzzles.)
   * cells : cell definition table.
   * symbols : the symbols (used for emitting solution data.)
   * state : the current state of the known cells. */

  /* A -1 entry in state[i] indicates a square that isn't resolved yet. */

  /* bitmaps of which symbols are as-yet unallocated across each group in each
   * dimension. */
  int *todo;
  int *poss; /* bitmap of which symbols could still go into each cell */

  int fill;
  int i, j, k, m;

  int any_not_solved;
  int did_something;

  fill = (1<<lay->ns) - 1;
  todo = new_array(int, lay->ng);
  for (i=0; i<lay->ng; i++) {
    todo[i] = fill;
  }

  poss = new_array(int, lay->nc);
  for (i=0; i<lay->nc; i++) poss[i] = fill;

  for (i=0; i<lay->nc; i++) {
    if (state[i] >= 0) { /* cell state known */
      poss[i] = 0;
    }
  }

  while (1) {
    
    if (options & OPT_VERBOSE) {
      display(stderr, lay, state);
    }

    iter++;
    any_not_solved = 0;
    did_something = 0;

    /* Now work out what can't fit into each cell. */
    for (i=0; i<lay->nc; i++) {
      if (state[i] >= 0) { /* cell state known */
        int mask = (1 << state[i]);
        for (j=0; j<lay->nc; j++) {
          if (j==i) continue;
          for (k=0; k<NDIM; k++) {
            int gi, gj;
            /* TODO : this is no use if we move to wierd layouts...?  The
             * matching groups may not be at the same index in the cell group
             * lists. */
            gi = lay->cells[i].group[k];
            gj = lay->cells[j].group[k];
            if ((gi == gj) && (gi >= 0)) {
              if (poss[j] & mask) {
                poss[j] &= ~mask;
              }
            }
          }
        }
        for (k=0; k<NDIM; k++) {
          int gi;
          gi = lay->cells[i].group[k];
          if (gi >= 0) {
            todo[gi] &= ~mask;
          } else {
            break;
          }
        }
      } else {
        any_not_solved = 1;
      }
    }

    if (!any_not_solved) {
      /* We're through. */
      return 1;
    }

    /* Detect any unsolved squares that have no options left : this means
     * the problem was insoluble or that we've speculated wrong earlier. */
    for (i=0; i<lay->nc; i++) {
      if ((state[i] < 0) && (poss[i] == 0)) {
        if (options & OPT_VERBOSE) {
          fprintf(stderr, "Stuck : no options for <%s>\n",
              lay->cells[i].name);
        }
        return 0;
      }
    }

    /* We apply the inference strategies in approx the same order that a human
     * would do.  Eventually, this will allow us to identify puzzles that require
     * a particular type of interference to solve them - e.g. easy puzzles will
     * only need the first strategy. */

    /* Now see what we can solve this time. */

    /* Try allocating unallocated symbols in each group (this is what a human
     * would do first?). */
    if (!did_something) {
      /* For each so-far unallocated symbol in each group, find out if there's a
       * unique cell that can take it and put it there if so. */
      for (k=0; k<lay->ng; k++) {
        int mask;
        int i;
        for (i=0; i<lay->ns; i++) {
          mask = 1<<i;
          if (todo[k] & mask) {
            int j;
            int count = 0;
            int free_ic = -1;
            for (j=0; j<lay->ns; j++) {
              int ic;
              ic = lay->groups[(k*lay->ns) + j];
              if (poss[ic] & mask) {
                free_ic = ic;
                count++;
              }
            }
            if (count == 0) {
              /* The solution has gone pear-shaped - this symbol has nowhere to go. */
              if (options & OPT_VERBOSE) {
                fprintf(stderr, "Cannot allocate <%c> in <%s>\n",
                    lay->symbols[i], lay->group_names[k]);
              }
              return 0;
            } else if (count == 1) {
              /* A unique cell in this group can still be allocated this symbol, do it. */
              if (options & OPT_VERBOSE) {
                fprintf(stderr, "%4d : Allocate <%c> to <%s> (allocate in <%s>)\n",
                    iter, lay->symbols[i], lay->cells[free_ic].name, lay->group_names[k]);
              }
              allocate(lay, state, poss, todo, free_ic, i);
              did_something = 1;
            }
          }
        }
      }
    }

    /* Look for cells that only have one possible symbol left. */
    if (1 || !did_something) {
      for (i=0; i<lay->nc; i++) {
        if (state[i] < 0) {
          if (count_bits(poss[i]) == 1) {
            /* Unique solution. */
            int sol = decode(poss[i]);
            if (options & OPT_VERBOSE) {
              fprintf(stderr, "%4d : Allocate <%c> to <%s> (only option)\n",
                  iter, lay->symbols[sol], lay->cells[i].name);
            }

            allocate(lay, state, poss, todo, i, sol);
            did_something = 1;
          }
        }
      }
    }

    if (!did_something) {
      int *counts;
      int count_size;
      int poss_cells;
      char *flags;
      count_size = NDIM * lay->ng;
      counts = new_array(int, count_size);
      flags = new_array(char, lay->nc);

      /* TODO: Analyse each group to find out which subset of cells can contain
       * each unallocated symbol.  If this subset is also a subset of some other
       * group, we can eliminate the symbol as a possibility from the rest of
       * that other group. */
      for (k=0; k<lay->ng; k++) {
        for (i=0; i<lay->ns; i++) {
          int mask = 1<<i;
          if (todo[k] & mask) { /* unallocated in this group. */
            memset(counts, 0, count_size * sizeof(int));
            memset(flags, 0, lay->nc);
            poss_cells = 0;
            for (j=0; j<lay->ns; j++) {
              int ic = lay->groups[k*lay->ns + j];
              if (poss[ic] & mask) {
                ++poss_cells;
                flags[ic] = 1;
                for (m=0; m<NDIM; m++) {
                  int kk = lay->cells[ic].group[m];
                  if (kk >= 0) {
                    if (kk != k) {
                      ++counts[kk];
                    }
                  } else {
                    break;
                  }
                }
              }
            }

            /* Now detect any subgroup of the right form. */
            for (m=0; m<lay->ng; m++) {
              if (counts[m] == poss_cells) {
                for (j=0; j<lay->ns; j++) {
                  int ic = lay->groups[m*lay->ns + j];
                  if (!flags[ic]) {
                    /* cell not in the original group. */
                    if (poss[ic] & mask) {
                      if (options & OPT_VERBOSE) {
                        fprintf(stderr, "%4d : Removing <%c> from <%s> (in <%s> due to placement of <%c> in <%s>)\n",
                            iter, lay->symbols[i], lay->cells[ic].name, lay->group_names[m],
                            lay->symbols[i], lay->group_names[k]);
                      }
                      poss[ic] &= ~mask;
                      did_something = 1;
                    }
                  }
                }
              }
            }
          }

        }
      }
      free(counts);
      free(flags);
    }

    if (!did_something) {
      /* TODO:
       * Deal with this case: suppose the symbols 2,3,5,6 are unallocated within
       * a group.  Suppose there are two cells
       *   A that could be 2,3,5
       *   B that could be 2,3,6
       * and neither 2 nor 3 could go anywhere else within the group under
       * analysis.  Then clearly we can eliminate 5 as a possibility on A and 6
       * as a possibility on B.
       * */

      /* [ns] : for each symbol, a bitmap of which cells in the current group
       * can still take the symbol. */
      int *poss_map;
      /* [ns] : for each symbol, the intersection of the possibilities on all the
       * cells that can still take this symbol. (i.e. for 2 & 3, this would be
       * {2,3} in the example above. */
      int *intersect;

      poss_map  = new_array(int, lay->ns);
      intersect = new_array(int, lay->ns);

      for (k=0; k<lay->ng; k++) {
        short *base;
        base = lay->groups + k*lay->ns;
        for (i=0; i<lay->ns; i++) { /* symbol */
          int mask = 1<<i;
          poss_map[i] = 0;
          intersect[i] = fill;
          for (j=0; j<lay->ns; j++) { /* cell in group */
            int ic = base[j];
            if (poss[ic] & mask) {
              intersect[i] &= poss[ic];
              poss_map[i] |= (1<<j);
            }
          }
        }

        /* Now look for subsets like the {2,3} case earlier. */
        for (i=0; i<lay->ns; i++) { /* symbol */
          if (count_bits(intersect[i]) == count_bits(poss_map[i])) {
            /* that is a necessary condition... */
            for (j=0; j<lay->ns; j++) {
              int mask = 1<<j;
              if (intersect[j] & mask) {
                if ((intersect[j] == intersect[i]) &&
                    (poss_map[j] == poss_map[i])) {
                  /* keep scanning... */
                } else {
                  goto try_next_symbol;
                }
              }
            }
          } else {
            goto try_next_symbol;
          }

          /* If control gets here, we have a good subset. */
          for (j=0; j<lay->ns; j++) {
            int ic = base[j]; /* cell number */
            int mask = 1<<i;
            if (poss[ic] & mask) {
              if (poss[ic] != intersect[i]) {
                /* i.e. actually removing some constraints rather than confirming
                 * what we already knew. */
                if (options & OPT_VERBOSE) {
                  fprintf(stderr, "%4d : Removing <", iter);
                  show_symbols_in_set(lay->ns, lay->symbols, poss[ic] & ~intersect[i]);
                  fprintf(stderr, "> from <%s> due to subgroup of <", lay->cells[ic].name);
                  show_symbols_in_set(lay->ns, lay->symbols, intersect[i]);
                  fprintf(stderr, ">\n");
                }
                poss[ic] = intersect[i];
                did_something = 1;
              }
            }
          }

try_next_symbol:
          (void) 0;
        }
      }
      free(poss_map);
      free(intersect);
    }

    if (!did_something) {

      if (options & OPT_SPECULATE) {
        int min_poss, index_min;
        /* Have to speculate and recurse.
         * TODO : ought to pick a cell with the lowest number of possibilities and iterate
         * over those?*/
        int ic;
        int n_solutions, n_sol;
        n_solutions = 0;
        index_min = -1;
        min_poss = lay->ns + 1;
        for (ic=0; ic<lay->nc; ic++) {
          int nposs = count_bits(poss[ic]);
          if ((nposs > 0) && (nposs < min_poss)) {
            min_poss = nposs;
            index_min = ic;
          }
        }
        if (index_min >= 0) {
          int *solution, *scratch;
          int i, mask;
          int start_point;
          int ii;
          solution = new_array(int, lay->nc);
          scratch = new_array(int, lay->nc);
          start_point = lrand48() % lay->ns;
          /* TODO : randomise start point? */
          for (i=0; i<lay->ns; i++) {
            ii = (start_point + i) % lay->ns;
            mask = 1<<ii;
            if (mask & poss[index_min]) {
              if (options & OPT_VERBOSE) {
                fprintf(stderr, "Speculate <%s> is <%c>\n",
                    lay->cells[index_min].name, lay->symbols[ii]);
              }
              memcpy(scratch, state, lay->nc * sizeof(int));
              scratch[index_min] = ii;
              n_sol = infer(lay, scratch, iter, options);
              if (n_sol > 0) {
                memcpy(solution, scratch, lay->nc * sizeof(int));
                n_solutions += n_sol;
                if (options & OPT_FIRST_ONLY) {
                  break;
                }
              }
              if ((options & OPT_STOP_ON_2) && (n_solutions >= 2)) {
                break;
              }
            }
          }
          free(scratch);
          memcpy(state, solution, lay->nc * sizeof(int));
          free(solution);
          return n_solutions;
        }
      } else {
        return 0;
      }
    } else {
      /* We made progress this time around : iterate again. */
    }
  }
}
/*}}}*/
/* ============================================================================ */

static void superlayout_5(struct super_layout *superlay)/*{{{*/
{
  superlay->n_subgrids = 5;
  superlay->n_links = 4;
  superlay->subgrids = new_array(struct subgrid, 5);
  superlay->links = new_array(struct subgrid_link, 4);
  superlay->subgrids[0] = (struct subgrid) {0,0,strdup("NW")};
  superlay->subgrids[1] = (struct subgrid) {0,2,strdup("NE")};
  superlay->subgrids[2] = (struct subgrid) {2,0,strdup("SW")};
  superlay->subgrids[3] = (struct subgrid) {2,2,strdup("SE")};
  superlay->subgrids[4] = (struct subgrid) {1,1,strdup("C")};

  superlay->links[0] = (struct subgrid_link) {0, SE, 4, NW};
  superlay->links[1] = (struct subgrid_link) {1, SW, 4, NE};
  superlay->links[2] = (struct subgrid_link) {2, NE, 4, SW};
  superlay->links[3] = (struct subgrid_link) {3, NW, 4, SE};
}
/*}}}*/
static void superlayout_8(struct super_layout *superlay)/*{{{*/
{
  superlay->n_subgrids = 8;
  superlay->n_links = 8;
  superlay->subgrids = new_array(struct subgrid, 8);
  superlay->links = new_array(struct subgrid_link, 8);
  superlay->subgrids[0] = (struct subgrid) {0,0,strdup("NW")};
  superlay->subgrids[1] = (struct subgrid) {0,2,strdup("N")};
  superlay->subgrids[2] = (struct subgrid) {0,4,strdup("NE")};
  superlay->subgrids[3] = (struct subgrid) {2,0,strdup("SW")};
  superlay->subgrids[4] = (struct subgrid) {2,2,strdup("S")};
  superlay->subgrids[5] = (struct subgrid) {2,4,strdup("S")};
  superlay->subgrids[6] = (struct subgrid) {1,1,strdup("CW")};
  superlay->subgrids[7] = (struct subgrid) {1,3,strdup("CE")};

  superlay->links[0] = (struct subgrid_link) {0, SE, 6, NW};
  superlay->links[1] = (struct subgrid_link) {1, SW, 6, NE};
  superlay->links[2] = (struct subgrid_link) {3, NE, 6, SW};
  superlay->links[3] = (struct subgrid_link) {4, NW, 6, SE};
  superlay->links[4] = (struct subgrid_link) {1, SE, 7, NW};
  superlay->links[5] = (struct subgrid_link) {2, SW, 7, NE};
  superlay->links[6] = (struct subgrid_link) {4, NE, 7, SW};
  superlay->links[7] = (struct subgrid_link) {5, NW, 7, SE};
}
/*}}}*/
static void superlayout_9(struct super_layout *superlay)/*{{{*/
{
  superlay->n_subgrids = 9;
  superlay->n_links = 12;
  superlay->subgrids = new_array(struct subgrid, superlay->n_subgrids);
  superlay->links = new_array(struct subgrid_link, superlay->n_links);
  superlay->subgrids[0] = (struct subgrid) {0,2,strdup("N")};
  superlay->subgrids[1] = (struct subgrid) {1,1,strdup("NW")};
  superlay->subgrids[2] = (struct subgrid) {1,3,strdup("NE")};
  superlay->subgrids[3] = (struct subgrid) {2,0,strdup("W")};
  superlay->subgrids[4] = (struct subgrid) {2,2,strdup("C")};
  superlay->subgrids[5] = (struct subgrid) {2,4,strdup("E")};
  superlay->subgrids[6] = (struct subgrid) {3,1,strdup("SW")};
  superlay->subgrids[7] = (struct subgrid) {3,3,strdup("SE")};
  superlay->subgrids[8] = (struct subgrid) {4,2,strdup("S")};

  superlay->links[0]  = (struct subgrid_link) {0, SW, 1, NE};
  superlay->links[1]  = (struct subgrid_link) {0, SE, 2, NW};
  superlay->links[2]  = (struct subgrid_link) {3, NE, 1, SW};
  superlay->links[3]  = (struct subgrid_link) {3, SE, 6, NW};
  superlay->links[4]  = (struct subgrid_link) {5, NW, 2, SE};
  superlay->links[5]  = (struct subgrid_link) {5, SW, 7, NE};
  superlay->links[6]  = (struct subgrid_link) {8, NW, 6, SE};
  superlay->links[7]  = (struct subgrid_link) {8, NE, 7, SW};
  superlay->links[8]  = (struct subgrid_link) {4, NW, 1, SE};
  superlay->links[9]  = (struct subgrid_link) {4, NE, 2, SW};
  superlay->links[10] = (struct subgrid_link) {4, SW, 6, NE};
  superlay->links[11] = (struct subgrid_link) {4, SE, 7, NW};
}
/*}}}*/
static void superlayout_11(struct super_layout *superlay)/*{{{*/
{
  superlay->n_subgrids = 11;
  superlay->n_links = 12;
  superlay->subgrids = new_array(struct subgrid, 11);
  superlay->links = new_array(struct subgrid_link, 12);
  superlay->subgrids[0]  = (struct subgrid) {0,0,strdup("NW")};
  superlay->subgrids[1]  = (struct subgrid) {0,2,strdup("NNW")};
  superlay->subgrids[2]  = (struct subgrid) {0,4,strdup("NNE")};
  superlay->subgrids[3]  = (struct subgrid) {0,6,strdup("NE")};
  superlay->subgrids[4]  = (struct subgrid) {2,0,strdup("SW")};
  superlay->subgrids[5]  = (struct subgrid) {2,2,strdup("SSW")};
  superlay->subgrids[6]  = (struct subgrid) {2,4,strdup("SSE")};
  superlay->subgrids[7]  = (struct subgrid) {2,6,strdup("SE")};
  superlay->subgrids[8]  = (struct subgrid) {1,1,strdup("CW")};
  superlay->subgrids[9]  = (struct subgrid) {1,3,strdup("CC")};
  superlay->subgrids[10] = (struct subgrid) {1,5,strdup("CE")};

  superlay->links[0]  = (struct subgrid_link) {0, SE,  8, NW};
  superlay->links[1]  = (struct subgrid_link) {1, SW,  8, NE};
  superlay->links[2]  = (struct subgrid_link) {4, NE,  8, SW};
  superlay->links[3]  = (struct subgrid_link) {5, NW,  8, SE};
  superlay->links[4]  = (struct subgrid_link) {1, SE,  9, NW};
  superlay->links[5]  = (struct subgrid_link) {2, SW,  9, NE};
  superlay->links[6]  = (struct subgrid_link) {5, NE,  9, SW};
  superlay->links[7]  = (struct subgrid_link) {6, NW,  9, SE};
  superlay->links[8]  = (struct subgrid_link) {2, SE, 10, NW};
  superlay->links[9]  = (struct subgrid_link) {3, SW, 10, NE};
  superlay->links[10] = (struct subgrid_link) {6, NE, 10, SW};
  superlay->links[11] = (struct subgrid_link) {7, NW, 10, SE};
}
/*}}}*/

/* ============================================================================ */
/* ============================================================================ */

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
static void find_symmetries(struct layout *lay)/*{{{*/
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
    }
  }
}
/*}}}*/
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
static void layout_NxN(int N, struct layout *lay) /*{{{*/
{
  /* This function is REQUIRED to return the cells in raster scan order.
   * Obviously this is necessary for the grid reader, but it's also
   * required for fusing the sub-grids together when setting up a
   * 5-gattai layout and similar horrors. */

  int i, j, m, n;
  int k;
  int N2 = N*N;
  char buffer[32];

  lay->n  = N;
  lay->ns = N2;
  lay->ng = 3*N2;
  lay->nc = N2*N2;
  lay->prows = lay->pcols = N2 + (N-1);
  if (N == 3) {
    lay->symbols = symbols_9;
  } else if (N == 4) {
    lay->symbols = symbols_16;
  } else if (N == 5) {
    lay->symbols = symbols_25;
  } else {
    fprintf(stderr, "No symbol table for N=%d\n", N);
    exit(1);
  }
  lay->cells = new_array(struct cell, N2*N2);
  lay->groups = new_array(short, (3*N2)*N2);
  for (i=0; i<N; i++) {
    for (j=0; j<N; j++) {
      int row = N*i+j;
      for (m=0; m<N; m++) {
        for (n=0; n<N; n++) {
          int col = N*m+n;
          int block = N*i+m;
          int ic = N2*row + col;
          sprintf(buffer, "%c%d", 'A'+row, 1+col);
          lay->cells[ic].name = strdup(buffer);
          lay->cells[ic].group[0] = row;
          lay->cells[ic].group[1] = N2 + col;
          lay->cells[ic].group[2] = 2*N2 + block;
          for (k=3; k<6; k++) {
            lay->cells[ic].group[k] = -1;
          }
          /* Put spacers every N rows/cols in the printout. */
          lay->cells[ic].prow = row + (row / N);
          lay->cells[ic].pcol = col + (col / N);
          lay->cells[ic].rrow = row;
          lay->cells[ic].rcol = col;
          lay->groups[N2*(row) + col] = ic;
          lay->groups[N2*(N2+col) + row] = ic;
          lay->groups[N2*(2*N2+block) + N*j + n] = ic;
        }
      }
    }
  }
  lay->group_names = new_array(char *, 3*N2);
  for (i=0; i<N2; i++) {
    char buffer[32];
    sprintf(buffer, "row-%c", 'A' + i);
    lay->group_names[i] = strdup(buffer);
    sprintf(buffer, "col-%d", 1 + i);
    lay->group_names[i+N2] = strdup(buffer);
    sprintf(buffer, "blk-%c%d", 'A' + N*(i/N), 1 + N*(i%N));
    lay->group_names[i+2*N2] = strdup(buffer);
  }

  lay->n_thinlines = 2 * (N2 - N);
  lay->n_thicklines = 2 * (N + 1);
  lay->thinlines = new_array(struct dline, lay->n_thinlines);
  lay->thicklines = new_array(struct dline, lay->n_thicklines);
  m = n = 0;
  for (i=0; i<N2+1; i++) {
    struct dline *d;
    if ((i%N) == 0) {
      d = lay->thicklines + n;
      n+=2;
    } else {
      d = lay->thinlines + m;
      m+=2;
    }
    d->x0 = 0, d->x1 = N2;
    d->y0 = d->y1 = i;
    d++;
    d->x0 = d->x1 = i;
    d->y0 = 0, d->y1 = N2;
  }

  find_symmetries(lay);
}
/*}}}*/

static int superlayout_cell_compare(const void *a, const void *b)/*{{{*/
{
  const struct cell *aa = (const struct cell *) a;
  const struct cell *bb = (const struct cell *) b;
  /* Sort cells into raster order, with all the overlapped cells pushed to the top end. */
  if (aa->index < 0 && bb->index >= 0) {
    return 1;
  } else if (aa->index >= 0 && bb->index < 0) {
    return -1;
  } else {
    if (aa->prow < bb->prow) {
      return -1;
    } else if (aa->prow > bb->prow) {
      return 1;
    } else if (aa->pcol < bb->pcol) {
      return -1;
    } else if (aa->pcol > bb->pcol) {
      return 1;
    } else {
      return 0;
    }
  }
}
/*}}}*/
static void fixup_lines(int n, struct dline *d, int xoff, int yoff)/*{{{*/
{
  int i;
  for (i=0; i<n; i++) {
    d[i].x0 += xoff;
    d[i].x1 += xoff;
    d[i].y0 += yoff;
    d[i].y1 += yoff;
  }
}
/*}}}*/
static void layout_N_superlay(int N, const struct super_layout *superlay, struct layout *lay)/*{{{*/
{
  struct layout *tlay;
  int nsg;
  int i;
  char buffer[64];
  int tng;
  int tnc;
  int tns;
  int *rmap;

  nsg = superlay->n_subgrids;
  tlay = new_array(struct layout, nsg);
  for (i=0; i<nsg; i++) {
    layout_NxN(N, tlay + i);
  }

  /* Relabel and reindex the tables. */
  for (i=0; i<nsg; i++) {
    struct subgrid *sg = superlay->subgrids + i;
    struct layout *ll = tlay + i;
    int group_base = i * ll->ng;
    int cell_base  = i * ll->nc;
    int j, k;
    for (j=0; j<ll->nc; j++) {
      struct cell *c = ll->cells + j;
      c->index = j + cell_base;
      sprintf(buffer, "%s:%s", sg->name, c->name);
      c->name = strdup(buffer);
      c->prow += (N-1)*(N+1) * sg->yoff;
      c->pcol += (N-1)*(N+1) * sg->xoff;
      c->rrow += sg->yoff * N*(N-1);
      c->rcol += sg->xoff * N*(N-1);

      for (k=0; k<NDIM; k++) {
        if (c->group[k] >= 0) {
          c->group[k] += group_base;
        } else {
          break;
        }
      }
    }
    for (j=0; j<ll->ng; j++) {
      for (k=0; k<ll->ns; k++) {
        ll->groups[j*ll->ns + k] += cell_base;
      }
      sprintf(buffer, "%s:%s", sg->name, ll->group_names[j]);
      ll->group_names[j] = strdup(buffer);
    }
    fixup_lines(ll->n_thinlines, ll->thinlines, N*(N-1)*sg->xoff, N*(N-1)*sg->yoff);
    fixup_lines(ll->n_thicklines, ll->thicklines, N*(N-1)*sg->xoff, N*(N-1)*sg->yoff);
  }

  /* Merge into one big table. */
  lay->n  = N;
  tns     = tlay[0].ns;
  lay->ns = tns;
  tng     = tlay[0].ng;
  lay->ng = tng * nsg;
  /* Number of cells excludes the overlaps. */
  tnc = tlay[0].nc;
  lay->nc = tnc * nsg - (superlay->n_links * tns);

  lay->symbols = tlay[0].symbols;
  lay->group_names = new_array(char *, lay->ng);
  lay->groups = new_array(short, lay->ng*lay->ns);

  /* This is oversized to start with - we just don't use the tail end of it later on. */
  lay->cells  = new_array(struct cell, tnc * nsg);

  lay->n_thicklines = nsg * tlay[0].n_thicklines;
  lay->n_thinlines = nsg * tlay[0].n_thinlines;
  lay->thicklines = new_array(struct dline, lay->n_thicklines);
  lay->thinlines = new_array(struct dline, lay->n_thinlines);
  
  for (i=0; i<nsg; i++) {
    void *addr;
    memcpy(lay->group_names + (tng * i),
           tlay[i].group_names,
           sizeof(char*) * tng);
    memcpy(lay->groups + (tng * tns * i),
           tlay[i].groups,
           sizeof(short) * tng * tns);
    memcpy(lay->cells + (tnc * i),
           tlay[i].cells,
           sizeof(struct cell) * tnc);
    memcpy(lay->thinlines + (tlay[0].n_thinlines * i),
           tlay[i].thinlines,
           sizeof(struct dline) * tlay[0].n_thinlines);
    memcpy(lay->thicklines + (tlay[0].n_thicklines * i),
           tlay[i].thicklines,
           sizeof(struct dline) * tlay[0].n_thicklines);
  }

  /* Now we can work purely on the master layout. */

  /* Merge cells in the overlapping blocks */
  for (i=0; i<superlay->n_links; i++) {
    struct subgrid_link *sgl = superlay->links + i;
    int m, n;
    struct layout *l0 = tlay + sgl->index0;
    struct layout *l1 = tlay + sgl->index1;
    int off0, off1;
    int N2 = N*N;
    switch (sgl->corner0) {
      case NW: off0 = 0; break;
      case NE: off0 = N*(N-1); break;
      case SW: off0 = N2*(N2-N); break;
      case SE: off0 = N2*(N2-N) + N*(N-1); break;
    }
    off0 += tnc * sgl->index0;
    switch (sgl->corner1) {
      case NW: off1 = 0; break;
      case NE: off1 = N*(N-1); break;
      case SW: off1 = N2*(N2-N); break;
      case SE: off1 = N2*(N2-N) + N*(N-1); break;
    }
    off1 += tnc * sgl->index1;

    for (m=0; m<N; m++) {
      for (n=0; n<N; n++) {
        int ic0, ic1;
        int q;
        struct cell *c0, *c1;
        ic0 = off0 + m*N2 + n;
        ic1 = off1 + m*N2 + n;
        c0 = lay->cells + ic0;
        c1 = lay->cells + ic1;
        /* Copy 2ary cell's groups into 1ary cell's table. */
        for (q=0; q<3; q++) {
          c0->group[3+q] = c1->group[q];
        }
        /* Merge cell names */
        sprintf(buffer, "%s/%s", c0->name, c1->name);
        c0->name = strdup(buffer);
        /* For each group c1 is in, change the index to point to c0 */
        for (q=0; q<3; q++) {
          int r;
          int grp = c1->group[q];
          for (r=0; r<tns; r++) {
            if (lay->groups[tns*grp + r] == ic1) {
              lay->groups[tns*grp + r] = ic0;
              break;
            }
          }
        }
        /* Mark c1 as being defunct */
        c1->index = -1;
      }
    }
  }

  /* Sort the remaining cells into geographical order. */
  qsort(lay->cells, tnc*nsg, sizeof(struct cell), superlayout_cell_compare);

  /* Build reverse mapping */
  rmap = new_array(int, tnc * nsg);
  for (i=0; i<tnc*nsg; i++) {
    rmap[i] = -1;
  }

  for (i=0; i<tnc*nsg; i++) {
    int idx;
    idx = lay->cells[i].index;
    if (idx >= 0) {
      rmap[idx] = i;
      /* Don't need to repair index fields of cells as they're not used after this. */
    }
  }
  /* Repair indexing (and in all groups) */
  for (i=0; i<lay->ns*lay->ng; i++) {
    int new_idx;
    new_idx = rmap[lay->groups[i]];
    if (new_idx >= 0) {
      lay->groups[i] = new_idx;
    } else {
      fprintf(stderr, "Oops.\n");
    }
  }

  free(rmap);
  /* Determine prows and pcols */
  lay->prows = 0;
  lay->pcols = 0;
  for (i=0; i<lay->nc; i++) {
    if (lay->cells[i].prow > lay->prows) lay->prows = lay->cells[i].prow;
    if (lay->cells[i].pcol > lay->pcols) lay->pcols = lay->cells[i].pcol;
  }
  ++lay->prows;
  ++lay->pcols;

  find_symmetries(lay);

}
/*}}}*/
static void debug_layout(struct layout *lay)/*{{{*/
{
  int i, j;
  for (i=0; i<lay->nc; i++) {
    fprintf(stderr, "%3d : %4d %4d %4d : %-16s ", 
        i,
        lay->cells[i].index,
        lay->cells[i].prow,
        lay->cells[i].pcol,
        lay->cells[i].name);
    for (j=0; j<NDIM; j++) {
      int kk = lay->cells[i].group[j];
      if (kk >= 0) {
        fprintf(stderr, " %d", kk);
      } else {
        break;
      }
    }
    fprintf(stderr, "\n");
  }
}
/*}}}*/

static void display(FILE *out, struct layout *lay, int *state)/*{{{*/
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
    if (state[i] < 0) {
      grid[idx] = '.';
    } else {
      grid[idx] = lay->symbols[state[i]];
    }
  }

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

static void read_grid(struct layout *lay, int *state)/*{{{*/
{
  int rmap[256];
  int valid[256];
  int i, c;

  for (i=0; i<256; i++) {
    rmap[i] = -1;
    valid[i] = 0;
  }
  for (i=0; i<lay->ns; i++) {
    rmap[lay->symbols[i]] = i;
    valid[lay->symbols[i]] = 1;
  }
  valid['.'] = 1;

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
static void solve(struct layout *lay, int options)/*{{{*/
{
  int *state;
  int i;
  int n_solutions;

  state = new_array(int, lay->nc);
  read_grid(lay, state);
  n_solutions = infer(lay, state, 0, options);

  if (n_solutions == 0) {
    fprintf(stderr, "The puzzle had no solutions\n");
    return;
  } else if (n_solutions == 1) {
    fprintf(stderr, "The puzzle had precisely 1 solution\n");

  } else {
    fprintf(stderr, "The puzzle had %d solutions (one is shown)\n", n_solutions);
  }

  display(stdout, lay, state);

  return;
}
/*}}}*/
static void solve_any(struct layout *lay, int options)/*{{{*/
{
  int *state;
  int i;
  int n_solutions;

  state = new_array(int, lay->nc);
  read_grid(lay, state);
  n_solutions = infer(lay, state, 0, OPT_SPECULATE | OPT_FIRST_ONLY | options);

  if (n_solutions == 0) {
    fprintf(stderr, "The puzzle had no solutions\n");
    return;
  }

  display(stdout, lay, state);

  return;
}
/*}}}*/
static int inner_reduce(struct layout *lay, int *state, int options)/*{{{*/
{
  int *copy, *answer;
  int *keep;
  int i;
  int ok;
  int tally;
  int kept_givens = 0;

  copy = new_array(int, lay->nc);
  answer = new_array(int, lay->nc);
  keep = new_array(int, lay->nc);

  memset(keep, 0, lay->nc * sizeof(int));
  tally = 0;
  for (i=0; i<lay->nc; i++) {
    if (state[i] >= 0) tally++;
  }

  /* Now remove givens one at a time until we find a minimum number that leaves
   * a puzzle with a unique solution. */
  memcpy(answer, state, lay->nc * sizeof(int));

  do {
    int start_point;
    int oi, oi0, oi1;
    start_point = lrand48() % lay->nc;
    ok = -1;
    oi = oi0 = oi1 = -1;
    for (i=0; i<lay->nc; i++) {
      int ii;
      int n_sol;

      ii = (i + start_point) % lay->nc;
      if (state[ii] < 0) continue;
      if (keep[ii] == 1) continue;
      oi = oi0 = oi1 = -1;
      if (options & OPT_SYM_180) {
        oi = lay->cells[ii].sy180;
        if (oi == ii) {
          oi = -1;
        } else if (options & OPT_SYM_90) {
          oi0 = lay->cells[ii].sy90;
          oi1 = lay->cells[oi].sy90;
        }
      }
      memcpy(copy, state, lay->nc * sizeof(int));
      copy[ii] = -1;
      /* If symmetric, remove those too */
      if (oi  >= 0) copy[oi]  = -1;
      if (oi0 >= 0) copy[oi0] = -1;
      if (oi1 >= 0) copy[oi1] = -1;
      if (options & OPT_SPECULATE) {
        n_sol = infer(lay, copy, 0, OPT_SPECULATE);
      } else {
        n_sol = infer(lay, copy, 0, OPT_STOP_ON_2);
      }
      tally--;
      if (n_sol == 1) {
        ok = ii;
        break;
      } else {
        /* If it's no good removing this given now, it won't be any better to try
         * removing it again later... */
        if (options & OPT_VERBOSE) {
          fprintf(stderr, "%4d :  (can't remove given from <%s>)\n", tally, lay->cells[ii].name);
        }
        keep[ii] = 1;
        ++kept_givens;
        if (oi >= 0) {
          keep[oi] = 1;
          ++kept_givens;
          if (options & OPT_VERBOSE) {
            fprintf(stderr, "%4d :  (can't remove given from <%s> (sym180))\n", tally, lay->cells[oi].name);
          }
          tally--;
        }
        if (oi0 >= 0) {
          keep[oi0] = 1;
          ++kept_givens;
          if (options & OPT_VERBOSE) {
            fprintf(stderr, "%4d :  (can't remove given from <%s> (sym90))\n", tally, lay->cells[oi0].name);
          }
          tally--;
        }
        if (oi1 >= 0) {
          keep[oi1] = 1;
          ++kept_givens;
          if (options & OPT_VERBOSE) {
            fprintf(stderr, "%4d :  (can't remove given from <%s> (sym90))\n", tally, lay->cells[oi1].name);
          }
          tally--;
        }
      }
    }

    if (ok >= 0) {
      state[ok] = -1;
      if (options & OPT_VERBOSE) {
        fprintf(stderr, "%4d : Removing given from <%s>\n", tally, lay->cells[ok].name);
      }

      if (oi >= 0) {
        state[oi] = -1;
        if (options & OPT_VERBOSE) {
          fprintf(stderr, "%4d : Removing given from <%s> (sym180)\n", tally, lay->cells[oi].name);
        }
        tally--;
      }
      if (oi0 >= 0) {
        state[oi0] = -1;
        if (options & OPT_VERBOSE) {
          fprintf(stderr, "%4d : Removing given from <%s> (sym90)\n", tally, lay->cells[oi0].name);
        }
        tally--;
      }
      if (oi1 >= 0) {
        state[oi1] = -1;
        if (options & OPT_VERBOSE) {
          fprintf(stderr, "%4d : Removing given from <%s> (sym90)\n", tally, lay->cells[oi1].name);
        }
        tally--;
      }
    }
  } while (ok >= 0);

  free(copy);
  free(answer);
  free(keep);

  return kept_givens;
}
/*}}}*/
static void reduce(struct layout *lay, int options)/*{{{*/
{
  int *state;
  int i;
  int ok;
  int tally;
  int kept_givens = 0;

  state = new_array(int, lay->nc);

  read_grid(lay, state);
  kept_givens = inner_reduce(lay, state, options);

  if (options & OPT_VERBOSE) {
    fprintf(stderr, "%d givens kept\n", kept_givens);
  }
  display(stdout, lay, state);

  return;
}
/*}}}*/
static void pose(struct layout *lay)/*{{{*/
{
  int *state, *copy, *answer;
  int *keep;
  int i;
  int ok;
  int tally;

  state = new_array(int, lay->nc);
  copy = new_array(int, lay->nc);
  answer = new_array(int, lay->nc);
  keep = new_array(int, lay->nc);

  for (i=0; i<lay->nc; i++) {
    state[i] = -1;
    keep[i] = 0;
  }

  if (1 != infer(lay, state, 0, OPT_SPECULATE | OPT_FIRST_ONLY)) {
    fprintf(stderr, "??? FAILED TO GENERATE AN INITIAL GRID!!\n");
    exit(1);
  }
  /* Must be at least 1 solution if the solver tries hard enough! */

  /* Now remove givens one at a time until we find a minimum number that leaves
   * a puzzle with a unique solution. */
  memcpy(answer, state, lay->nc * sizeof(int));

  tally = lay->nc;
  do {
    int start_point;
    start_point = lrand48() % lay->nc;
    ok = -1;
    for (i=0; i<lay->nc; i++) {
      int ii;
      int n_sol;
      ii = (i + start_point) % lay->nc;
      if (state[ii] < 0) continue;
      if (keep[ii] == 1) continue;
      memcpy(copy, state, lay->nc * sizeof(int));
      copy[ii] = -1;
      n_sol = infer(lay, copy, 0, OPT_STOP_ON_2);
      tally--;
      if (n_sol == 1) {
        ok = ii;
        break;
      } else {
        /* If it's no good removing this given now, it won't be any better to try
         * removing it again later... */
        fprintf(stderr, "%4d :  (can't remove given from <%s>)\n", tally, lay->cells[ii].name);
        keep[ii] = 1;
      }
    }

    if (ok >= 0) {
      fprintf(stderr, "%4d : Removing given from <%s>\n", tally, lay->cells[ok].name);
      state[ok] = -1;
    }
  } while (ok >= 0);

  display(stdout, lay, state);

  return;
}
/*}}}*/
static void blank(struct layout *lay)/*{{{*/
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

static void discover(int options)/*{{{*/
{
  /* Experimental code...  Look for cases that have unambiguous solutions, but
   * where the solver still has to speculate to find the solution.  Print out
   * the solution as far as it can get, so we can work out what inference
   * approach it is lacking.  */

  struct layout lay;
  int *state, *copy, *copy2;
  int i;
  int n_solutions;
  int min_n_givens;

  layout_NxN(3, &lay);
  state = new_array(int, lay.nc);
  copy = new_array(int, lay.nc);
  copy2 = new_array(int, lay.nc);
  for (i=0; i<lay.nc; i++) {
    state[i] = -1;
  }
  n_solutions = infer(&lay, state, 0, OPT_SPECULATE | OPT_FIRST_ONLY | options);
  if (n_solutions == 0) {
    fprintf(stderr, "oops, couldn't find a solution at the start!!\n");
    exit(1);
  }
  fprintf(stderr, "Complete puzzle:\n");
  display(stderr, &lay, state);

  min_n_givens = 81;
  do {
    int n_givens1, n_givens2;
    memcpy(copy, state, lay.nc * sizeof(int));
    n_givens1 = inner_reduce(&lay, copy, 0);
    memcpy(copy2, copy, lay.nc * sizeof(int));
    n_givens2 = inner_reduce(&lay, copy, OPT_SPECULATE);
    if (n_givens1 > n_givens2) {
      fprintf(stderr, "Puzzle is :\n");
      display(stderr, &lay, copy);
      fprintf(stderr, "Solvable puzzle is :\n");
      display(stderr, &lay, copy2);
      n_solutions = infer(&lay, copy, 0, OPT_VERBOSE);
      fprintf(stderr, "Can get this far without guessing:\n");
      display(stdout, &lay, copy);
      exit(0);
    }
    if (n_givens1 < min_n_givens) {
      min_n_givens = n_givens1;
    }
    fprintf(stderr, "n_givens = %d   min = %d\n", n_givens1, min_n_givens);
  } while (1);
}
/*}}}*/
static void format_emit_lines(int n, struct dline *d, double stroke_width)/*{{{*/
{
  int i;
  double scale, offset;

  scale = 72.27 / 2.54;
  offset = 2.0 * scale;

  for (i=0; i<n; i++) {
    double x0, x1, y0, y1;
    x0 = offset + scale * (double) d[i].x0;
    x1 = offset + scale * (double) d[i].x1;
    y0 = offset + scale * (double) d[i].y0;
    y1 = offset + scale * (double) d[i].y1;
    printf("<path style=\"fill:none;stroke:#000;stroke-width:%f;stroke-linecap:square;stroke-linejoin:miter;stroke-miterlimit:4.0;stroke-opacity:1.0\"\n", stroke_width);
    printf("d=\"M %f,%f L %f,%f\" />\n", x0, y0, x1, y1);
  }
}
/*}}}*/
static void format_output(struct layout *lay, int options)/*{{{*/
{
  int *state;
  double scale, offset;
  int i;

  state = new_array(int, lay->nc);
  read_grid(lay, state);
  
  scale = 72.27 / 2.54;
  offset = 2.0 * scale;
  
  printf("<?xml version=\"1.0\"?>\n");
  printf("<svg\n");

  printf("xmlns:svg=\"http://www.w3.org/2000/svg\"\n"
         "xmlns=\"http://www.w3.org/2000/svg\"\n"
         "id=\"svg2\"\n"
         "height=\"1052.3622\"\n"
         "width=\"744.09448\"\n"
         "y=\"0.0000000\"\n"
         "x=\"0.0000000\"\n"
         "version=\"1.0\">\n");
  printf("<defs\n"
         "id=\"defs3\" />\n");
  printf("<g id=\"layer1\">\n");
  format_emit_lines(lay->n_thinlines, lay->thinlines, 1.0);
  format_emit_lines(lay->n_thicklines, lay->thicklines, 3.0);
  for (i=0; i<lay->nc; i++) {
    if (state[i] >= 0) {
      double x, y;
        x = offset + scale * ((double) lay->cells[i].rcol + 0.5);
        y = offset + scale * ((double) lay->cells[i].rrow + 0.8);
        printf("<text style=\"font-size:24;font-style:normal;font-variant:normal;font-weight:bold;fill:#000;fill-opacity:1.0;stroke:none;font-family:Luxi Sans;text-anchor:middle;writing-mode:lr-tb\"\n");
        printf("x=\"%f\" y=\"%f\">%c</text>\n", x, y, lay->symbols[state[i]]);
    }
  }

  printf("</g>\n");
  printf("</svg>\n");
}
/*}}}*/

int main (int argc, char **argv)/*{{{*/
{
  int options;
  int seed;
  int N = 3;
  enum format {
    LAY_3x3,
    LAY_4x4,
    LAY_5x5,
    LAY_3x3x5,
    LAY_3x3x8,
    LAY_3x3x11,
    LAY_3x3x9,
    LAY_4x4x5
  } format;
  enum operation {
    OP_BLANK,     /* Generate a blank grid */
    OP_ANY,       /* Generate any solution to a partial grid */
    OP_REDUCE,    /* Remove givens until it's no longer possible without
                     leaving an ambiguous puzzle. */
    OP_POSE,
    OP_SOLVE,
    OP_DISCOVER,
    OP_FORMAT
  } operation;
  struct super_layout superlay;
  struct layout lay;
  
  format = LAY_3x3;
  operation = OP_SOLVE;

  options = 0;
  while (++argv, --argc) {
    if (!strcmp(*argv, "-v")) {
      options |= OPT_VERBOSE;
    } else if (!strcmp(*argv, "-f")) {
      options |= OPT_FIRST_ONLY;
    } else if (!strcmp(*argv, "-p")) {
      operation = OP_POSE;
    } else if (!strcmp(*argv, "-b")) {
      operation = OP_BLANK;
    } else if (!strcmp(*argv, "-a")) {
      operation = OP_ANY;
    } else if (!strcmp(*argv, "-r")) {
      operation = OP_REDUCE;
    } else if (!strcmp(*argv, "-s")) {
      options |= OPT_SPECULATE;
    } else if (!strcmp(*argv, "-F")) {
      operation = OP_FORMAT;
    } else if (!strcmp(*argv, "-y")) {
      options |= OPT_SYM_180;
    } else if (!strcmp(*argv, "-yy")) {
      options |= OPT_SYM_180 | OPT_SYM_90;
    } else if (!strcmp(*argv, "-d")) {
      operation = OP_DISCOVER;
    } else if (!strcmp(*argv, "-4")) {
      format = LAY_4x4;
    } else if (!strcmp(*argv, "-5")) {
      format = LAY_5x5;
    } else if (!strcmp(*argv, "-3/5")) {
      format = LAY_3x3x5;
    } else if (!strcmp(*argv, "-3/8")) {
      format = LAY_3x3x8;
    } else if (!strcmp(*argv, "-3/11")) {
      format = LAY_3x3x11;
    } else if (!strcmp(*argv, "-3/9")) {
      format = LAY_3x3x9;
    } else if (!strcmp(*argv, "-4/5")) {
      format = LAY_4x4x5;
    }
  }

  seed = time(NULL) ^ getpid();
  if (options & OPT_VERBOSE) {
    fprintf(stderr, "Seed=%d\n", seed);
  }
  srand48(seed);
  switch (format) {
    case LAY_3x3:
      layout_NxN(3, &lay);
      break;
    case LAY_4x4:
      layout_NxN(4, &lay);
      break;
    case LAY_5x5:
      layout_NxN(5, &lay);
      break;
    case LAY_3x3x5:
      superlayout_5(&superlay);
      layout_N_superlay(3, &superlay, &lay);
      break;
    case LAY_3x3x8:
      superlayout_8(&superlay);
      layout_N_superlay(3, &superlay, &lay);
      break;
    case LAY_3x3x11:
      superlayout_11(&superlay);
      layout_N_superlay(3, &superlay, &lay);
      break;
    case LAY_3x3x9:
      superlayout_9(&superlay);
      layout_N_superlay(3, &superlay, &lay);
      break;
    case LAY_4x4x5:
      superlayout_5(&superlay);
      layout_N_superlay(4, &superlay, &lay);
      break;
  }
  switch (operation) {
    case OP_SOLVE:
      solve(&lay, options);
      break;
    case OP_POSE:
      pose(&lay);
      break;
    case OP_ANY:
      solve_any(&lay, options);
      break;
    case OP_REDUCE:
      reduce(&lay, options);
      break;
    case OP_BLANK:
      blank(&lay);
      break;
    case OP_DISCOVER:
      discover(options);
      break;
    case OP_FORMAT:
      format_output(&lay, options);
      break;
  }
  return 0;
}
/*}}}*/

/* ============================================================================ */

