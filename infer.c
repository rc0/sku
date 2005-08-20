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

struct ws {/*{{{*/
  int nc, ng, ns;

  int *poss;
  int *todo;
  int solvepos;
  int spec_depth;
  int n_todo;

  /* Pointers/values passed in at the outer level. */
  int options;
  int *order;
  
  int *state;

  /* Queues for what to scan next. */
  struct queue *scan_q;
};
/*}}}*/
static struct ws *make_ws(int nc, int ng, int ns)/*{{{*/
{
  struct ws *ws = new(struct ws);
  int fill;
  int i;

  ws->nc = nc;
  ws->ng = ng;
  ws->ns = ns;
  ws->spec_depth = 0;

  fill = (1<<ns) - 1;
  ws->todo = new_array(int, ng);
  ws->poss = new_array(int, nc);
  ws->n_todo = 0;
  for (i=0; i<ng; i++) ws->todo[i] = fill;
  for (i=0; i<nc; i++) ws->poss[i] = fill;

  ws->scan_q = mk_queue(ng);
  
  return ws;
}
/*}}}*/
static int *copy_array(int n, int *data)/*{{{*/
{
  int *result;
  result = new_array(int, n);
  memcpy(result, data, n * sizeof(int));
  return result;
}
/*}}}*/
static struct ws *clone_ws(const struct ws *src)/*{{{*/
{
  struct ws *ws;
  ws = new(struct ws);
  ws->nc = src->nc;
  ws->ng = src->ng;
  ws->ns = src->ns;
  ws->spec_depth = 1 + src->spec_depth;
  ws->n_todo = src->n_todo;
  ws->solvepos = src->solvepos;
  ws->poss = copy_array(src->nc, src->poss); 
  ws->todo = copy_array(src->ng, src->todo);
  ws->options = src->options;
  ws->order = src->order;
  /* Not sure about this. */
  ws->state = NULL;

  /* When we need to clone, we know the queues must be empty! */
  ws->scan_q = mk_queue(src->ng);
  return ws;
}
/*}}}*/
static void free_ws(struct ws *ws)/*{{{*/
{
  free(ws->poss);
  free(ws->todo);
  rm_queue(ws->scan_q);
  free(ws);
}
/*}}}*/

/* ============================================================================ */

static int inner_infer(struct layout *lay, struct ws *ws);

/* ============================================================================ */

/*{{{ requeue_groups() */
static void
requeue_groups(struct layout *lay,
               struct ws *ws,
               int ic)
{
  int i;
  struct cell *cell = lay->cells + ic;
  for (i=0; i<NDIM; i++) {
    int gi = cell->group[i];
    if (gi >= 0) enqueue(ws->scan_q, gi);
    else break;
  }
}
  /*}}}*/

