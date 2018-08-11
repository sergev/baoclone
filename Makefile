CC		= gcc -m64

VERSION         = 1.5
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

install:	baoclone baoclone-ru.mo
		install -c -s baoclone /usr/local/bin/baoclone
		install -D baoclone-ru.mo /usr/local/share/locale/ru/LC_MESSAGES/baoclone.mo

baoclone.linux: baoclone
		cp -p $< $@
		strip $@

baoclone.po:    $(SRCS)
		xgettext --from-code=utf-8 --keyword=_ \
                    --package-name=BaoClone --package-version=$(VERSION) \
                    $(SRCS) -o $@

baoclone-ru.mo: baoclone-ru.po
		msgfmt -c -o $@ $<

baoclone-ru-cp866.mo: baoclone-ru.po
		iconv -f utf-8 -t cp866 $< | sed 's/UTF-8/CP866/' | msgfmt -c -o $@ -

###
bf-888s.o: bf-888s.c radio.h util.h
bf-t1.o: bf-t1.c radio.h util.h
main.o: main.c radio.h util.h
radio.o: radio.c radio.h util.h
util.o: util.c util.h
uv-5r.o: uv-5r.c radio.h util.h
uv-b5.o: uv-b5.c radio.h util.h
