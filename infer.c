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
  int other_poss;

  mask = 1<<val;
  NS = lay->ns;

  state[ic] = val;
  other_poss = poss[ic] & ~mask;
  poss[ic] = 0;
  if (order) {
    order[ic] = (*solvepos)++;
  }

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
          if (lay->cells[ic].is_terminal) {
            lay->cells[ic].is_terminal = 0;
          }
        }
        if (poss[jc] & other_poss) {
          /* The discovery of state[ic] has contributed to eventually solving [jc],
           * so [ic] is now non-terminal. */
          if (lay->cells[ic].is_terminal) {
            lay->cells[ic].is_terminal = 0;
          }
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
        if (options & OPT_HINT) {
          rm_queue(scan_q);
          free(state);
          free(todo);
          free(poss);
          free_layout(lay);
          exit(0);
        }
      }
    }
  }

  return found_any ? 2 : 1;

}
/*}}}*/
/*{{{ try_subsets() */
static int
try_subsets(int gi,
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
      int found_something = 0;
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
                found_something = 1;
                did_anything = 1;
              }
            }
          }
        }
      }
      if (0 && found_something) {
        /* Mark the cells that caused the derivation as non-terminal */
        for (j=0; j<NC; j++) {
          if (flags[j]) {
            if (lay->cells[j].is_terminal) {
              lay->cells[j].is_terminal = 0;
              fprintf(stderr, "Clearing terminal status of <%s>\n", lay->cells[j].name);
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
/*{{{ try_near_stragglers() */
static int
try_near_stragglers(int gi,
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
/*{{{ try_remote_stragglers() */
static int
try_remote_stragglers(int gi,
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
  if ((options & OPT_NO_LINES) && (lay->is_block[gi] == 0)) return 1;

  /* Group will get enqueued when the final symbol is allocated; we can exit
   * right away when it next gets scanned. */
  if (todo[gi] == 0) return 1;

  status = try_group_allocate(gi, lay, state, poss, todo, scan_q, order, solvepos, n_todo, options);
  if (!status) return status;

  if ((status == 1) && !(options & OPT_NO_SUBSETS)) {
    status = try_subsets(gi, lay, state, poss, todo, scan_q, order, solvepos, n_todo, options);
  }

  if ((status == 1) && !(options & OPT_NO_REMOTE)) {
    status = try_remote_stragglers(gi, lay, state, poss, todo, scan_q, order, solvepos, n_todo, options);
  }
 
  if ((status == 1) && !(options & OPT_NO_NEAR)) {
    status = try_near_stragglers(gi, lay, state, poss, todo, scan_q, order, solvepos, n_todo, options);
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
        if (options & OPT_HINT) {
          rm_queue(scan_q);
          free(state);
          free(todo);
          free(poss);
          free_layout(lay);
          exit(0);
        }
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
    if (!(options & OPT_NO_ONLYOPT)) {
      if (!do_uniques(lay, state, poss, todo, scan_q, order, &solvepos, &n_todo, options)) {
        result = 0;
        goto get_out;
      }
    }

    if (empty_p(scan_q)) break;
  }

  if (n_todo == 0) {
    if ((options & (OPT_SPECULATE | OPT_SHOW_ALL)) == (OPT_SPECULATE | OPT_SHOW_ALL)) {
      /* ugh, ought to be via an argument */
      static int sol_no = 1;
      printf("Solution %d:\n", sol_no++);
      display(stdout, lay, state);
      printf("\n");
    }
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
