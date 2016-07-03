##
## lzdgen - LZ data generator example
##
## GCC Makefile
##

.SUFFIXES:

.PHONY: clean all

CFLAGS = -Wall -Wextra -march=native -Ofast -flto
CPPFLAGS = -DNDEBUG

ifeq ($(OS),Windows_NT)
  LDFLAGS += -static -s
  ifeq ($(CC),cc)
    CC = gcc
  endif
endif

objs = lzdgen.o lzdatagen.o parg.o pcg_basic.o

target = lzdgen

all: $(target)

%.o : %.c
	$(CC) $(CFLAGS) $(CPPFLAGS) -c -o $@ $<

$(target): $(objs)
	$(CC) $(CFLAGS) $(CPPFLAGS) $(LDFLAGS) $^ $(LDLIBS) -o $@

clean:
	$(RM) $(objs) $(target)

lzdgen.o: lzdatagen.h parg.h pcg_basic.h
lzdatagen.o: lzdatagen.h
parg.o: parg.h
pcg_basic.o: pcg_basic.h
