#
# make
# make all      -- build everything
#
# make install  -- install baoclone binary to /usr/local
#
# make test     -- run unit tests
#
# make clean    -- remove build files
#

all:    build
	$(MAKE) -C build $@

test:   build
	$(MAKE) -C build $@

install: build
	$(MAKE) -C build $@

clean:
	rm -rf build

build:
	mkdir $@
	cmake -B $@ .

.PHONY: all clean test install
