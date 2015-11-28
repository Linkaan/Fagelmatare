#
# Makefile:
#	Temperature Handler - Program to log temperature to server
# Send Serial - Utility program to interact with Serial Handler
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

all: temperature.out send_serial.out

temperature.out : temperature.c
	gcc -g -Wall -Werror `mysql_config --include` -D _GNU_SOURCE -o temperature.out temperature.c -lmysqlclient

send_serial.out : send_serial.c
	gcc -g -Wall -Werror -o send_serial.out send_serial.c -lwiringPi -D _GNU_SOURCE
