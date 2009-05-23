include ../Makefile.inc

.IGNORE: clean

LIBPATIENT0_SOURCES=spawn.c infect.c process.c mach_jump/libdupe.c mach_jump/clobber.c mach_jump/mach_jump.c mach_jump/jump_table.c mach_jump/lazy_symbol.c runtime.c log.c

LIBPATIENT0_OBJS=spawn.o infect.o process.o mach_jump/libdupe.o mach_jump/clobber.o mach_jump/mach_jump.o mach_jump/jump_table.o mach_jump/lazy_symbol.o runtime.o log.o

libpatient0.a: inject_bundle.h $(LIBPATIENT0_OBJS)
	libtool -static -o libpatient0.a $(LIBPATIENT0_OBJS)

inject_bundle.h: ../ext/_inject_bundle_from_mem.s
	nasm ../ext/_inject_bundle_from_mem.s -o inject_bundle
	xxd -i inject_bundle > inject_bundle.h
	rm inject_bundle

patient0.bundle: patient0.c libpatient0.a
	gcc $(CFLAGS) patient0.c libpatient0.a -lpthread -framework CoreServices -o patient0.bundle -bundle -lpthread

patient0_bundle.h: patient0.bundle
	xxd -i patient0.bundle > patient0_bundle.h

syringe: patient0_bundle.h syringe.c libpatient0.a
	gcc $(CFLAGS) syringe.c -o syringe libpatient0.a -lpthread

syringe.bundle: patient0_bundle.h syringe.c libpatient0.a
	gcc $(CFLAGS) -DSYRINGE_BUNDLE syringe.c libpatient0.a -bundle -o syringe.bundle -lpthread

standalone: syringe

metasploit: syringe.bundle

# Dummy testing targets
spawn_main: spawn.c
	gcc -I$(LIBPATIENT0_INCLUDE) -ggdb3 -g spawn.c -o spawn_main -DSPAWN_MAIN

process_main: process.c
	gcc -I$(LIBPATIENT0_INCLUDE) -ggdb3 -g process.c -o process_main -DPROCESS_MAIN

all: standalone metasploit

clean:
	@rm -rf *.bundle *.o mach_jump/*.o *.dSYM syringe spawn_main process_main inject_bundle.h patient0_bundle.h *.a &> /dev/null
