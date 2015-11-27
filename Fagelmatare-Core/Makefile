IDIR := ./include
CFLAGS := -I$(IDIR) -std=gnu11 -O3 -g -gdwarf-3 -Wall -Werror -march=armv6zk -mfloat-abi=hard -mfpu=vfp -D _GNU_SOURCE
CC := gcc
NAME := fagelmatare
SRCDIR := ./src
OUTDIR := ./bin

all: $(OUTDIR)/$(NAME)

$(OUTDIR)/$(NAME): log.o config.o state.o lstack.o $(NAME).o
	$(CC) $^ -o $@ -lpthread -lwiringPi -ldblogger

$(NAME).o: $(SRCDIR)/$(NAME).c
	$(CC) $(CFLAGS) `mysql_config --include` -c -o $@ $^

lstack.o: $(SRCDIR)/lstack.c
	$(CC) $(CFLAGS) -c -o $@ $^

state.o: $(SRCDIR)/state.c
	$(CC) $(CFLAGS) -c -o $@ $^

config.o: $(SRCDIR)/config.c
	$(CC) $(CFLAGS) -c -o $@ $^

log.o: $(SRCDIR)/log.c
	$(CC) $(CFLAGS) `mysql_config --include` -c -o $@ $^

clean:
	$(RM) *.o $(OUTDIR)/*
