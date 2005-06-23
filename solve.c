
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
  char *name;
  short group[NDIM];
};

/* examples are for the regular 9x9 puzzle. */
struct layout {
  int n;                /* sqrt(ns) */
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

#define OPT_VERBOSE (1<<0)
#define OPT_FIRST_ONLY (1<<1)
#define OPT_STOP_ON_2 (1<<2)
#define OPT_SPECULATE (1<<3)

/* ============================================================================ */

static void print_NxN(FILE *out, int N, int *state, const char *symbols);

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
static int solve(struct layout *lay, int *state, int iter, int options)/*{{{*/
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
      print_NxN(stderr, lay->n, state, lay->symbols);
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
        /* Have to speculate and recurse.
         * TODO : ought to pick a cell with the lowest number of possibilities and iterate
         * over those?*/
        int ic;
        int n_solutions, n_sol;
        n_solutions = 0;
        for (ic=0; ic<lay->nc; ic++) {
          if (poss[ic]) {
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
              if (mask & poss[ic]) {
                if (options & OPT_VERBOSE) {
                  fprintf(stderr, "Speculate <%s> is <%c>\n",
                      lay->cells[ic].name, lay->symbols[ii]);
                }
                memcpy(scratch, state, lay->nc * sizeof(int));
                scratch[ic] = ii;
                n_sol = solve(lay, scratch, iter, options);
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
/* ============================================================================ */
/* ============================================================================ */

/* Routines specific to a 9x9 standard grid. */

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
static void layout_NxN(int N, struct layout *lay) {/*{{{*/
  int i, j, m, n;
  int k;
  int N2 = N*N;
  char buffer[32];

  lay->n  = N;
  lay->ns = N2;
  lay->ng = 3*N2;
  lay->nc = N2*N2;
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
    sprintf(buffer, "row %c", 'A' + i);
    lay->group_names[i] = strdup(buffer);
    sprintf(buffer, "col %d", 1 + i);
    lay->group_names[i+N2] = strdup(buffer);
    sprintf(buffer, "blk %c%d", 'A' + N*(i/N), 1 + N*(i%N));
    lay->group_names[i+2*N2] = strdup(buffer);
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

static void print_NxN(FILE *out, int N, int *state, const char *symbols)/*{{{*/
{
  int i, j, k;
  int N2 = N*N;
  i = 0;
  for (j=0; j<N2; j++) {
    for (k=0; k<N2; k++) {
      int s = state[i++];
      if ((k>0) && ((k % N) == 0)) fprintf(out, " ");
      fprintf(out, "%1c", (s>=0) ? symbols[s] : '.');
    }
    fprintf(out, "\n");
    if ((j % N) == (N-1)) fprintf(out, "\n");

  }
  fprintf(out, "\n");
}
/*}}}*/

static void solve_NxN(int N, int options)/*{{{*/
{
  /* e.g. N=3, 4 */
  int *state;
  int N2, N4;
  int i, j, k;
  int c;
  int n_solutions;
  struct layout lay;
  int rmap[256];
  int valid[256];

  N2 = N*N;
  N4 = N2*N2;
  state = new_array(int, N4);

  layout_NxN(N, &lay);

  for (i=0; i<256; i++) {
    rmap[i] = -1;
    valid[i] = 0;
  }
  for (i=0; i<lay.ns; i++) {
    rmap[lay.symbols[i]] = i;
    valid[lay.symbols[i]] = 1;
  }
  valid['.'] = 1;

  for (i=0; i<N4; i++) {
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

  n_solutions = solve(&lay, state, 0, options);

  if (n_solutions == 0) {
    fprintf(stderr, "The puzzle had no solutions\n");
    return;
  } else if (n_solutions == 1) {
    fprintf(stderr, "The puzzle had precisely 1 solution\n");

  } else {
    fprintf(stderr, "The puzzle had %d solutions (one is shown)\n", n_solutions);
  }

  print_NxN(stdout, N, state, lay.symbols);

  return;
}
/*}}}*/

static void pose_NxN(int N)/*{{{*/
{
  int *state, *copy, *answer;
  int *keep;
  int i;
  struct layout lay;
  int ok;
  int N2, N4;
  int tally;

  N2 = N*N;
  N4 = N2*N2;

  state = new_array(int, N4);
  copy = new_array(int, N4);
  answer = new_array(int, N4);
  keep = new_array(int, N4);

  layout_NxN(N, &lay);
  for (i=0; i<N4; i++) {
    state[i] = -1;
    keep[i] = 0;
  }

  if (1 != solve(&lay, state, 0, OPT_SPECULATE | OPT_FIRST_ONLY)) {
    fprintf(stderr, "??? FAILED TO GENERATE AN INITIAL GRID!!\n");
    exit(1);
  }
  /* Must be at least 1 solution if the solver tries hard enough! */

  /* Now remove givens one at a time until we find a minimum number that leaves
   * a puzzle with a unique solution. */
  memcpy(answer, state, N4*sizeof(int));

  tally = N4;
  do {
    int start_point;
    start_point = lrand48() % N4;
    ok = -1;
    for (i=0; i<N4; i++) {
      int ii;
      int n_sol;
      ii = (i + start_point) % N4;
      if (state[ii] < 0) continue;
      if (keep[ii] == 1) continue;
      memcpy(copy, state, N4*sizeof(int));
      copy[ii] = -1;
      n_sol = solve(&lay, copy, 0, OPT_STOP_ON_2);
      tally--;
      if (n_sol == 1) {
        ok = ii;
        break;
      } else {
        /* If it's no good removing this given now, it won't be any better to try
         * removing it again later... */
        fprintf(stderr, "%4d :  (can't remove given from <%s>)\n", tally, lay.cells[ii].name);
        keep[ii] = 1;
      }
    }

    if (ok >= 0) {
      fprintf(stderr, "%4d : Removing given from <%s>\n", tally, lay.cells[ok].name);
      state[ok] = -1;
    }
  } while (ok >= 0);

  print_NxN(stdout, N, state, lay.symbols);

  return;
}
/*}}}*/

int main (int argc, char **argv)/*{{{*/
{
  int options;
  int do_pose = 0;
  int do_solve = 1;
  int seed;
  int N = 3;

  options = 0;
  while (++argv, --argc) {
    if (!strcmp(*argv, "-v")) {
      options |= OPT_VERBOSE;
    } else if (!strcmp(*argv, "-f")) {
      options |= OPT_FIRST_ONLY;
    } else if (!strcmp(*argv, "-p")) {
      do_pose = 1;
      do_solve = 0;
    } else if (!strcmp(*argv, "-s")) {
      options |= OPT_SPECULATE;
    } else if (!strcmp(*argv, "-4")) {
      N = 4;
    } else if (!strcmp(*argv, "-5")) {
      N = 5;
    }
  }

  seed = time(NULL) ^ getpid();
  fprintf(stderr, "Seed=%d\n", seed);
  srand48(seed);
  if (do_solve) {
    solve_NxN(N, options);
  }
  if (do_pose) {
    pose_NxN(N);
  }
  return 0;
}
/*}}}*/
/* ============================================================================ */
