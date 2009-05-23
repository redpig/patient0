include ../../Makefile.inc

libpatient0.a:
	make -C $(TOP)/src libpatient0.a
	cp $(TOP)/src/libpatient0.a ./

objcspy.bundle: objcspy.c libpatient0.a
	gcc $(CFLAGS) -DSYRINGE_BUNDLE objcspy.c libpatient0.a -bundle -o rubella.bundle -lpthread  -lpthread -lobjc /opt/local/lib/libavcall.a -I/opt/local/include

metasploit: objcspy.bundle

all: metasploit

clean:
	@rm libpatient0.a *.bundle &> /dev/null

.IGNORE: clean
