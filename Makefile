CDEBUGFLAGS = -O
SOLARISFLAGS = -DSVR4

all: idler

idler: idler.c
	cc $(CDEBUGFLAGS) $(SOLARISFLAGS) -o idler idler.c -lrpcsvc -lm
	
clean:
	rm -f idler idler.o

install: idler
	install -d $(DESTDIR)/usr/bin $(DESTDIR)/usr/share/man/man1
	install idler $(DESTDIR)/usr/bin/idler
	install idler.1 $(DESTDIR)/usr/share/man/man1/idler.1
