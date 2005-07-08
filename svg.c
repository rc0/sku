
#include "sku.h"

struct intpair {/*{{{*/
  int a;
  int b;
};
/*}}}*/
static int compare_intpair(const void *a, const void *b)/*{{{*/
{
  const struct intpair *aa = (const struct intpair *) a;
  const struct intpair *bb = (const struct intpair *) b;
  if (aa->b < bb->b) return 1;
  else if (aa->b >  bb->b) return -1;
  else return 0;
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
void format_output(struct layout *lay, int grey_cells, int options)/*{{{*/
{
  int *state, *copy;
  int *order;
  double scale, offset;
  int i, j, k, n;
  struct intpair *shade = NULL;
  int *shade_index;
  int *flags;

  state = new_array(int, lay->nc);
  read_grid(lay, state);

  if (grey_cells > 0) {
    int max_order;
    order = new_array(int, lay->nc);
    copy = new_array(int, lay->nc);
    memcpy(copy, state, lay->nc * sizeof(int));
    memset(order, 0, lay->nc * sizeof(int));
    infer(lay, copy, order, 0, 0, OPT_SPECULATE);
    shade = new_array(struct intpair, lay->nc);
    for (i=0; i<lay->nc; i++) {
      shade[i].a = i;
      shade[i].b = order[i];
    }
    qsort(shade, lay->nc, sizeof(struct intpair), compare_intpair);
    flags = new_array(int, lay->nc);
    memset(flags, 0, lay->nc * sizeof(int));
    shade_index = new_array(int, grey_cells);
    for (i=0, j=0; i<grey_cells; i++) {
      int ic;
      do {
        ic = shade[j++].a;
      } while (flags[ic]);
      shade_index[i] = ic;
      for (k=0; k<NDIM; k++) {
        int gx = lay->cells[ic].group[k];
        if (gx >= 0) {
          for (n=0; n<lay->ns; n++) {
            int oic = lay->groups[gx*lay->ns + n];
            flags[oic] = 1;
          }
        } else {
          break;
        }
      }
    }
  }
  
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

  if (grey_cells > 0) {
    for (i=0; i<grey_cells; i++) {
      double x, y;
      double wh;
      x = offset + scale * ((double) lay->cells[shade_index[i]].rcol);
      y = offset + scale * ((double) lay->cells[shade_index[i]].rrow);
      wh = scale * 1.0;
      printf("<rect style=\"fill:#d8d8d8;fill-opacity:1.0;stroke:none;\"\n");
      printf("  x=\"%f\" y=\"%f\" width=\"%f\" height=\"%f\" />\n",
          x, y, wh, wh);
      x = offset + scale * ((double) lay->cells[shade_index[i]].rcol + 0.8);
      y = offset + scale * ((double) lay->cells[shade_index[i]].rrow + 0.3);
      printf("<text style=\"font-size:9;font-style:normal;font-variant:normal;font-weight:bold;fill:#000;fill-opacity:1.0;stroke:none;font-family:Luxi Sans;text-anchor:middle;writing-mode:lr-tb\"\n");
      printf("x=\"%f\" y=\"%f\">%c</text>\n", x, y, 'A' + i);
    }
  }
  
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
