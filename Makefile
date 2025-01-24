CFLAGS = -g3 -Wall -Wextra -Wconversion -Wcast-qual -Wcast-align -g -D_GNU_SOURCE
CFLAGS += -Winline -Wfloat-equal -Wnested-externs
CFLAGS += -pedantic -std=gnu99 -Werror

PROMPT = -DPROMPT

EXECS = 33sh 33noprompt
SRC = sh.c jobs.c
CC = gcc

.PHONY: all clean 

all: $(EXECS)

33sh: $(SRC)
	$(CC) $(PROMPT) $(CFLAGS) -o $@ $^

33noprompt: $(SRC)
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -f $(EXECS)

