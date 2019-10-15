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
import sys
import getopt
import textwrap

from org.boltlinux.buildbox.utils import switch
from org.boltlinux.buildbox.target import BBoxTarget
from org.boltlinux.buildbox.error import BBoxError

EXIT_OK = 0
EXIT_ERROR = 1

class BBoxCLI:

    def run(self, *args):
        command = args[0]

        for case in switch(command):
            if case("create"):
                self.create(*args[1:])
                break
            if case("list"):
                self.list(*args[1:])
                break
            if case("delete"):
                self.delete(*args[1:])
                break
            if case():
                raise BBoxError("invalid CLI command: {}".format(command))
        #end switch
    #end function

    def create(self, *args):
        def usage():
            print(textwrap.dedent(
                """
                Usage: build-box create [OPTIONS] <target-name> <spec>

                OPTIONS:

                 -h,--help           Print this help message and exit immediately.
                 -s,--suite <suite>  The name of the suite to bootstrap (default: stable)
                 -a,--arch <arch>    The architecture to bootstrap (default: x86_64)
                 -t,--targets <dir>  Create target below the given directory. The default
                                     target base directory is '~/.bolt/targets'.
                 --force             Overwrite any existing target with the same name.
                 --repo-base <url>   Repository base URL up to and including the "repo"
                                     folder.
                """
            ))
        #end inline function

        options = {
            "suite":
                "stable",
            "arch":
                "x86_64",
            "target_prefix":
                BBoxTarget.target_prefix(),
            "force":
                False,
            "repo_base":
                "http://archive.boltlinux.org/repo"
        }

        try:
            opts, args = getopt.getopt(
                args, "hs:a:t:", [
                    "suite=", "arch=", "targets=", "force", "repo-base="
                ]
            )
        except getopt.GetoptError as e:
            usage()
            sys.exit(EXIT_ERROR)

        for o, v in opts:
            for case in switch(o):
                if case("-s", "--suite"):
                    options["suite"] = v.strip()
                    break
                if case("-a", "--arch"):
                    options["arch"] = v.strip()
                    break
                if case("-t", "--targets"):
                    options["target_prefix"] = os.path.normpath(
                        os.path.abspath(v.strip())
                    )
                if case("--force"):
                    options["force"] = True
                    break
                if case("--repo-base"):
                    options["repo_base"] = v.strip()
                    break
            #end for
        #end for

        if len(args) != 2:
            usage()
            sys.exit(EXIT_ERROR)

        target_name = args[0]
        target_spec = args[1]
        BBoxTarget.create(target_name, target_spec, **options)
    #end function

    def list(self, *args):
        def usage():
            print(textwrap.dedent(
                """
                Usage: build-box list [OPTIONS]

                OPTIONS:

                 -h,--help           Print this help message and exit immediately.
                 -t,--targets <dir>  Search for targets in the given directory. The
                                     default location is '~/.bolt/targets'.
                """
            ))

        options = {
            "target_prefix":
                BBoxTarget.target_prefix()
        }

        try:
            opts, args = getopt.getopt(
                args, "ht:", ["targets="]
            )
        except getopt.GetoptError as e:
            usage()
            sys.exit(EXIT_ERROR)

        for o, v in opts:
            for case in switch(o):
                if case("-t", "--targets"):
                    options["target_prefix"] = os.path.normpath(
                        os.path.abspath(v.strip())
                    )
                    break
            #end for
        #end for

        if len(args) != 0:
            usage()
            sys.exit(EXIT_ERROR)

        BBoxTarget.list(**options)
    #end function

    def delete(self, *args):
        def usage():
            print(textwrap.dedent(
                """
                Usage: build-box delete [OPTIONS] <target-name>

                OPTIONS:

                 -h,--help           Print this help message and exit immediately.
                 -t,--targets <dir>  Look for target to delete below in the given directory.
                                     The default location is '~/.bolt/targets'.
                """
            ))

        options = {
            "target_prefix":
                BBoxTarget.target_prefix()
        }

        try:
            opts, args = getopt.getopt(
                args, "ht:", ["targets="]
            )
        except getopt.GetoptError as e:
            usage()
            sys.exit(EXIT_ERROR)

        for o, v in opts:
            for case in switch(o):
                if case("-t", "--targets"):
                    options["target_prefix"] = os.path.normpath(
                        os.path.abspath(v.strip())
                    )
                    break
            #end for
        #end for

        if len(args) != 1:
            usage()
            sys.exit(EXIT_ERROR)

        target_name = args[0]
        BBoxTarget.delete(target_name, **options)
    #end function

#end class
