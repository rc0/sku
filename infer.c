#include "sku.h"

/* ============================================================================ */

struct queue/*{{{*/
{
  int ri;       /* read cursor */
  int wi;       /* write cursor */
  int n1;       /* n1=n+1; sizeof(slots)=n1, sizeof(flags)=n */
  int *slots;   /* ring buffer containing queue */
  int *flags;   /* flags indicating which groups are in the queue (avoid multiple entries) */
};
/*}}}*/
static inline int empty_p(struct queue *q)/*{{{*/
{
  if (q->ri == q->wi)
    return 1;
  else
    return 0;
}
/*}}}*/
static struct queue *mk_queue(int n)/*{{{*/
{
  struct queue *result;
  result = new(struct queue);
  result->slots = new_array(int, 1+n);
  result->flags = new_array(int, n);
  result->n1 = n+1;
  result->ri = result->wi = 0;
  memset(result->flags, 0, n * sizeof(int));
  return result;
}
/*}}}*/
static void rm_queue(struct queue *q)/*{{{*/
{
  free(q->slots);
  free(q->flags);
  free(q);
}
/*}}}*/
static void enqueue(struct queue *q, int x)/*{{{*/
{
  if (q->flags[x]) return; /* Already in queue */
#if 0
  fprintf(stderr, "    ENQ %d wi=%d\n", x, q->wi);
#endif
  q->slots[q->wi] = x;
  q->flags[x] = 1;
  ++q->wi;
  if (q->wi == q->n1) q->wi = 0;
  if (empty_p(q)) {
    fprintf(stderr, "Queue has overflowed\n");
    exit(1);
  }
}
/*}}}*/
static int dequeue(struct queue *q)/*{{{*/
{
  if (empty_p(q)) {
    return -1;
  } else {
    int result = q->slots[q->ri];
    ++q->ri;
    if (q->ri == q->n1) q->ri = 0;
    if (q->flags[result] == 0) {
      fprintf(stderr, "Flag table out of sync\n");
      exit(1);
    }
    q->flags[result] = 0;
    return result;
  }
}
/*}}}*/

/* ============================================================================ */

/* Rewrite */
/*{{{ requeue_groups() */
static void
requeue_groups(struct layout *lay,
               struct queue *scan_q,
               int ic)
{
  int i;
  struct cell *cell = lay->cells + ic;
  for (i=0; i<NDIM; i++) {
    int gi = cell->group[i];
    if (gi >= 0) enqueue(scan_q, gi);
    else break;
  }
}
  /*}}}*/


