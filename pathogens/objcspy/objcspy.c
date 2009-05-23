/* objcspy.c
 * Some half-baked leftover code from tinkering around with obj-c interposition.
 * It needs _a lot_ of work if it is to ever be functional.
 * It depends on ffcall.  I used the ports version.
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

#include <patient0/log.h>
#include <patient0/mach_jump.h>
#include <patient0/runtime.h>

#include <objc/objc.h>
#include <objc/runtime.h>
#include <objc/message.h>
#include <avcall.h> /* from ffcall */


/* Replace Cocoa calls to the auth framework too. Really? */
typedef id (*objc_msgSend_t)(id, SEL, ...);

/* Since we can't pass the varargs, we don't have the luxury of just tracing and calling
 * the orig.  We have to determine how many args, then call it with them. Bummer.
 * ffcall may save us the hassle here of building a dynamic call.
 * http://www.haible.de/bruno/packages-ffcall.html
 */
id objcspy_objc_msgSend(id class_id, SEL selector, ...) {
  objc_msgSend_t orig = dlsym(RTLD_DEFAULT, "objc_msgSend");

  av_alist msgSend_args;
  va_list args;
  const char *arg_type;
  int arg_index;
  int arg_count;
  Method m;
  id ret;
  char type[50] = {0};
  Class theClass = object_getClass(class_id);
  p0_logf(P0_INFO, "[%s %s]", NAMEOF(class_id), SELNAME(selector));
  /* WHere is SFAuthorization? Maybe SendSuper? lookup_Class? send_fpret? */
  if ((m = class_getClassMethod(theClass, selector)) == nil) {
    if ((m = class_getInstanceMethod(theClass, selector)) == nil) {
      return nil;
    }
  }

  /* We can pass the addr in directly since we patched the jmp table */
  av_start_ptr(msgSend_args, &objc_msgSend, id, &ret);
  va_start(args, selector);

  /* Push on the first 2 */
  av_ptr(msgSend_args, id, class_id);
  av_ptr(msgSend_args, SEL, selector);

  /* Now we iterate over the arguments using their stack size to get it from vaargs */
  p0_logf(P0_INFO, "expecting size: %d", method_getSizeOfArguments(m));
  arg_count = method_getNumberOfArguments(m);
  for (arg_index = 0; arg_index < arg_count; ++arg_index)  {
    method_getArgumentType(m, arg_index, type, sizeof(type));
    switch (type[0]) {
      case _C_BFLD: /* b */
      case _C_ATOM: /* % */
      case _C_ARY_B: /* [ */
      case _C_ARY_E: /* ] */
      case _C_UNION_B: /* ( */
      case _C_UNION_E: /* ) */
      case _C_STRUCT_E: /* } */
      case _C_VECTOR: /* ! */
      case _C_CONST: /* r */
      case _C_UNDEF: /* ? */
        p0_logf(P0_INFO, "unhandled type: %s --> int?", type);
      case _C_BOOL: /* B */
      case _C_CHR: /* c */
      case _C_UCHR: /* C */
      case _C_SHT: /* s */
      case _C_USHT: /* S */
        /* promoted to int... */
      case _C_ID: /* @ */
      case _C_CLASS: /* # */
      case _C_SEL: /* : */
      case _C_INT: /* i */
      case _C_UINT: /* I */
      case _C_LNG: /* l */
      case _C_ULNG: /* L */
      case _C_VOID: /* v */
      case _C_PTR: /* ^ */
      case _C_CHARPTR: /* * */ {
        p0_logf(P0_INFO, "arg: %d type: %s as int", arg_index, type);
        int arg = va_arg(args, int);
        av_int(msgSend_args, arg);
        #if 0
        if (type[0] == _C_ID) {
          p0_logf(P0_INFO, "--> id: %s", NAMEOF((id)arg));
        } else if (type[0] == _C_SEL) {
          p0_logf(P0_INFO, "--> sel: %s", SELNAME((SEL)arg));
        }
        #endif
      }
      break;

      case _C_STRUCT_B: /* { */ /* treat a struct as a long long for now. when <8 bytes it's okay... */
      case _C_LNG_LNG: /* q */
      case _C_ULNG_LNG: /* Q */ {
        p0_logf(P0_INFO, "arg: %d type: %s as long long", arg_index, type);
        long long arg = va_arg(args, long long);
        av_longlong(msgSend_args, arg);
      }
      break;

      case _C_FLT: /* f */ 
        /* promoted to double */
      case _C_DBL: /* d */ {
        p0_logf(P0_INFO, "arg: %d type: %s as double", arg_index, type);
        double arg = va_arg(args, double);
        av_double(msgSend_args, arg);
      }
      break;

      default:
        p0_logf(P0_INFO, "unknown type: %s", type);
        /* treat at int */ {
        int arg = va_arg(args, int);
        p0_logf(P0_INFO, "arg: %d type: %s as int", arg_index, type);
        av_int(msgSend_args, arg);
      }
      break;
    }
  }
  va_end(args);
  /* do it. */
  av_call(msgSend_args);
  return ret; 
}

void run(unsigned char *code, uint32_t size) {
  unsigned char *cursor = code + size - sizeof(uint32_t);
  uint32_t args_size = *((uint32_t *)cursor);

  p0_logf(P0_INFO, "objcspy running");
  /* install function replacements */
  mach_jump_init();
  if (!mach_jump_patch("objc_msgSend", objcspy_objc_msgSend)) {
    p0_logf(P0_ERR, "Failed to patch objc_msgSend");
  }
  /* hang this thread */
  runtime_deadlock();
}
