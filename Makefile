PROJDIRS := .

MAIN_SRC := main.c
MAIN_EXE := $(patsubst %.c,%.exe,$(MAIN_SRC))

HDRFILES := $(shell find $(PROJDIRS) -type f -name "\*.h")
SRCFILES := $(filter-out $(TESTS_SRC) $(MAIN_SRC),$(wildcard *.c))
OBJFILES := $(patsubst %.c,%.o,$(SRCFILES))

WARNINGS := -Wall -Wextra -pedantic -Wshadow -Wpointer-arith -Wcast-align \
            -Wwrite-strings -Wmissing-prototypes -Wmissing-declarations \
            -Wredundant-decls -Wnested-externs -Winline -Wno-long-long \
            -Wconversion -Wstrict-prototypes -Werror

CFLAGS := -g -std=c17 $(WARNINGS)

DEPFLAGS := -MMD -MP

CC := clang

.PHONY: test main all clean

build: $(MAIN_SRC) $(SRCFILES)
	$(CC) $(CFLAGS) $^ $(DEPFLAGS) -o $(MAIN_EXE)
	chmod a+x $(MAIN_EXE)

test: build
	./$(MAIN_EXE) --test

all: build test

clean:
	rm -rf $(OBJFILES) $(MAIN_EXE)
