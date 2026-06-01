CC       = gcc
CFLAGS   = -Wall -Wextra -std=c11
MINIZ_W  = -w
PREFIX   = cres_
GUARD    = RESOURCES_H

# Compile miniz as a separate object with warnings suppressed
miniz.o: miniz.c miniz.h
	$(CC) -std=c11 $(MINIZ_W) -DCRES_COMPRESSION -c -o $@ miniz.c

# --- Tool (with compression support) ---
cres: cres_tool.c miniz.o
	$(CC) $(CFLAGS) -DCRES_COMPRESSION -o $@ cres_tool.c miniz.o

# --- Demo: generate resources, then compile ---
demo/resources.h demo/resources.c: cres demo/resources.txt
	./cres -m demo/resources.txt -o demo/resources --guard $(GUARD) --prefix $(PREFIX)

demo/demo: demo/main.c demo/resources.c demo/resources.h cres.c cres.h miniz.o
	$(CC) $(CFLAGS) -DCRES_COMPRESSION -I. -Idemo -o $@ demo/main.c demo/resources.c cres.c miniz.o

demo: demo/demo
	@echo "Run: ./demo/demo"

# --- Tests ---
test/resources.h test/resources.c: cres test/resources.txt
	./cres -m test/resources.txt -o test/resources

test/test_cres: test/test_cres.c test/resources.c test/resources.h cres.c cres.h miniz.o
	$(CC) $(CFLAGS) -DCRES_COMPRESSION -I. -Itest -o $@ test/test_cres.c test/resources.c cres.c miniz.o

test: test/test_cres
	./test/test_cres

ifeq ($(OS),Windows_NT)
  RM = del /f /q
  SLASH = \\
else
  RM = rm -f
  SLASH = /
endif

clean:
	-$(RM) cres miniz.o demo$(SLASH)demo demo$(SLASH)resources.h demo$(SLASH)resources.c test$(SLASH)test_cres test$(SLASH)resources.h test$(SLASH)resources.c

.PHONY: demo test clean
