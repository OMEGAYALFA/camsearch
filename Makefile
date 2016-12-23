PREFIX = /usr
PROGNAME = camsearch

build: clean
	gcc -Wall -O2 -std=gnu11 -lpthread camsearch.c -o camsearch

debug:
	gcc -Wall -g -std=gnu11 -lpthread camsearch.c -o camsearch 

install:
	install -m 0755 $(PROGNAME) $(PREFIX)/bin/$(PROGNAME)

clean:
	rm -f camsearch
