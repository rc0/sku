
#include "sku.h"


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
void format_output(int options)/*{{{*/
{
  int *state;
  double scale, offset;
  int i;
  struct layout *lay;
  char grey_sym = 'A';

  read_grid(&lay, &state, options);

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

  for (i=0; i<lay->nc; i++) {
    if (state[i] == -2) {
      /* greyed out cell */
      double x, y;
      double wh;
      x = offset + scale * ((double) lay->cells[i].rcol);
      y = offset + scale * ((double) lay->cells[i].rrow);
      wh = scale * 1.0;
      printf("<rect style=\"fill:#d8d8d8;fill-opacity:1.0;stroke:none;\"\n");
      printf("  x=\"%f\" y=\"%f\" width=\"%f\" height=\"%f\" />\n",
          x, y, wh, wh);
      x = offset + scale * ((double) lay->cells[i].rcol + 0.8);
      y = offset + scale * ((double) lay->cells[i].rrow + 0.3);
      printf("<text style=\"font-size:9;font-style:normal;font-variant:normal;font-weight:bold;fill:#000;fill-opacity:1.0;stroke:none;font-family:Luxi Sans;text-anchor:middle;writing-mode:lr-tb\"\n");
      printf("x=\"%f\" y=\"%f\">%c</text>\n", x, y, grey_sym++);
    }
  }

  format_emit_lines(lay->n_thinlines, lay->thinlines, 0.5);
  format_emit_lines(lay->n_mediumlines, lay->mediumlines, 1.5);
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
