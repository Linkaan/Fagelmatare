#
# Makefile:
#	Fagelmatare-Zero - Main program for Fågelmataren running on the Raspberry Pi zero
#
#	Copyright (C) 2015 Linus Styrén
##############################################################################
#  This file is part of Fågelmataren:
#    https://github.com/Linkaan/Fagelmatare/
#
#  This library is free software; you can redistribute it and/or
#  modify it under the terms of the GNU Lesser General Public
#  License as published by the Free Software Foundation; either
#  version 2.1 of the License, or (at your option) any later version.
#
#  This library is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
#  Lesser General Public License for more details.
#
#  You should have received a copy of the GNU Lesser General Public
#  License along with this library; if not, write to the Free Software
#  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301
#  USA
##############################################################################

IDIR := ./include
CFLAGS := -I$(IDIR) -std=gnu11 -O3 -g -gdwarf-3 -Wall -Werror -march=armv6zk -mfloat-abi=hard -mfpu=vfp -D _GNU_SOURCE
CC := gcc
NAME := fagelmatare
SRCDIR := ./src
OUTDIR := ./bin

all: $(OUTDIR)/$(NAME)

$(OUTDIR)/$(NAME): log.o config.o lstack.o $(NAME).o
	$(CC) $^ -o $@ -lpthread -ldblogger

$(NAME).o: $(SRCDIR)/$(NAME).c
	$(CC) $(CFLAGS) `mysql_config --include` -c -o $@ $^

lstack.o: $(SRCDIR)/lstack.c
	$(CC) $(CFLAGS) -c -o $@ $^

config.o: $(SRCDIR)/config.c
	$(CC) $(CFLAGS) -c -o $@ $^

log.o: $(SRCDIR)/log.c
	$(CC) $(CFLAGS) `mysql_config --include` -c -o $@ $^

clean:
	$(RM) *.o $(OUTDIR)/*
