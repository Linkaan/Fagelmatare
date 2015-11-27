IDIR := ./include
CFLAGS := -I$(IDIR) -std=gnu11 -O3 -g -gdwarf-3 -Wall -Werror -march=armv6zk -mfloat-abi=hard -mfpu=vfp -D _GNU_SOURCE #-D _BSD_SOURCE
CC := gcc
NAME := shandler
SRCDIR := ./src
OUTDIR := ./bin

all: $(OUTDIR)/$(NAME)

$(OUTDIR)/$(NAME): log.o config.o lstack.o ehandler.o $(NAME).o
	$(CC) $^ -o $@ -lpthread -lwiringPi -ldblogger

$(NAME).o: $(SRCDIR)/$(NAME).c
	$(CC) $(CFLAGS) `mysql_config --include` -c -o $@ $^

ehandler.o: $(SRCDIR)/ehandler.c
	$(CC) $(CFLAGS) `mysql_config --include` -c -o $@ $^

lstack.o: $(SRCDIR)/lstack.c
	$(CC) $(CFLAGS) -c -o $@ $^

config.o: $(SRCDIR)/config.c
	$(CC) $(CFLAGS) -c -o $@ $^

log.o: $(SRCDIR)/log.c
	$(CC) $(CFLAGS) `mysql_config --include` -c -o $@ $^

clean:
	$(RM) *.o $(OUTDIR)/*
