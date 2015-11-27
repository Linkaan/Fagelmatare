all: temperature.out send_serial.out

temperature.out : temperature.c
	gcc -g -Wall -Werror `mysql_config --include` -D _GNU_SOURCE -o temperature.out temperature.c -lmysqlclient

send_serial.out : send_serial.c
	gcc -g -Wall -Werror -o send_serial.out send_serial.c -lwiringPi
