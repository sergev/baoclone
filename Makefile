CC		= gcc -m32

VERSION         = 1.4
CFLAGS		= -g -O -Wall -Werror -DVERSION='"$(VERSION)"'
LDFLAGS		=

OBJS		= main.o util.o radio.o uv-5r.o uv-b5.o bf-888s.o ft-60r.o
SRCS		= main.c util.c radio.c uv-5r.c uv-b5.c bf-888s.c ft-60r.c
LIBS            =

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
ft-60r.o: ft-60r.c radio.h util.h
main.o: main.c radio.h util.h
radio.o: radio.c radio.h util.h
util.o: util.c util.h
uv-5r.o: uv-5r.c radio.h util.h
uv-b5.o: uv-b5.c radio.h util.h
