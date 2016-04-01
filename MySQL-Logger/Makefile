#
# Makefile:
#	MySQL-Logger - Library to send error/info logs to Fågelmatare Server
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
CFLAGS := -I$(IDIR) -O3 -Wall -fPIC -march=armv6zk -mfloat-abi=hard -mfpu=vfp -I/usr/include/mysql -DBIG_JOINS=1 -fno-strict-aliasing -g -D _GNU_SOURCE
CC := gcc
MAJOR := 0
MINOR := 6
NAME := dblogger
SRCDIR := ./src
OUTDIR := ./lib
VERSION := $(MAJOR).$(MINOR)

all: $(OUTDIR)/lib$(NAME).so.$(VERSION)

$(OUTDIR)/lib$(NAME).so.$(VERSION): $(NAME).o
	$(CC) -shared -Wl,-soname,lib$(NAME).so.$(MAJOR) $^ -o $@ -L/usr/lib/arm-linux-gnueabihf -lmysqlclient_r -lpthread -lz -lm -lrt -ldl

$(NAME).o: $(SRCDIR)/$(NAME).c
	$(CC) $(CFLAGS) -c -o $@ $(SRCDIR)/$(NAME).c

install: all
	install -m 0755 $(OUTDIR)/lib$(NAME).so.$(VERSION) /usr/lib
	/sbin/ldconfig
	ln -nsf /usr/lib/lib$(NAME).so.$(VERSION) /usr/lib/lib$(NAME).so
	mkdir -p -m 0755 /usr/include/libdblogger
	install -m 0755 include/dblogger.h /usr/include/libdblogger
	install -m 0755 include/log_entry.h /usr/include/libdblogger

clean:
	$(RM) *.o $(OUTDIR)/*.so*
