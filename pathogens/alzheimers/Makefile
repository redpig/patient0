include ../../Makefile.inc

libpatient0.a:
	make -C $(TOP)/src libpatient0.a
	cp $(TOP)/src/libpatient0.a ./

alzheimers.bundle: alzheimers.c libpatient0.a
	gcc $(CFLAGS) -DSYRINGE_BUNDLE alzheimers.c libpatient0.a -bundle -o alzheimers.bundle -lpthread

metasploit: alzheimers.bundle

all: metasploit

clean:
	@rm -rf *.dSYM libpatient0.a *.bundle &> /dev/null

.IGNORE: clean
