#include "sku.h"

void superlayout_5(struct super_layout *superlay)/*{{{*/
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
void superlayout_8(struct super_layout *superlay)/*{{{*/
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
void superlayout_9(struct super_layout *superlay)/*{{{*/
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
void superlayout_11(struct super_layout *superlay)/*{{{*/
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
void layout_N_superlay(int N, const struct super_layout *superlay, struct layout *lay)/*{{{*/
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

