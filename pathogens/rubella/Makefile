include ../../Makefile.inc

libpatient0.a:
	make -C $(TOP)/src libpatient0.a
	cp $(TOP)/src/libpatient0.a ./

rubella.bundle: rubella.c libpatient0.a rubella_payload.h rerun_rb.h pivot_rb.h
	gcc $(CFLAGS) -DSYRINGE_BUNDLE rubella.c libpatient0.a -bundle -o rubella.bundle -lpthread -I/System/Library/Frameworks/Security.framework/Headers/ -lpthread

# Doesn't play nice in threads when doing more than printing.
#rubyist.bundle: rubyist.c libpatient0.a
#	gcc rubyist.c -o rubyist.bundle -I/System/Library/Frameworks/Ruby.framework/Headers -bundle $(CFLAGS) libpatient0.a  -lruby -lpthread

tclist.bundle: tclist.c libpatient0.a
	gcc tclist.c -o tclist.bundle -bundle $(CFLAGS) libpatient0.a -lpthread -framework Tcl

# We could grep out comments and compress it, but payload size isn't a concern
# right now.
rerun_rb.h: rerun.rb
	xxd -i rerun.rb > rerun_rb.h

pivot_rb.h: pivot.rb
	xxd -i pivot.rb > pivot_rb.h

#rubella_payload.h: rubyist.bundle server.rb
#	cp rubyist.bundle rubella.payload.tmp
#	cat server.rb | grep -vEe '^[ ]*#' >> rubella.payload.tmp
#	ruby -e 'print [File.stat("server.rb").size].pack("V")' >> rubella.payload.tmp
#	cat rubella.payload.tmp | xxd -i > rubella.payload
#	xxd -i rubella.payload > rubella_payload.h

rubella_payload.h: tclist.bundle web.tcl
	cp tclist.bundle rubella.payload.tmp
	cat web.tcl | grep -vEe '^[ ]*#' >> rubella.payload.tmp
	ruby -e 'print [File.stat("web.tcl").size].pack("V")' >> rubella.payload.tmp
	cat rubella.payload.tmp | xxd -i > rubella.payload
	xxd -i rubella.payload > rubella_payload.h

metasploit:  rubella.bundle

all: metasploit

clean:
	@rm -rf *.dSYM &> /dev/null
	@rm rubyist_bundle.h pivot_rb.h rerun_rb.h rubella.payload rubella.payload.tmp rubella_payload.h libpatient0.a *.bundle &> /dev/null

.IGNORE: clean
