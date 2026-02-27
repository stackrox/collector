DESTDIR?=/usr/local
CFLAGS+=$(shell pkg-config --cflags rpm)
LDLIBS+=$(shell pkg-config --libs rpm)

all: orphaner

install: orphaner
	cp orphaner $(DESTDIR)/bin

orphaner:orphaner.o avl.o avl-cmp.o
clean:
	rm -f orphaner *.o

.PHONY:all clean install
