#
# Makefile:
#	Temperature Handler - Program to log temperature to server
# Send Serial - Utility program to interact with Serial Handler
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

all: temperature.out send_serial.out send_event.out

temperature.out : temperature.c
	gcc -g -Wall `mysql_config --include` -D _GNU_SOURCE -o temperature.out temperature.c -lmysqlclient

send_serial.out : send_serial.c
	gcc -g -Wall -o send_serial.out send_serial.c -D _GNU_SOURCE

send_event.out : send_event.c
	gcc -g -Wall -o send_event.out send_event.c -D _GNU_SOURCE
