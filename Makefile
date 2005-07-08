CC := gcc
CFLAGS := -g

OBJ := sku.o solve.o blank.o display.o util.o infer.o layout.o superlayout.o reduce.o svg.o reader.o

sku : $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^

%.o : %.c
	$(CC) $(CFLAGS) -c $< -o $@


