IDIR := ./include
CFLAGS := -I$(IDIR) -O3 -Wall -Werror -fPIC -march=armv6zk -mfloat-abi=hard -mfpu=vfp -I/usr/include/mysql -DBIG_JOINS=1 -fno-strict-aliasing -g
CC := gcc
MAJOR := 0
MINOR := 1
NAME := dblogger
SRCDIR := ./src
OUTDIR := ./lib
VERSION := $(MAJOR).$(MINOR)

all: $(OUTDIR)/lib$(NAME).so.$(VERSION)

$(OUTDIR)/lib$(NAME).so.$(VERSION): $(NAME).o
	$(CC) -shared -Wl,-soname,lib$(NAME).so.$(MAJOR) $^ -o $@ -L/usr/lib/arm-linux-gnueabihf -lmysqlclient -lpthread -lz -lm -lrt -ldl

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
