PREFIX = /usr
PROGNAME = camsearch

build: clean
	gcc -O2 -std=gnu11 -D_GNU_SOURCE -lpthread camsearch.c -o camsearch

debug:
	gcc -g -std=gnu11 -D_GNU_SOURCE -lpthread camsearch.c -o camsearch 

install:
	install -m 0755 $(PROGNAME) $(PREFIX)/bin/$(PROGNAME)

clean:
	rm -f camsearch
