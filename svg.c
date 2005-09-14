/*
 *  sku - analysis tool for Sudoku puzzles
 *  Copyright (C) 2005  Richard P. Curnow
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA
 */

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
static void emit_clusters(struct layout *lay, struct clusters *clus)/*{{{*/
{



}
/*}}}*/
void format_output(int options)/*{{{*/
{
  int *state;
  struct clusters *clus;
  double scale, offset;
  int i;
  struct layout *lay;
  char grey_sym = 'A';

  read_grid(&lay, &state, &clus, options);

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
    int is_marked, is_barred;
    is_marked = (state[i] == CELL_MARKED);
    is_barred = (state[i] == CELL_BARRED);
    if (is_marked || is_barred) {
      /* greyed out cell */
      double x, y;
      double wh;
      const char *cell_colour = NULL;
      x = offset + scale * ((double) lay->cells[i].rcol);
      y = offset + scale * ((double) lay->cells[i].rrow);
      wh = scale * 1.0;
      if (is_marked) cell_colour = "#d8d8d8";
      else if (is_barred) cell_colour = "#ff0000";
      printf("<rect style=\"fill:%s;fill-opacity:1.0;stroke:none;\"\n", cell_colour);
      printf("  x=\"%f\" y=\"%f\" width=\"%f\" height=\"%f\" />\n",
          x, y, wh, wh);
      if (is_marked) {
        x = offset + scale * ((double) lay->cells[i].rcol + 0.8);
        y = offset + scale * ((double) lay->cells[i].rrow + 0.3);
        printf("<text style=\"font-size:9;font-style:normal;font-variant:normal;"
                 "font-weight:bold;fill:#000;fill-opacity:1.0;stroke:none;"
                 "font-family:Luxi Sans;text-anchor:middle;writing-mode:lr-tb\"\n");
        printf("x=\"%f\" y=\"%f\">%c</text>\n", x, y, grey_sym++);
      }
    }
  }

  if (lay->is_additive && clus) {
    emit_clusters(lay, clus);
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

  free(state);
  if (clus) free_clusters(clus);
  free_layout(lay);
}
/*}}}*/
