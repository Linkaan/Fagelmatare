#
# Makefile:
#	Fagelmatare-Zero - Main program for Fågelmataren running on the Raspberry Pi zero
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
CFLAGS := -I$(IDIR) -std=gnu11 -O3 -g -gdwarf-3 -Wall -I/usr/include/mysql -D _GNU_SOURCE -mfpu=vfp -mfloat-abi=hard -march=armv6zk -mtune=arm1176jzf-s
CC := gcc
NAME := fagelmatare
SRCDIR := ./src
OUTDIR := ./bin

all: $(OUTDIR)/$(NAME)

$(OUTDIR)/$(NAME): log.o config.o lstack.o sensors.o $(NAME).o
	$(CC) $^ -o $@ -lpthread -ldblogger -lm -lmysqlclient_r

$(NAME).o: $(SRCDIR)/$(NAME).c
	$(CC) $(CFLAGS) `mysql_config --include` -c -o $@ $^

sensors.o: $(SRCDIR)/sensors.c
	$(CC) $(CFLAGS) -c -o $@ $^

lstack.o: $(SRCDIR)/lstack.c
	$(CC) $(CFLAGS) -c -o $@ $^

config.o: $(SRCDIR)/config.c
	$(CC) $(CFLAGS) -c -o $@ $^

log.o: $(SRCDIR)/log.c
	$(CC) $(CFLAGS) `mysql_config --include` -c -o $@ $^

clean:
	$(RM) *.o $(OUTDIR)/*
