
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* ============================================================================ */

/* To cope with interlocked puzzles where some cells may be in 6 logical groups. */
#define NDIM 6

struct cell {
  char *name;
  short group[NDIM];
};

/* examples are for the regular 9x9 puzzle. */
struct layout {
  int ns;               /* number of symbols, e.g. 9 (=groupsize)*/
  int nc;               /* number of cells, e.g. 81 */
  int ng;               /* number of groups, e.g. 27 */
  const char *symbols;        /* [ns] table of the symbols */
  struct cell *cells;   /* [nc] table of cell definitions */
  short *groups;        /* [ng*ns] table of cell indices in each of the groups */
  char **group_names;    /* [ng] array of strings. */
};

/* ============================================================================ */

#define new_array(T, n) (T *) malloc((n) * sizeof(T))

/* ============================================================================ */

static void print_9x9(int *state, char *symbols);

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
static int solve(struct layout *lay, int *state, int iter)/*{{{*/
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
    
    print_9x9(state, lay->symbols);

    iter++;
    any_not_solved = 0;
    did_something = 0;

#if 0
    fprintf(stderr, "%4d : Current options:\n", iter);
    for (i=0; i<lay->nc; i++) {
      fprintf(stderr, "  %4d %s : %s\n", i, lay->cells[i].name, tobin(lay->ns, poss[i]));
    }
#endif

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
#if 0
                fprintf(stderr, "%4d : Removing <%c> from cell <%s> (<%s> in <%s>)",
                    iter, lay->symbols[state[i]],
                    lay->cells[j].name, lay->cells[i].name,
                    lay->group_names[gi]);
#endif
#if 0
                fprintf(stderr, " leaving %s", tobin(lay->ns, poss[j]));
                fprintf(stderr, "\n");
                did_something = 1;
#endif
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
        fprintf(stderr, "Stuck : no options for <%s>\n",
            lay->cells[i].name);
        return 0;
      }
    }

    /* We apply the inference strategies in approx the same order that a human
     * would do.  Eventually, this will allow us to identify puzzles that require
     * a particular type of interference to solve them - e.g. easy puzzles will
     * only need the first strategy. */

    /* Now see what we can solve this time. */
    /* Look for cells that only have one possible symbol left. */
    for (i=0; i<lay->nc; i++) {
      if (state[i] < 0) {
        if (count_bits(poss[i]) == 1) {
          /* Unique solution. */
          int sol = decode(poss[i]);
          fprintf(stderr, "%4d : Allocate <%c> to <%s> (only option)\n",
              iter, lay->symbols[sol], lay->cells[i].name);
              
          allocate(lay, state, poss, todo, i, sol);
          did_something = 1;
        }
      }
    }

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
              fprintf(stderr, "Cannot allocate <%c> in <%s>\n",
                  lay->symbols[i], lay->group_names[k]);
              return 0;
            } else if (count == 1) {
              /* A unique cell in this group can still be allocated this symbol, do it. */
              fprintf(stderr, "%4d : Allocate <%c> to <%s> (allocate in <%s>)\n",
                  iter, lay->symbols[i], lay->cells[free_ic].name, lay->group_names[k]);
              allocate(lay, state, poss, todo, free_ic, i);
              did_something = 1;
            }
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
                      fprintf(stderr, "%4d : Removing <%c> from <%s> (in <%s> due to placement of <%c> in <%s>)\n",
                          iter, lay->symbols[i], lay->cells[ic].name, lay->group_names[m],
                          lay->symbols[i], lay->group_names[k]);
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
    }

    if (!did_something) {
      /* Have to speculate and recurse. */
      int ic;
      int n_solutions, n_sol;
      n_solutions = 0;
      for (ic=0; ic<lay->nc; ic++) {
        if (poss[ic]) {
          int *solution, *scratch;
          int i, mask;
          solution = new_array(int, lay->nc);
          scratch = new_array(int, lay->nc);
          /* TODO : randomise start point? */
          for (i=0; i<lay->ns; i++) {
            mask = 1<<i;
            if (mask & poss[ic]) {
              fprintf(stderr, "Speculate <%s> is <%c>\n",
                  lay->cells[ic].name, lay->symbols[i]);
              memcpy(scratch, state, lay->nc * sizeof(int));
              scratch[ic] = i;
              n_sol = solve(lay, scratch, iter);
              if (n_sol > 0) {
                memcpy(solution, scratch, lay->nc * sizeof(int));
                n_solutions += n_sol;
              }
            }
          }
          free(scratch);
          memcpy(state, solution, lay->nc * sizeof(int));
          free(solution);
          return n_solutions;
        }
      }
    }
    /* Iterate again. */
  }
}
/*}}}*/

