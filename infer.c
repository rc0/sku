#include "sku.h"

static inline void allocate(struct layout *lay, int *state, int *poss, int *todo, int *order, int ic, int val, int *solvepos)/*{{{*/
{
  int mask;
  int k;

  mask = 1<<val;
  state[ic] = val;
  poss[ic] = 0;
  if (order) {
    order[ic] = (*solvepos)++;
  }

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
/* ============================================================================ */
int infer(struct layout *lay, int *state, int *order, int iter, int solvepos, int options)/*{{{*/
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
              allocate(lay, state, poss, todo, order, free_ic, i, &solvepos);
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

            allocate(lay, state, poss, todo, order, i, sol, &solvepos);
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
         * ?*/
        int ic;
        int n_solutions, n_sol;
        n_solutions = 0;
        /* Try to find an overlap cell first, otherwise -a mode on a multi-grid
         * takes forever */
        for (ic=0; ic<lay->nc; ic++) {
          if (lay->cells[ic].is_overlap) {
            int nposs = count_bits(poss[ic]);
            if (nposs > 0) break;
          }
        }
        if (ic == lay->nc) {
          /* Failed to find an overlap cell, now just take any. */
          for (ic=0; ic<lay->nc; ic++) {
            int nposs = count_bits(poss[ic]);
            if (nposs > 0) break;
          }
        }
        if (ic < lay->nc) {
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
              n_sol = infer(lay, scratch, order, iter, solvepos, options);
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
        } else {
          return 0;
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
