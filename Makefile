CC		= gcc -m32
#CC              = i586-mingw32msvc-gcc -m32

CFLAGS		= -g -O -Wall -Werror
LDFLAGS		=

OBJS		= main.o util.o radio.o uv-5r.o uv-b5.o bf-888s.o
LIBS            =

all:		baoclone

baoclone:	$(OBJS)
		$(CC) $(LDFLAGS) -o $@ $(OBJS) $(LIBS)

clean:
		rm -f *~ *.o core baoclone

install:	baoclone
		install -c -s baoclone /usr/local/bin/baoclone

###
main.o: main.c radio.h util.h
radio.o: radio.c radio.h util.h
util.o: util.c util.h
uv-5r.o: uv-5r.c radio.h util.h