/*{{{ allocate() */
static void
allocate(struct layout *lay, struct ws *ws, int is_init, int ic, int val)
{
  int mask;
  int j, k;
  int NS;
  short *base;
  int other_poss;

  mask = 1<<val;
  NS = lay->ns;

  ws->state[ic] = val;
  other_poss = ws->poss[ic] & ~mask;
  ws->poss[ic] = 0;
  if (!is_init && ws->order) {
    ws->order[ic] = (ws->solvepos)++;
  }

  for (k=0; k<NDIM; k++) {
    int gg = lay->cells[ic].group[k];
    if (gg >= 0) {
      ws->todo[gg] &= ~mask;
      enqueue(ws->scan_q, gg);
      base = lay->groups + gg*NS;
      for (j=0; j<NS; j++) {
        int jc;
        jc = base[j];
        if (ws->poss[jc] & mask) {
          ws->poss[jc] &= ~mask;
          requeue_groups(lay, ws, jc);
          if (lay->cells[ic].is_terminal) {
            lay->cells[ic].is_terminal = 0;
          }
        }
        if (ws->poss[jc] & other_poss) {
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
try_group_allocate(int gi, struct layout *lay, struct ws *ws)
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
    if (ws->todo[gi] & mask) {
      int j, count, xic;
      xic = -1;
      count = 0;
      for (j=0; j<NS; j++) {
        int ic = base[j];
        if (ws->poss[ic] & mask) {
          xic = ic;
          count++;
          if (count > 1) break;
        }
      }
      if (count == 0) {
        if (ws->options & OPT_VERBOSE) {
          fprintf(stderr, "Cannot allocate <%c> in <%s>\n",
              lay->symbols[sym], lay->group_names[gi]);
        }
        return 0;
      } else if (count == 1) {
        if (ws->options & OPT_VERBOSE) {
          fprintf(stderr, "Allocate <%c> to <%s> (allocate in <%s>)\n",
              lay->symbols[sym], lay->cells[xic].name, lay->group_names[gi]);
        }
        --ws->n_todo;
        allocate(lay, ws, 0, xic, sym);
        found_any = 1;
        if (ws->options & OPT_HINT) {
          free_ws(ws);
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
    struct ws *ws)
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
    if (ws->todo[gi] & mask) {
      int j;
      int found_something = 0;
      memset(flags, 0, NC);
      memset(counts, 0, NG*sizeof(int));
      n_poss_cells = 0;
      for (j=0; j<NS; j++) {
        int ic = base[j];
        if (ws->poss[ic] & mask) {
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
              if (ws->poss[ic] & mask) {
                if (ws->options & OPT_VERBOSE) {
                  fprintf(stderr, "Removing <%c> from <%s> (in <%s> due to placement in <%s>)\n",
                      lay->symbols[sym], lay->cells[ic].name,
                      lay->group_names[j], lay->group_names[gi]);
                }
                ws->poss[ic] &= ~mask;
                requeue_groups(lay, ws, ic);
                found_something = 1;
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
/*{{{ try_near_stragglers() */
static int
try_near_stragglers(int gi,
    struct layout *lay,
    struct ws *ws)
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
      if (ws->poss[ci] & mask) {
        intersect[sym] &= ws->poss[ci];
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
          if (ws->poss[ci] != intersect[sym]) {
            if (ws->options & OPT_VERBOSE) {
              fprintf(stderr, "Removing <");
              show_symbols_in_set(NS, lay->symbols, ws->poss[ci] & ~intersect[sym]);
              fprintf(stderr, "> from <%s>, must be one of <", lay->cells[ci].name);
              show_symbols_in_set(NS, lay->symbols, intersect[sym]);
              fprintf(stderr, ">\n");
            }
            ws->poss[ci] = intersect[sym];
            requeue_groups(lay, ws, ci);
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
    struct ws *ws)
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
      if (!ws->poss[ci]) continue;
      count = 1; /* including the 'i' cell!! */
      for (j=i+1; j<NS; j++) {
        cj = base[j];
        if (ws->poss[ci] == ws->poss[cj]) {
          ++count;
        }
      }
      /* count==1 is a normal allocate done elsewhere! */
      if ((count > 1) && (count == count_bits(ws->poss[ci]))) {
        /* got one. */
        for (j=0; j<NS; j++) {
          cj = base[j];
          if ((ws->poss[cj] != ws->poss[ci]) && (ws->poss[cj] & ws->poss[ci])) {
            did_anything = did_anything_this_iter = 1;
            if (ws->options & OPT_VERBOSE) {
              int k, fk;
              fprintf(stderr, "Removing <");
              show_symbols_in_set(NS, lay->symbols, ws->poss[cj] & ws->poss[ci]);
              fprintf(stderr, "> from <%s:", lay->cells[cj].name);
              show_symbols_in_set(NS, lay->symbols, ws->poss[cj]);
              fprintf(stderr, ">, because <");
              show_symbols_in_set(NS, lay->symbols, ws->poss[ci]);
              fprintf(stderr, "> must be in <");
              fk = 1;
              for (k=0; k<NS; k++) {
                int ck = base[k];
                if (ws->poss[ck] == ws->poss[ci]) {
                  if (!fk) {
                    fprintf(stderr, ",");
                  }
                  fk = 0;
                  fprintf(stderr, "%s", lay->cells[ck].name);
                }
              }
              fprintf(stderr, ">\n");
            }
            ws->poss[cj] &= ~ws->poss[ci];
            requeue_groups(lay, ws, cj);
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
do_group(int gi, struct layout *lay, struct ws *ws)
{
  /* Return 0 if something went wrong, 1 if it was OK.  */

  int status;

  /* Don't continue if it's a row or column and we don't want that */
  if ((ws->options & OPT_NO_LINES) && (lay->is_block[gi] == 0)) return 1;

  /* Group will get enqueued when the final symbol is allocated; we can exit
   * right away when it next gets scanned. */
  if (ws->todo[gi] == 0) return 1;

  status = try_group_allocate(gi, lay, ws);
  if (!status) return status;

  if ((status == 1) && !(ws->options & OPT_NO_SUBSETS)) {
    status = try_subsets(gi, lay, ws);
  }

  if ((status == 1) && !(ws->options & OPT_NO_REMOTE)) {
    status = try_remote_stragglers(gi, lay, ws);
  }
 
  if ((status == 1) && !(ws->options & OPT_NO_NEAR)) {
    status = try_near_stragglers(gi, lay, ws);
  }
  
  /* Add new infererence techniques here, if status==1 */
  return 1; /* Success */
}
/*}}}*/
/*{{{ do_uniques() */
static int
do_uniques(struct layout *lay, struct ws *ws)
{
  int ic;
  int NC;
  int nb;
  NC = lay->nc;
  for (ic=0; ic<NC; ic++) {
    if (ws->state[ic] < 0) {
      nb = count_bits(ws->poss[ic]);
      if (nb == 0) {
        if (ws->options & OPT_VERBOSE) {
          fprintf(stderr, "Cell <%s> has no options left\n", lay->cells[ic].name);
        }
        return 0;
      } else if (nb == 1) {
        int sym = decode(ws->poss[ic]);
        if (ws->options & OPT_VERBOSE) {
          fprintf(stderr, "Allocate <%c> to <%s> (only option)\n",
              lay->symbols[sym], lay->cells[ic].name);
        }
        --ws->n_todo;
        allocate(lay, ws, 0, ic, sym);
        if (ws->options & OPT_HINT) {
          free_ws(ws);
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
speculate(struct layout *lay, struct ws *ws_in)
{
  /* Called when all else fails and we have to guess a cell but be able to back
   * out the guess if it goes wrong. */
  int ic;
  int start_point;
  int i;
  int NS, NC;
  int *scratch, *solution;
  int n_sol, total_n_sol;

  ic = select_minimal_cell(lay, ws_in->state, ws_in->poss, 1);
  if (ic < 0) {
    ic = select_minimal_cell(lay, ws_in->state, ws_in->poss, 0);
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
    if (mask & ws_in->poss[ic]) {
      struct ws *ws;
      ws = clone_ws(ws_in);
      memcpy(scratch, ws_in->state, NC * sizeof(int));
      ws->state = scratch;
      --ws->n_todo;
      allocate(lay, ws, 0, ic, ii);
      n_sol = inner_infer(lay, ws);
      if (n_sol > 0) {
        memcpy(solution, scratch, NC * sizeof(int));
        total_n_sol += n_sol;
        if (ws_in->options & OPT_FIRST_ONLY) break;
      } else {
      }
      if ((ws_in->options & OPT_STOP_ON_2) && (total_n_sol >= 2)) break;
    }
  }
  free(scratch);
  memcpy(ws_in->state, solution, NC * sizeof(int));
  free(solution);
  return total_n_sol;
}
/*}}}*/
static int inner_infer(struct layout *lay, struct ws *ws)/*{{{*/
{
  int NC, NG, NS;
  int gi;
  int result;

  NC = lay->nc;
  NG = lay->ng;
  NS = lay->ns;

  while (1) {
    while ((gi = dequeue(ws->scan_q)) >= 0) {
      if (!do_group(gi, lay, ws)) {
        result = 0;
        goto get_out;
      }
    }
    if (!(ws->options & OPT_NO_ONLYOPT)) {
      if (!do_uniques(lay, ws)) {
        result = 0;
        goto get_out;
      }
    }

    if (empty_p(ws->scan_q)) break;
  }

  if (ws->n_todo == 0) {
    if ((ws->options & (OPT_SPECULATE | OPT_SHOW_ALL)) == (OPT_SPECULATE | OPT_SHOW_ALL)) {
      /* ugh, ought to be via an argument */
      static int sol_no = 1;
      printf("Solution %d:\n", sol_no++);
      display(stdout, lay, ws->state);
      printf("\n");
    }
    result = 1;
  } else if (ws->n_todo > 0) {
    /* Didn't get a solution - decide whether to speculate or not. */
    if (ws->options & OPT_SPECULATE) {
      result = speculate(lay, ws);
    } else {
      result = 0;
    }
  } else {
    fprintf(stderr, "n_todo became negative\n");
    exit(1);
  }

get_out:
  return result;
  
}
/*}}}*/
int infer(struct layout *lay, int *state, int *order, int options)/*{{{*/
{
  int nc, ng, ns;
  struct ws *ws;
  int i;
  int result;

  nc = lay->nc;
  ng = lay->ng;
  ns = lay->ns;

  ws = make_ws(nc, ng, ns);
  ws->solvepos = 0;
  ws->options = options;
  ws->state = state;
  ws->order = order;

  for (i=0; i<nc; i++) {
    if (state[i] >= 0) {
      /* This will clear the poss bits on cells in the same groups, remove todo
       * bits, and enqueue groups that need scanning immediately at the start
       * of the search. */
      allocate(lay, ws, 1, i, state[i]);
    } else {
      ++ws->n_todo;
    }
  }

  result = inner_infer(lay, ws);

  free_ws(ws);
  return result;
  
}
/*}}}*/
/* ============================================================================ */