/*{{{ allocate() */
static void
allocate(struct layout *lay,
         int *state,
         int *poss,
         int *todo,
         struct queue *scan_q,
         int *order,
         int *solvepos,
         int ic,
         int val)
{
  int mask;
  int j, k;
  int NS;
  short *base;

  state[ic] = val;
  poss[ic] = 0;
  if (order) {
    order[ic] = (*solvepos)++;
  }

  mask = 1<<val;
  NS = lay->ns;

  for (k=0; k<NDIM; k++) {
    int gg = lay->cells[ic].group[k];
    if (gg >= 0) {
      todo[gg] &= ~mask;
#if 0
      fprintf(stderr, "  1. enqueue %s\n", lay->group_names[gg]);
#endif
      enqueue(scan_q, gg);
      base = lay->groups + gg*NS;
      for (j=0; j<NS; j++) {
        int jc;
        jc = base[j];
        if (poss[jc] & mask) {
          poss[jc] &= ~mask;
          requeue_groups(lay, scan_q, jc);
        }
      }
    } else {
      break;
    }
  }
}
/*}}}*/
/*{{{ try_group_allocate() */
static int
try_group_allocate(int gi,
    struct layout *lay,
    int *state,
    int *poss,
    int *todo,
    struct queue *scan_q,
    int *order,
    int *solvepos,
    int *n_todo,
    int options)
{
  /* Return 0 if the solution is broken,
   *        1 if we didn't allocate anything,
   *        2 if we did. */
  int NS;
  short *base;
  int sym, mask;
  int found_any = 0;

  NS = lay->ns;
  base = lay->groups + gi*NS;
  for (sym=0; sym<NS; sym++) {
    mask = 1<<sym;
    if (todo[gi] & mask) {
      int j, count, xic;
      xic = -1;
      count = 0;
      for (j=0; j<NS; j++) {
        int ic = base[j];
        if (poss[ic] & mask) {
          xic = ic;
          count++;
          if (count > 1) break;
        }
      }
      if (count == 0) {
        if (options & OPT_VERBOSE) {
          fprintf(stderr, "Cannot allocate <%c> in <%s>\n",
              lay->symbols[sym], lay->group_names[gi]);
        }
        return 0;
      } else if (count == 1) {
        if (options & OPT_VERBOSE) {
          fprintf(stderr, "Allocate <%c> to <%s> (allocate in <%s>)\n",
              lay->symbols[sym], lay->cells[xic].name, lay->group_names[gi]);
        }
        --*n_todo;
        allocate(lay, state, poss, todo, scan_q, order, solvepos, xic, sym);
        found_any = 1;
      }
    }
  }

  return found_any ? 2 : 1;

}
/*}}}*/
/*{{{ try_subsetting() */
static int
try_subsetting(int gi,
    struct layout *lay,
    int *state,
    int *poss,
    int *todo,
    struct queue *scan_q,
    int *order,
    int *solvepos,
    int *n_todo,
    int options)
{
  /* Couldn't do any allocates in the group.
   * So try the more sophisticated analysis:
   * Analyse the group to find out which subset of cells can contain
   * each unallocated symbol.  If this subset is also a subset of some other
   * group, we can eliminate the symbol as a possibility from the rest of
   * that other group.
   */
  int NC, NS, NG;
  char *flags;
  int *counts;
  int sym;
  int n_poss_cells;
  short *base;
  int did_anything = 0;

  NS = lay->ns;
  NC = lay->nc;
  NG = lay->ng;
  flags = new_array(char, NC);
  counts = new_array(int, NG);

  base = lay->groups + gi*NS;
  for (sym=0; sym<NS; sym++) {
    int mask = (1 << sym);
    if (todo[gi] & mask) {
      int j;
      memset(flags, 0, NC);
      memset(counts, 0, NG*sizeof(int));
      n_poss_cells = 0;
      for (j=0; j<NS; j++) {
        int ic = base[j];
        if (poss[ic] & mask) {
          int m;
          ++n_poss_cells;
          flags[ic] = 1;
          for (m=0; m<NDIM; m++) {
            int gm = lay->cells[ic].group[m];
            if (gm >= 0) {
              if (gm != gi) ++counts[gm];
            } else {
              break;
            }
          }
        }
      }
      for (j=0; j<NG; j++) {
        if (counts[j] == n_poss_cells) {
          int m;
          short *base = lay->groups + j*NS;
          for (m=0; m<NS; m++) {
            int ic = base[m];
            if (!flags[ic]) { /* cell not in the original group. */
              if (poss[ic] & mask) {
                if (options & OPT_VERBOSE) {
                  fprintf(stderr, "Removing <%c> from <%s> (in <%s> due to placement in <%s>)\n",
                      lay->symbols[sym], lay->cells[ic].name,
                      lay->group_names[j], lay->group_names[gi]);
                }
                poss[ic] &= ~mask;
                requeue_groups(lay, scan_q, ic);
                did_anything = 1;
              }
            }
          }
        }
      }
    }
  }
  free(counts);
  free(flags);
  return did_anything ? 2 : 1;
}

