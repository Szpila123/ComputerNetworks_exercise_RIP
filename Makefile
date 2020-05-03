#bartosz_szpila
#307554

CC=gcc
CFLAGS=-std=c99 -Wall -Wextra

SRC_C=$(wildcard bin/*.c)
SRC_H=$(wildcard include/*.h)

OBJS=$(SRC_C:%.c=%.o)

ARCH=bartosz_szpila

all: $(SRC_C) $(SRC_H) $(OBJS) program.out

program.out: $(OBJS)
	@echo "[CC] $@ <- $^"
	$(CC) $(CFLAGS) -o $@ $^

bin/%.o: bin/%.c include/%.h
	@echo "[CC] $@ <- $^"
	$(CC) $(CFLAGS)  -o $@ -c $< -I/usr/include


distclean:
	rm -vf $(OBJS)
	rm -vf program.out
	rm -vf  $(ARCH).tar.xz

clean:
	rm -vf $(OBJS)

archive:
	mkdir $(ARCH) $(ARCH)/bin $(ARCH)/include
	cp $(SRC_C) $(ARCH)/bin
	cp $(SRC_H) $(ARCH)/include
	cp Makefile $(ARCH)
	tar cvJhf $(ARCH).tar.xz $(ARCH)
	rm -r $(ARCH)

.PHONY: all clean distclean archive