/* ============================================================================ */
/* ============================================================================ */
/* ============================================================================ */

/* Routines specific to a 9x9 standard grid. */

const static char symbols_9[9] = {/*{{{*/
  '1', '2', '3', '4', '5', '6', '7', '8', '9'
};
/*}}}*/
static void layout_9x9(struct layout *lay) {/*{{{*/
  int i, j, m, n;
  int k;

  lay->ns = 9;
  lay->ng = 27;
  lay->nc = 81;
  lay->symbols = symbols_9;
  lay->cells = new_array(struct cell, 81);
  lay->groups = new_array(short, 27*9);
  for (i=0; i<3; i++) {
    for (j=0; j<3; j++) {
      int row = 3*i+j;
      for (m=0; m<3; m++) {
        for (n=0; n<3; n++) {
          int col = 3*m+n;
          int block = 3*i+m;
          int ic = 9*row + col;
          lay->cells[ic].name = new_array(char, 3);
          lay->cells[ic].name[0] = 'A' + row;
          lay->cells[ic].name[1] = '1' + col;
          lay->cells[ic].group[0] = row;
          lay->cells[ic].group[1] = 9 + col;
          lay->cells[ic].group[2] = 18 + block;
          for (k=3; k<6; k++) {
            lay->cells[ic].group[k] = -1;
          }
          lay->groups[9*(row) + col] = ic;
          lay->groups[9*(9+col) + row] = ic;
          lay->groups[9*(18+block) + 3*j + n] = ic;
        }
      }
    }
  }
  lay->group_names = new_array(char *, 27);
  for (i=0; i<9; i++) {
    char buffer[32];
    sprintf(buffer, "row %c", 'A' + i);
    lay->group_names[i] = strdup(buffer);
    sprintf(buffer, "col %c", '1' + i);
    lay->group_names[i+9] = strdup(buffer);
    sprintf(buffer, "blk %c%c", 'A' + (i/3), '1' + (i%3));
    lay->group_names[i+18] = strdup(buffer);
  }
}
/*}}}*/
static void debug_layout(struct layout *lay)/*{{{*/
{
  int i, j;
  for (i=0; i<lay->nc; i++) {
    fprintf(stderr, "%3d : %8s ", i, lay->cells[i].name);
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

static void print_9x9(int *state, char *symbols)/*{{{*/
{
  int i, j, k;
  i = 0;
  for (j=0; j<9; j++) {
    for (k=0; k<9; k++) {
      int s = state[i++];
      fprintf(stderr, "%1c", (s>=0) ? symbols[s] : '.');
      if (k==2 || k==5) fprintf(stderr, " ");
    }
    fprintf(stderr, "\n");
    if (j==2 || j==5) fprintf(stderr, "\n");
  }
  fprintf(stderr, "\n");
}
/*}}}*/

static void solve_9x9(void)/*{{{*/
{
  int state[81];
  int i, j, k;
  int c;
  int n_solutions;
  struct layout lay;

  layout_9x9(&lay);

#if 0
  debug_layout(&lay);
#endif

  for (i=0; i<81; i++) {
    int valid = 0;
    do {
      c = getchar();
      switch (c) {
        case '.': state[i] = -1; valid = 1; break;
        case '1': state[i] =  0; valid = 1; break;
        case '2': state[i] =  1; valid = 1; break;
        case '3': state[i] =  2; valid = 1; break;
        case '4': state[i] =  3; valid = 1; break;
        case '5': state[i] =  4; valid = 1; break;
        case '6': state[i] =  5; valid = 1; break;
        case '7': state[i] =  6; valid = 1; break;
        case '8': state[i] =  7; valid = 1; break;
        case '9': state[i] =  8; valid = 1; break;
        case EOF:
          fprintf(stderr, "Ran out of input data!\n");
          exit(1);
        default: valid = 0; break;
      }
    } while (!valid);
  }

  n_solutions = solve(&lay, state, 0);

  if (n_solutions == 0) {
    fprintf(stderr, "The puzzle had no solutions\n");
    return;
  } else if (n_solutions == 1) {
    fprintf(stderr, "The puzzle had precisely 1 solution\n");

  } else {
    fprintf(stderr, "The puzzle had %d solutions (one is shown)\n", n_solutions);
  }

  print_9x9(state, symbols_9);

  return;
}
/*}}}*/
int main (int argc, char **argv)/*{{{*/
{
  solve_9x9();
  return 0;
}
/*}}}*/
/* ============================================================================ */
