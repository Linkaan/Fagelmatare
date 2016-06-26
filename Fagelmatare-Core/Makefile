#
# Makefile:
#	Fagelmatare-Core - Core program for Fågelmataren running on the Raspberry Pi
##############################################################################
#  This file is part of Fågelmataren, an advanced bird feeder equipped with
#  many peripherals. See <https://github.com/Linkaan/Fagelmatare>
#  Copyright (C) 2015-2016 Linus Styrén
#
#  Fågelmataren is free software; you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation; either version 3 of the Licence, or
#  (at your option) any later version.
#
#  Fågelmataren is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public Licence for more details.
#
#  You should have received a copy of the GNU General Public Licence
#  along with Fågelmataren.  If not, see <http://www.gnu.org/licenses/>.
##############################################################################

IDIR := ./include
CFLAGS := -I$(IDIR) -std=gnu11 -g -O3 -gdwarf-3 -Wall -D _GNU_SOURCE -mfpu=neon-vfpv4 -mfloat-abi=hard -march=armv7-a -mtune=cortex-a7
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
	$(CC) $(CFLAGS) `mysql_config --include` -c -o $@ $^

config.o: $(SRCDIR)/config.c
	$(CC) $(CFLAGS) -c -o $@ $^

log.o: $(SRCDIR)/log.c
	$(CC) $(CFLAGS) `mysql_config --include` -c -o $@ $^

clean:
	$(RM) *.o $(OUTDIR)/*
