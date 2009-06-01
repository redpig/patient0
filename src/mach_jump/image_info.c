#include <dlfcn.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/types.h>
#include <mach/mach_init.h>
#include <mach/vm_map.h>
#include <mach/vm_prot.h>
#include <mach-o/dyld.h>
#include <mach-o/dyld_images.h>
#include <mach-o/getsect.h>
#include <mach-o/nlist.h>

#include <patient0/log.h>
#include <patient0/mach_jump/jump_table.h>
#include <patient0/mach_jump/image_info.h>

static struct dyld_all_image_infos *all_image_infos = NULL;

bool image_info_initialize() {
  /* Looking up dyld_all_image_infos as per mach-o/dyld_images.h */
  struct nlist l[8] = { 0 };
  struct nlist *list = &l[0];
  list->n_un.n_name = "_dyld_all_image_infos";
  /* Hmm I'd prefer to pull this from memory if possible ... */
  nlist("/usr/lib/dyld", list);
  if (list->n_value) {
    all_image_infos = (struct dyld_all_image_infos *) list->n_value;
    return true;
  }
  return false;
}


bool image_info_ready() {
  return image_info_initialize() && all_image_infos->infoArray;
}

uint32_t image_info_count() {
  if (!image_info_initialize()) {
    return 0;
  }
  return all_image_infos->infoArrayCount;
}

bool image_info_wait_until_ready() {
  /* TODO: gdb sets a breakpoint... */
  return false;
}

bool image_info_jump_table(uint32_t index, jump_table_t *table) {
  const struct mach_header *header;
  if (!image_info_ready()) {
    p0_logf(P0_ERR, "image info not currently ready");
    return false;
  }
  if (index >= all_image_infos->infoArrayCount) {
    p0_logf(P0_ERR, "index out of range");
    return false;
  }
  if (!table) {
    p0_logf(P0_ERR, "specified jump_table pointer is NULL");
    return false;
  }
  header = all_image_infos->infoArray[index].imageLoadAddress;
  p0_logf(P0_INFO, "loading image '%s'",
           all_image_infos->infoArray[index].imageFilePath);
  if (!header) {
    p0_logf(P0_ERR, "failed to acquire header for %d", index);
    return false;
  }
  table->addr = (intptr_t) getsectdatafromheader(header,
                                                 "__IMPORT",
                                                 "__jump_table",
                                                 (unsigned long *)
                                                 &table->size);
  p0_logf(P0_INFO, "header: %p addr %p", header, table->addr);
  if (table->addr == 0) {
    p0_logf(P0_ERR, "jump table mapped at 0x0: bailing");
    return false;
  }
  /* Make sure we can patch the table */
  if (vm_protect(mach_task_self(),
                 (vm_address_t)table->addr,
                 table->size,
                 false,
                 VM_PROT_ALL) != KERN_SUCCESS) {
    /* we will keep on truckin' though. just in case! */
    p0_logf(P0_WARN, "failed to change the protections on the jump table");
  }
  return true;
}

