# -*- encoding: utf-8 -*-
#
# The MIT License (MIT)
#
# Copyright (c) 2019 Tobias Koch <tobias.koch@gmail.com>
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.
#

import os
import re
import pwd
import signal
import time

from org.boltlinux.buildbox.error import BBoxError

def homedir():
    pw = pwd.getpwuid(os.geteuid())
    if pw:
        home = pw.pw_dir
    if not home:
        raise BBoxError(
            "unable to determine user home directory."
        )
    return os.path.normpath(os.path.realpath(home))
#end function

def valid_arch(arch):
    return arch in [
        "aarch64",
        "armv4t",
        "armv6",
        "armv7a",
        "i686",
        "mipsel",
        "mips64el",
        "powerpc",
        "powerpc64le",
        "s390x",
        "x86_64"
    ]
#end function

def target_for_machine(machine):
    if machine.startswith("aarch64"):
        return "aarch64-linux-musl"
    if machine.startswith("armv4t"):
        return "armv4-linux-musleabi"
    if machine.startswith("armv6"):
        return "armv6-linux-musleabihf"
    if machine.startswith("armv7a"):
        return "armv7a-linux-musleabihf"
    if re.match(r"^i\d86.*$", machine):
        return "i686-bolt-linux-musl"
    if machine.startswith("mips64el"):
        return "mips64el-linux-musl"
    if re.match(r"^mips\d*el.*$", machine):
        return "mipsel-linux-musl"
    if re.match(r"^powerpc64(?:le|el).*$", machine):
        return "powerpc64le-linux-musl"
    if machine.startswith("powerpc"):
        return "powerpc-linux-musl"
    if machine.startswith("s390x"):
        return "s390x-linux-musl"
    if re.match(r"^x86[-_]64$", machine):
        return "x86_64-bolt-linux-musl"
#end function

def kill_chrooted_processes(chroot):
    chroot = os.path.normpath(os.path.realpath(chroot))

    for entry in os.listdir("/proc"):
        try:
            pid = int(entry)

            proc_root = os.path.normpath(
                os.path.realpath("/proc/{}/root".format(entry))
            )

            proc_entry = "/proc/{}".format(entry)

            if chroot == proc_root:
                os.kill(-pid, signal.SIGTERM)
                for i in range(10):
                    os.lstat(proc_entry)
                    time.sleep(0.05 * 1.1**i)

                os.kill(-pid, signal.SIGKILL)
                for i in range(10):
                    os.lstat(proc_entry)
                    time.sleep(0.05 * 1.1**i)
            #end if
        except (ValueError, ProcessLookupError, PermissionError,
                    FileNotFoundError):
            pass
    #end for
#end function
