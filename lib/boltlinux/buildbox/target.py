# -*- encoding: utf-8 -*-
#
# The MIT License (MIT)
#
# Copyright (c) 2019-2021 Tobias Koch <tobias.koch@gmail.com>
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
import subprocess
import shutil
import signal
import sys

from boltlinux.buildbox.error import BuildBoxError
from boltlinux.buildbox.generator import BuildBoxGenerator
from boltlinux.buildbox.misc.paths import Paths
from boltlinux.buildbox.sysroot import Sysroot

class BuildBoxTarget:

    @classmethod
    def init(cls):
        cmd = [sys.argv[0], "init"]
        try:
            subprocess.run(
                cmd,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                universal_newlines=True,
                check=True
            )
        except subprocess.CalledProcessError as e:
            raise BuildBoxError(e.stderr.strip())
    #end function

    @classmethod
    def create(cls, target_name, *specs, **kwargs):
        cls.init()

        if not re.match(r"^[-_a-zA-Z0-9.]+$", target_name):
            raise BuildBoxError(
                "the target name must consist only of characters "
                "matching [-_a-zA-Z0-9.]"
            )

        target_prefix = kwargs.get("target_prefix", Paths.target_prefix())

        if not os.path.isdir(target_prefix):
            try:
                os.makedirs(target_prefix)
            except OSError as e:
                raise BuildBoxError(
                    "failed to create target prefix '{}': {}"
                    .format(target_prefix, str(e))
                )
        #end if

        target_dir = os.path.join(target_prefix, target_name)

        if os.path.exists(target_dir) and os.listdir(target_dir):
            if kwargs.get("force"):
                cls.delete([target_name], **kwargs)
            else:
                raise BuildBoxError(
                    "target '{}' already exists, aborting."
                    .format(target_name)
                )
            #end if
        #end if

        try:
            os.mkdir(target_dir)
        except OSError as e:
            raise BuildBoxError(
                'failed to create target folder "{}": {}'
                .format(target_dir, e)
            )

        try:
            image_gen = BuildBoxGenerator(copy_qemu=True, **kwargs)
            image_gen.prepare(target_dir, target_name)

            with Sysroot(target_dir):
                image_gen.bootstrap(target_dir, specs, **kwargs)
        except (KeyboardInterrupt, Exception):
            old_sig_handler = signal.signal(signal.SIGINT, signal.SIG_IGN)

            try:
                cls._delete(target_name, **kwargs)
            except Exception:
                pass
            finally:
                signal.signal(signal.SIGINT, old_sig_handler)

            raise
        #end try
    #end function

    @classmethod
    def list(cls, **kwargs):
        target_prefix = kwargs.get("target_prefix", Paths.target_prefix())
        if not os.path.isdir(target_prefix):
            return

        for entry in sorted(os.listdir(target_prefix)):
            machine = "defunct"

            if not os.path.isdir(os.path.join(target_prefix, entry)):
                continue

            shell_found = False

            for prefix in ["usr", "tools"]:
                shell = os.path.join(target_prefix, entry, prefix, "bin", "sh")
                if os.path.exists(shell):
                    shell_found = True
                    break
            #end for

            etc_target = os.path.join(target_prefix, entry, "etc", "target")

            if shell_found and os.path.exists(etc_target):
                with open(etc_target, "r", encoding="utf-8") as f:
                    for line in f:
                        m = re.match(
                            r"^TARGET_MACHINE\s*=\s*(?P<machine>\S+)\s*$",
                            line
                        )
                        if not m:
                            continue
                        machine = m.group("machine")
                    #end for
                #end with
            #end if

            print("{} ({})".format(entry, machine))
        #end for
    #end function

    @classmethod
    def delete(cls, targets: list, **kwargs):
        for target_name in set(targets):
            cls._delete(target_name, **kwargs)

    @classmethod
    def _delete(cls, target_name, **kwargs):
        target_prefix = kwargs.get("target_prefix", Paths.target_prefix())

        target_dir = os.path.normpath(os.path.join(target_prefix, target_name))
        if not os.path.isdir(target_dir):
            raise BuildBoxError("target '{}' not found.".format(target_name))

        sysroot = Sysroot(target_dir)
        sysroot.terminate_processes()
        sysroot.umount_all()

        for subdir in ["dev", "proc", "sys"]:
            full_path = os.path.join(target_dir, subdir)

            if os.path.exists(full_path) and os.listdir(full_path):
                raise BuildBoxError(
                    "the '{}' subdirectory is not empty, aborting."
                    .format(subdir)
                )
            #end if
        #end for

        with open("/proc/mounts", "r", encoding="utf-8") as f:
            buf = f.read()

        for line in buf.splitlines():
            _, mountpoint, _, _, _, _ = line.strip().split()

            mountpoint = os.path.normpath(os.path.realpath(mountpoint))
            if mountpoint.startswith(target_dir + os.sep):
                raise BuildBoxError(
                    "there is something mounted at '{}', aborting."
                    .format(mountpoint)
                )
            #end if
        #end for

        old_sig_handler = signal.signal(signal.SIGINT, signal.SIG_IGN)
        shutil.rmtree(target_dir)
        signal.signal(signal.SIGINT, old_sig_handler)
    #end function

#end class
