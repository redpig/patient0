INCLUDE_DIR=../include

mach_jump_test: mach_jump_test.c
	gcc -I$(INCLUDE_DIR) -ggdb3 -g mach_jump_test.c ../src/mach_jump/*.c -o mach_jump_test

dummy.bundle:
	gcc -bundle dummy.c -o dummy.bundle

dummy.h: dummy.bundle
	xxd -i dummy.bundle > dummy.h

infect_test: dummy.h infect_test.c
	gcc -I$(INCLUDE_DIR) -ggdb3 -g infect_test.c ../src/infect.c ../src/spawn.c -o infect_test

clean:
	@rm -rf *.o *.dSYM mach_jump_test infect_test dummy.h dummy.bundle &> /dev/null