/*}}}*/
/*{{{ try_clusters() */
static int
try_clusters(int gi,
    struct layout *lay,
    int *state,
    int *poss,
    int *todo,
    struct queue *scan_q,
    int *order,
    int *solvepos,
    int *n_todo,
    int options)
{
  /* 
   * Deal with this case: suppose the symbols 2,3,5,6 are unallocated within
   * a group.  Suppose there are two cells
   *   A that could be 2,3,5
   *   B that could be 2,3,6
   * and neither 2 nor 3 could go anywhere else within the group under
   * analysis.  Then clearly we can eliminate 5 as a possibility on A and 6
   * as a possibility on B.
   * */

  int NC, NS, NG;
  int did_anything = 0;
  int *intersect, *poss_map;
  int sym, cell, ci;
  int fill, mask;
  short *base;

  NS = lay->ns;
  NC = lay->nc;
  NG = lay->ng;
  intersect = new_array(int, NS);
  poss_map = new_array(int, NS);

  base = lay->groups + gi*NS;
  fill = (1 << NS) - 1;
  
  for (sym=0; sym<NS; sym++) {
    intersect[sym] = fill;
    poss_map[sym] = 0;
    mask = 1<<sym;
    for (cell=0; cell<NS; cell++) {
      ci = base[cell];
      if (poss[ci] & mask) {
        intersect[sym] &= poss[ci];
        poss_map[sym] |= (1<<cell);
      }
    }
  }

  /* Now analyse to look for candidates. */
  for (sym=0; sym<NS; sym++) {
    if (count_bits(intersect[sym]) == count_bits(poss_map[sym])) {
      /* that is a necessary condition... */
      int sym1;
      for (sym1=0; sym1<NS; sym1++) {
        int mask = 1<<sym1;
        if (sym1 == sym) continue;
        if (intersect[sym] & mask) {
          if ((intersect[sym] == intersect[sym1]) &&
              (poss_map[sym] == poss_map[sym1])) {

          } else {
            goto examine_next_symbol;
          }
        }
      }

      /* Good subset: */
      for (cell=0; cell<NS; cell++) {
        if (poss_map[sym] & (1<<cell)) {
          int ci = base[cell];
          if (poss[ci] != intersect[sym]) {
            if (options & OPT_VERBOSE) {
              fprintf(stderr, "Removing <");
              show_symbols_in_set(NS, lay->symbols, poss[ci] & ~intersect[sym]);
              fprintf(stderr, "> from <%s>, must be one of <", lay->cells[ci].name);
              show_symbols_in_set(NS, lay->symbols, intersect[sym]);
              fprintf(stderr, ">\n");
            }
            poss[ci] = intersect[sym];
            requeue_groups(lay, scan_q, ci);
          }
        }
      }
    }
examine_next_symbol:
    (void) 0;
  }

  free(intersect);
  free(poss_map);
  return did_anything ? 2 : 1;
}

/*}}}*/
/*{{{ try_ucd() */
static int
try_ucd(int gi,
    struct layout *lay,
    int *state,
    int *poss,
    int *todo,
    struct queue *scan_q,
    int *order,
    int *solvepos,
    int *n_todo,
    int options)
{
  /* 
   * Deal with this case: suppose the symbols 2,3,5 are unallocated within
   * a group.  Suppose there are three cells
   *   A that could be 2,3
   *   B that could be 2,3
   *   C that could be 3,5
   * and neither 2 nor 3 could go anywhere else within the group under
   * analysis.  Then we can eliminate 3 as an option on C.
   * */

  int NC, NS, NG;
  int did_anything = 0;
  int did_anything_this_iter;
  int i, ci, j, cj;
  short *base;

  NS = lay->ns;
  NC = lay->nc;
  NG = lay->ng;

  base = lay->groups + gi*NS;

  do {
    did_anything_this_iter = 0;
    for (i=0; i<NS-1; i++) {
      int count;
      ci = base[i];
      if (!poss[ci]) continue;
      count = 1; /* including the 'i' cell!! */
      for (j=i+1; j<NS; j++) {
        cj = base[j];
        if (poss[ci] == poss[cj]) {
          ++count;
        }
      }
      /* count==1 is a normal allocate done elsewhere! */
      if ((count > 1) && (count == count_bits(poss[ci]))) {
        /* got one. */
        for (j=0; j<NS; j++) {
          cj = base[j];
          if ((poss[cj] != poss[ci]) && (poss[cj] & poss[ci])) {
            did_anything = did_anything_this_iter = 1;
            if (options & OPT_VERBOSE) {
              int k, fk;
              fprintf(stderr, "Removing <");
              show_symbols_in_set(NS, lay->symbols, poss[cj] & poss[ci]);
              fprintf(stderr, "> from <%s:", lay->cells[cj].name);
              show_symbols_in_set(NS, lay->symbols, poss[cj]);
              fprintf(stderr, ">, because <");
              show_symbols_in_set(NS, lay->symbols, poss[ci]);
              fprintf(stderr, "> must be in <");
              fk = 1;
              for (k=0; k<NS; k++) {
                int ck = base[k];
                if (poss[ck] == poss[ci]) {
                  if (!fk) {
                    fprintf(stderr, ",");
                  }
                  fk = 0;
                  fprintf(stderr, "%s", lay->cells[ck].name);
                }
              }
              fprintf(stderr, ">\n");
            }
            poss[cj] &= ~poss[ci];
            requeue_groups(lay, scan_q, cj);
          }
        }
      }
    }
  } while (did_anything_this_iter);

  return did_anything ? 2 : 1;
}

