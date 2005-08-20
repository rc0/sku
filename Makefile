CC := gcc
#CFLAGS := -O2 -Wall -pg -fprofile-arcs -fno-inline
#CFLAGS := -O2 -Wall
CFLAGS := -g -Wall

PROG := sku
OBJ := sku.o \
	solve.o blank.o display.o util.o \
	infer.o \
	genlayout.o layout_mxn.o superlayout.o \
	reduce.o \
	svg.o \
	reader.o \
	mark.o \
	grade.o

$(PROG) : $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^

%.o : %.c sku.h
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	-rm -f *.o *.gcda *.gcno $(PROG)

.PHONY: clean


