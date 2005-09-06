#  sku - analysis tool for Sudoku puzzles
#  Copyright (C) 2005  Richard P. Curnow
#
#  This program is free software; you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation; either version 2 of the License, or
#  (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software
#  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA


CC := gcc
#CFLAGS := -O2 -Wall -pg -fprofile-arcs -fno-inline
CFLAGS := -O2 -Wall
#CFLAGS := -g -Wall

PROG := sku
OBJ := sku.o \
	solve.o blank.o display.o util.o \
	infer.o \
	genlayout.o layout_mxn.o superlayout.o \
	reduce.o \
	svg.o \
	reader.o \
	mark.o \
	grade.o \
	tidy.o

$(PROG) : $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^

%.o : %.c sku.h
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	-rm -f *.o *.gcda *.gcno $(PROG)

.PHONY: clean