/*}}}*/
/*{{{ do_group() */
static int
do_group(int gi,
         struct layout *lay,
         int *state,
         int *poss,
         int *todo,
         struct queue *scan_q,
         int *order,
         int *solvepos,
         int *n_todo,
         int options)
{
  /* Return 0 if something went wrong, 1 if it was OK.  */

  int status;

  /* Don't continue if it's a row or column and we don't want that */
  if ((options & OPT_NO_ROWCOL_ALLOC) && (lay->is_block[gi] == 0)) return 1;

  /* Group will get enqueued when the final symbol is allocated; we can exit
   * right away when it next gets scanned. */
  if (todo[gi] == 0) return 1;

  status = try_group_allocate(gi, lay, state, poss, todo, scan_q, order, solvepos, n_todo, options);
  if (!status) return status;

  if ((status == 1) && !(options & OPT_NO_UCD)) {
    status = try_ucd(gi, lay, state, poss, todo, scan_q, order, solvepos, n_todo, options);
  }
 
  if ((status == 1) && !(options & OPT_NO_CLUSTERING)) {
    status = try_clusters(gi, lay, state, poss, todo, scan_q, order, solvepos, n_todo, options);
  }
  
  if ((status == 1) && !(options & OPT_NO_SUBSETTING)) {
    status = try_subsetting(gi, lay, state, poss, todo, scan_q, order, solvepos, n_todo, options);
  }
 
  /* Add new infererence techniques here, if status==1 */
  return 1; /* Success */
}
/*}}}*/
/*{{{ do_uniques() */
static int
do_uniques(struct layout *lay,
           int *state,
           int *poss,
           int *todo,
           struct queue *scan_q,
           int *order,
           int *solvepos,
           int *n_todo,
           int options)
{
  int ic;
  int NC;
  int nb;
  NC = lay->nc;
  for (ic=0; ic<NC; ic++) {
    if (state[ic] < 0) {
      nb = count_bits(poss[ic]);
      if (nb == 0) {
        fprintf(stderr, "Cell <%s> has no options left\n", lay->cells[ic].name);
        return 0;
      } else if (nb == 1) {
        int sym = decode(poss[ic]);
        if (options & OPT_VERBOSE) {
          fprintf(stderr, "Allocate <%c> to <%s> (only option)\n",
              lay->symbols[sym], lay->cells[ic].name);
        }
        --*n_todo;
        allocate(lay, state, poss, todo, scan_q, order, solvepos, ic, sym);
      }
    }
  }
  return 1;
}
/*}}}*/
/*{{{ select_minimal_cell() */
static int
select_minimal_cell(struct layout *lay,
                    int *state,
                    int *poss,
                    int in_overlap)
{
  int ic;
  int minbits;
  int i;
  minbits = lay->ns + 1;
  ic = -1;
  for (i=0; i<lay->nc; i++) {
    if (!in_overlap || lay->cells[i].is_overlap) {
      if (state[i] < 0) {
        int nb = count_bits(poss[i]);
        if (nb < minbits) {
          minbits = nb;
          ic = i;
        }
      }
    }
  }
  return ic;
}
/*}}}*/
/*{{{ speculate() */
static int
speculate(struct layout *lay,
          int *state,
          int *poss,
          int *order,
          int solvepos,
          int options)
{
  /* Called when all else fails and we have to guess a cell but be able to back
   * out the guess if it goes wrong. */
  int ic;
  int start_point;
  int i;
  int NS, NC;
  int *scratch, *solution;
  int n_sol, total_n_sol;

  ic = select_minimal_cell(lay, state, poss, 1);
  if (ic < 0) {
    ic = select_minimal_cell(lay, state, poss, 0);
  }
  if (ic < 0) {
    return 0;
  }

  NS = lay->ns;
  NC = lay->nc;
  scratch = new_array(int, NC);
  solution = new_array(int, NC);
  start_point = lrand48() % NS;
  total_n_sol = 0;
  for (i=0; i<NS; i++) {
    int ii = (i + start_point) % NS;
    int mask = 1<<ii;
    if (mask & poss[ic]) {
      memcpy(scratch, state, NC * sizeof(int));
      scratch[ic] = ii;
      n_sol = infer(lay, scratch, order, 0, solvepos, options);
      if (n_sol > 0) {
        memcpy(solution, scratch, NC * sizeof(int));
        total_n_sol += n_sol;
        if (options & OPT_FIRST_ONLY) break;
      }
      if ((options & OPT_STOP_ON_2) && (total_n_sol >= 2)) break;
    }
  }
  free(scratch);
  memcpy(state, solution, NC * sizeof(int));
  free(solution);
  return total_n_sol;
}
/*}}}*/
int infer(struct layout *lay, int *state, int *order, int iter, int solvepos, int options)/*{{{*/
{
  int *todo;
  int *poss;
  struct queue *scan_q;
  int NC, NG, NS;
  int FILL;
  int i;
  int n_todo; /* Number of cells still to solve for. */
  int gi;
  int result;

  NC = lay->nc;
  NG = lay->ng;
  NS = lay->ns;

  scan_q = mk_queue(NG);
  todo = new_array(int, NG);
  poss = new_array(int, NC);

  FILL = (1 << NS) - 1;
  n_todo = 0;
  for (i=0; i<NG; i++) {
    todo[i] = FILL;
  }
  for (i=0; i<NC; i++) {
    poss[i] = FILL;
  }
  for (i=0; i<NC; i++) {
    if (state[i] >= 0) {
      /* This will clear the poss bits on cells in the same groups, remove todo
       * bits, and enqueue groups that need scanning immediately at the start
       * of the search. */
      allocate(lay, state, poss, todo, scan_q, NULL, NULL, i, state[i]);
    } else {
      n_todo++;
    }
  }

  while (1) {
    while ((gi = dequeue(scan_q)) >= 0) {
      if (!do_group(gi, lay, state, poss, todo, scan_q, order, &solvepos, &n_todo, options)) {
        result = 0;
        goto get_out;
      }
    }
    if (!(options & OPT_NO_UNIQUES)) {
      if (!do_uniques(lay, state, poss, todo, scan_q, order, &solvepos, &n_todo, options)) {
        result = 0;
        goto get_out;
      }
    }

    if (empty_p(scan_q)) break;
  }

  if (n_todo == 0) {
    result = 1;
  } else if (n_todo > 0) {
    /* Didn't get a solution - decide whether to speculate or not. */
    if (options & OPT_SPECULATE) {
      result = speculate(lay, state, poss, order, solvepos, options);
    } else {
      result = 0;
    }
  } else {
    fprintf(stderr, "n_todo became negative\n");
    exit(1);
  }

get_out:
  rm_queue(scan_q);
  free(todo);
  free(poss);
  return result;
  
}
/*}}}*/
/* ============================================================================ */
#if 0
static inline void old_allocate(struct layout *lay, int *state, int *poss, int *todo, int *order, int ic, int val, int *solvepos)/*{{{*/
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
int old_infer(struct layout *lay, int *state, int *order, int iter, int solvepos, int options)/*{{{*/
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
#endif
