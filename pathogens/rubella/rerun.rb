# rerun.rb - re-runs a supplied script in a new ruby interpreter
#
# Usage:
#   ruby /dev/fd/1
# Escapes safe level == 1 restrictions by spawning a new ruby interpreter
# and passes along the first argument as an integer and then writes the
# "real" script to the pipe.
#
# Copyright (c) 2009 Will Drewry <redpig@dataspill.org>. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file or at http://github.org/redpig/patient0.

# REAL_SCRIPT must be supplied by the caller.
#REAL_SCRIPT=String.new(<<'EOF')
#XXXREALSCRIPTXXX
#EOF

# Here we escape the setuid SAFE level
if Process.uid != 0
  Process.uid = 0
  Process.euid = 0
  Process.gid = 0
  Process.egid = 0
  a = []
  ARGV.each{|arg| entry = arg.dup; entry.untaint; a << entry }
  # Try not to shell-inject yourself.
  pipe = IO.popen("/usr/bin/ruby - #{a.join(" ")}", "w")
  pipe.write(REAL_SCRIPT)
  pipe.close
end

