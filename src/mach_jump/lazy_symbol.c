/*
 *
 * Copyright (c) 2009 Will Drewry <redpig@dataspill.org>. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file or at http://github.org/redpig/patient0.
 * vim:tw=80:ts=2:et:sw=2
 */
#include <assert.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <libkern/OSByteOrder.h>
#include <mach/mach_init.h>
#include <mach/vm_prot.h>
#include <mach/vm_map.h>
#include <mach-o/dyld.h>
#include <mach-o/getsect.h>
#include <mach-o/nlist.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <patient0/log.h>
#include <patient0/mach_jump/lazy_symbol.h>
#include <patient0/mach_jump/jump_table.h>

static const struct segment_command *linkedit;
static bool linkedit_initialized = false;

static struct symtab_command *symtab = NULL;
static uint32_t *indirect_symtab = NULL;
static uint32_t indirect_symtab_size = 0;
static bool lazy_symbol_initialized = false;


bool linkedit_init() {
  linkedit_initialized = false;
  linkedit = getsegbyname("__LINKEDIT");
  if (linkedit) {
    linkedit_initialized = true;
  }
  return linkedit_initialized;
}

/* lazy_symbol_init
 *
 * This function will attempt to find and store references to
 * the indirect symbol table and the symbol table to allow names
 * to be assigned to jump table entries.
 *
 * This is not configured as a constructor just in case the memory
 * isn't laid out as expected and this function becomes risky.
 */
bool lazy_symbol_init() {
  const struct mach_header *mh;
  struct dysymtab_command *dysymtab = NULL;
  struct load_command *load_command;
  int cmd = 0;

  if (lazy_symbol_initialized) {
    return true;
  }
  if (!linkedit_initialized && !linkedit_init()) {
    p0_logf(P0_INFO, "linkedit_init() is required for lazy_symbol_init\n");
    return false;
  }
  /* Acquire the MACH-O header for the executable image */
  mh = (const struct mach_header *)_dyld_get_image_header(0);
  if (!mh) {
    return false;
  }
  /* The first load command (LC) follows immediately after the header.
   * Hopefully.
   */
  load_command = ((void *)mh) + sizeof(struct mach_header);

   /* Iterate over all load commands looking for the dysymtab and
    * the symtab.  The dysymtab will give us the indirect symbol
    * table.  The indirect symbol table provides the index into the symbol
    * table.  In addition, the indirect symbol table entries match
    * the jump table entries (one for one) offset by an index given in
    * the jump_table->reserved1 variable.  Once we have the symtab and
    * the dysymtab, we can perform that calculation on demand.
    */
  for ( ; cmd < mh->ncmds;
          ++cmd, load_command=((void*)load_command)+load_command->cmdsize) {
    switch (load_command->cmd) {
      case LC_DYSYMTAB:
        dysymtab = (struct dysymtab_command *)load_command;
        break;
      case LC_SYMTAB:
        symtab = (struct symtab_command *)load_command;
        break;
    }
  }
  if (symtab && dysymtab) {
    /* Setup the indirect symbol table */
    indirect_symtab = (uint32_t *)(dysymtab->indirectsymoff -
                                   symtab->symoff +
                                   linkedit->vmaddr);
    indirect_symtab_size = dysymtab->nindirectsyms;
    lazy_symbol_initialized = true;
  }
  p0_logf(P0_INFO, "symtab: %p dysymtab: %p\n", symtab, dysymtab);
  return lazy_symbol_initialized;
}


/* lazy_symbol_stub
 * Walks the symbol table looking for the given symbol.  Once found, it attempts
 * to determine the address where the jump table stub is located in memory.
 * If the symbol is not indirect or cannot be found, this will return 0.
 * This will test for a match with and without a "_", so don't include one
 * unless you think there'd be more than one.
 * TODO: getsectdatafromFramework("SecurityFoundation", "__IMPORT", "__jump_table", &size)
 *       should drop it right in the stub section. maybe
 *     _dyld_get_image_vmaddr_slide + result = addr.
 *
 */
intptr_t lazy_symbol_stub(const char *symbol) {
  /* nlists are symbol table list elements */
  struct nlist *nl = (struct nlist *)linkedit->vmaddr;
  size_t symbol_length = strlen(symbol);
  uint32_t symbol_index = 0;
  intptr_t match = 0;

  if (!lazy_symbol_initialized && !lazy_symbol_init()) {
    p0_logf(P0_INFO, "lazy_symbol_stub requires lazy_symbol_init()\n");
    return 0;
  }
  if (!jump_table_init()) {
    p0_logf(P0_INFO, "lazy_symbol_stub requires jump_table_init()\n");
    return 0;
  }

  /* Walk the symbol table looking for the requested symbol.
   * Once found, index into the jump table if possible.
   */
  for ( ; symbol_index < symtab->nsyms; ++symbol_index, ++nl ) {
    /* The entry name lives in the strtab.  This is located at the linkedit
     * address offset by the stroff value in the symtab.  This subtracts the
     * symoff from the stroff to get the offset into linkedit.  symoff and
     * stroff are absolute offsets into the original file, but when running,
     * symoff == linkedit->vmaddr.
     * TODO: add error checking for the value in n_strx.
     */
    char *entry_name = (char *)(linkedit->vmaddr + nl->n_un.n_strx +
                                (symtab->stroff - symtab->symoff));
    /* TODO: add verbosity levels to p0_logf so we can log things
     *       like all the entry names, when needed.
     */
    /* If the symbol isn't external, this probably won't be useful */
    if (nl->n_desc & N_EXT) {
      uint32_t indirect_index = 0;
      jump_table_t *jump_table = jump_table_global();
      /* Do nothing if this isn't the right symbol */
      if (strncmp(entry_name, symbol, symbol_length)) {
        if (entry_name[0] == '_' && symbol[0] != '_') {
          if (strncmp(++entry_name, symbol, symbol_length)) {
            continue;
          }
        } else {
          continue;
        }
      }
      /* strncmp will match a partial. E.g., open$UNIX2003 versus open */
      if (entry_name[symbol_length] != '\0') {
        continue;
      }
      /* According to the comments in mach-o/loader.h:417, the index into the
       * jump_table should point to an entry in the indirect symbol table
       * starting at the index offset in reserved1.  So if we walk the
       * indirect symbol table here, we will get the index that equates to
       * this symbol. Then when we can guess the jump table index using indir
       * sym index + reserved1.
       */
      for ( ; indirect_index < indirect_symtab_size; ++indirect_index) {
        if (indirect_symtab[indirect_index] == symbol_index) {
          jmp_entry_t *entry = (jmp_entry_t *)jump_table->addr;
          int jump_index = indirect_index - jump_table->reserved1;
          if (jump_index < 0) {
            continue;
          }
          /* Now we index into the jump_table */
          //entry = ((uint32_t)entry) + (jump_index * jump_table->reserved2);
          entry += jump_index;
          if ((uint32_t)entry > (jump_table->addr + jump_table->size)) {
            p0_logf(P0_INFO, "invalid jump target: %p\n", entry);
            return 0;
          }
          /* Return the pointer to the stub entry. */
          /* Instead of returning here, we'll log, note, and keep going.
           * Maybe there are dupes! */
          match = (intptr_t)entry;
          p0_logf(P0_INFO, "'%s' matched @ %p", entry_name, entry);
        }
      }
    }
  }
  return match;
}
