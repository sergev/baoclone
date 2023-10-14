CC		= gcc -m64

VERSION         = 1.6
GITCOUNT        = $(shell git rev-list HEAD --count)
CFLAGS		= -g -O -Wall -Werror -DVERSION='"$(VERSION).$(GITCOUNT)"'
LDFLAGS		=

OBJS		= main.o util.o radio.o uv-5r.o uv-b5.o bf-888s.o bf-t1.o
SRCS		= main.c util.c radio.c uv-5r.c uv-b5.c bf-888s.c bf-t1.c
LIBS            =

# Mac OS X
#CFLAGS          += -I/usr/local/opt/gettext/include
#LIBS            += -L/usr/local/opt/gettext/lib -lintl

all:		baoclone

baoclone:	$(OBJS)
		$(CC) $(LDFLAGS) -o $@ $(OBJS) $(LIBS)

clean:
		rm -f *~ *.o core baoclone

install:	baoclone
		install -c -s baoclone /usr/local/bin/baoclone

baoclone.linux: baoclone
		cp -p $< $@
		strip $@

###
bf-888s.o: bf-888s.c radio.h util.h
bf-t1.o: bf-t1.c radio.h util.h
main.o: main.c radio.h util.h
radio.o: radio.c radio.h util.h
util.o: util.c util.h
uv-5r.o: uv-5r.c radio.h util.h
uv-b5.o: uv-b5.c radio.h util.h
