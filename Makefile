include Makefile.inc
VERSION = $(shell tail -1 $(TOP)/VERSION | cut -f1 -d' ')
SUBDIRS = src
.PHONY: metasploit_bundles build standalone_bin
.IGNORE: clean

# Normally used targets
all: metasploit standalone README.html

metasploit: metasploit_bundles
	cp -r extras/metasploit/* build/metasploit/
	@echo "To use: cp -r build/metasploit/* your/metasploit-root/"

standalone: standalone_bin
	@echo "Run ./build/bin/syringe"

#
# Distribution related targets
#

README.html: README.markdown
	bluecloth README.markdown > README.html

# distclean adds a rendered readme.
distclean: clean README.html

# Build a patient0-$(VERSION).tar.gz
dist: distclean
	mkdir -p dist/patient0-$(VERSION)
	tar --exclude .git --exclude dist -cf - $(TOP) | tar -C dist/patient0-$(VERSION) -xf -
	tar -C dist -V patient0-$(VERSION) --owner nobody --group nobody -czvf patient0-$(VERSION).tar.gz patient0-$(VERSION)

#
# Subdir targets
#
builddir:
	@mkdir -p build/bin &> /dev/null
	@mkdir -p build/metasploit/data &> /dev/null

metasploit_bundles: builddir
	$(MAKE) -C src metasploit
	$(MAKE) -C pathogens/rubella metasploit
	@mkdir -p build/metasploit/data &> /dev/null
	cp src/syringe.bundle build/metasploit/data
	cp pathogens/rubella/rubella.bundle build/metasploit/data

standalone_bin: builddir
	$(MAKE) -C src standalone
	cp src/syringe build

clean:
	$(MAKE) -C src clean
	$(MAKE) -C pathogens/rubella clean
	@rm -f *.tar.gz &> /dev/null
	@rm -f README.html &> /dev/null
	@rm -rf build dist &> /dev/null
