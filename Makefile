CC := gcc
CFLAGS := -O2 -Wall

PROG := sku
OBJ := sku.o \
	solve.o blank.o display.o util.o \
	infer.o \
	genlayout.o layout_mxn.o superlayout.o \
	reduce.o \
	svg.o \
	reader.o \
	mark.o

$(PROG) : $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^

%.o : %.c sku.h
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	-rm -f *.o $(PROG)

.PHONY: clean


