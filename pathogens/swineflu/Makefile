include ../../Makefile.inc

libpatient0.a:
	make -C $(TOP)/src libpatient0.a
	cp $(TOP)/src/libpatient0.a ./

swineflu.bundle: swineflu.c libpatient0.a
	gcc $(CFLAGS) -DSYRINGE_BUNDLE swineflu.c libpatient0.a -bundle -o swineflu.bundle -lpthread

metasploit: swineflu.bundle

all: metasploit

clean:
	@rm libpatient0.a *.bundle &> /dev/null

.IGNORE: clean
