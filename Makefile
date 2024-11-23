BINDIR := $(PREFIX)/bin
CFLAGS := -Os -Wall

ptywrap: ptywrap.c Makefile
	$(CC) $(CFLAGS) -o $@ $(filter %.c,$^)

install: ptywrap
	mkdir -p $(DESTDIR)$(BINDIR)
	install -s ptywrap $(DESTDIR)$(BINDIR)

clean:
	rm -f ptywrap

.PHONY: install clean
