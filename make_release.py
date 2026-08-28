#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Release automation script for updating the application version metadata and
building the project after the release values are adjusted.

Author: Anton Chernov
Date: 2026-02-28
"""
import sys
from os import chdir, rename, remove
from os.path import isdir, isfile
from platform import system
from datetime import date
import re
import subprocess

OS = system().lower()
SEPARATOR = "\\" if OS == "windows" else "/"
DEFAULT_VER = "1.0.0"
YEAR = date.today().strftime("%Y")

HTTPS = R"https://"
TAG = R"github.com/mdt-crm-internal/TTI_client/releases/tag/"

###############################################################################
# Utility wrapper for text files with newline normalization.


class File:
    def __init__(self, file_name, method):
        self.file_obj = open(file_name, method, newline="\n")

    def __enter__(self):
        return self.file_obj

    def __exit__(self, exc_type, exc_value, traceback):
        self.file_obj.close()

###############################################################################
# Main updater class.
# It validates the project layout, collects the files to update, and rewrites
# only the version metadata relevant to this repository.


class Updater:
    CMAKE_FILE = "CMakeLists.txt"
    MAIN_CPP_FILE = "app" + SEPARATOR + "main.cpp"
    README_FILE = "README.md"

    def __init__(self, args: tuple = ()) -> None:
        self.check_passed = False
        self.skip_readme = False
        self.skip_build = False
        self.lib_ver = DEFAULT_VER

        filtered_args = []
        for arg in args:
            if arg == "--skip-readme":
                self.skip_readme = True
            elif arg == "--skip-build":
                self.skip_build = True
            else:
                filtered_args.append(arg)

        if len(filtered_args) == 2:
            if isdir(filtered_args[1]):
                chdir(filtered_args[1]) # change the path to the solution
            else:
                print(
                    "Warning! The passed path does not exist.\n\t{}\n"
                    "\tThe current directory will be used instead."
                    .format(filtered_args[1])
                )

        if (
            len(filtered_args) > 0
            and re.match(r"\d+\.\d+\.\d+", filtered_args[0])
        ):
            self.lib_ver = filtered_args[0]

    # Step 1: check the runtime and the expected project files.
    def validate(self) -> bool:
        if sys.version_info < (3, 10):
            print("Python 3.10 or higher is required.")
            print(
                "You are using Python {}.{}."
                .format(sys.version_info.major, sys.version_info.minor))
            return False

        if not isfile(self.CMAKE_FILE):
            print("{} is not available".format(self.CMAKE_FILE))
            return False

        if not isfile(self.MAIN_CPP_FILE):
            print("{} is not available".format(self.MAIN_CPP_FILE))
            return False

        self.check_passed = True
        return True

    def isValid(self) -> bool:
        return self.check_passed

    # Step 2: collect the files that need version metadata updates.
    def findLibFiles(self) -> tuple:
        flist = [self.CMAKE_FILE, self.MAIN_CPP_FILE]
        if not self.skip_readme and isfile(self.README_FILE):
            flist.append(self.README_FILE)
        return tuple(flist)

    @staticmethod
    def reserveFiles(ftuple: tuple) -> None:
        import shutil
        for path in ftuple:
            pos = path.rfind('.')
            if pos == -1:
                continue
            shutil.copyfile(path, path[:pos] + "_tmp" + path[pos:])

    @staticmethod
    def clean_up(returncode: int, ftuple: tuple) -> None:
        if returncode:
            print("Library build completed with error #{}".format(returncode))
        else:
            print("Library build completed successfully.")

        for path in ftuple:
            pos = path.rfind('.')
            if pos == -1:
                continue
            reserved_path = path[:pos] + "_tmp" + path[pos:]
            if returncode:
                if isfile(reserved_path):
                    if isfile(path):
                        remove(path)
                    rename(reserved_path, path)
                else:
                    print(
                        "The reserved {} file does not exist."
                        .format(reserved_path)
                    )
                    print(
                        "You need to deal with the {} file manually."
                        .format(path)
                    )
            else:
                if isfile(reserved_path):
                    remove(reserved_path)
                else:
                    print("The {} file does not exist".format(reserved_path))

    # Step 3: rewrite the version strings in the project files.
    def updateFiles(self, ftuple: tuple = ()) -> None:
        HEADER = r"(?m)^(\s*\*\s+@version\s+)\d+\.\d+\.\d+"
        CMAKE_VER = r"project\s*\([^\n]*VERSION\s+\d+\.\d+\.\d+"
        MAJOR_VER = r"(?m)^(#define\s+VERSION_MAJOR\s+)\d+"
        MINOR_VER = r"(?m)^(#define\s+VERSION_MINOR\s+)\d+"
        PATCH_VER = r"(?m)^(#define\s+VERSION_PATCH\s+)\d+"
        FILE_VER = (
            r'(?m)^(\s+"FileVersion:\s*)\d+\.\d+\.\d+\.\d+(\\n")'
        )
        PROD_VER = (
            r'(?m)^(\s+"ProductVersion:\s*)\d+\.\d+\.\d+\.\d+(\\n")'
        )
        COPYRIGHT = (
            r'(?m)^(\s+"LegalCopyright:\sCopyright \(C\)\s)[^\n]*(\\n")'
        )
        MD_LIB_VER = (
            r"version\s+of\s+the\s+application\s+is\s+\d+\.\d+\.\d+"
        )

        VER = self.lib_ver.split('.')
        for file in ftuple:
            content = ""
            try:
                with File(file, "r") as opened_file:
                    content = opened_file.read()
            except OSError:
                print("{} is not available".format(file))

            if file == self.CMAKE_FILE:
                content = re.sub(
                    CMAKE_VER,
                    "project(Oscilloscope VERSION {}".format(self.lib_ver),
                    content,
                    count=1,
                )
            elif file == self.MAIN_CPP_FILE:
                content = re.sub(
                    HEADER,
                    rf"\g<1>{self.lib_ver}",
                    content,
                    count=1,
                )
                major, minor, patch = VER
                content = re.sub(MAJOR_VER, rf"\g<1>{major}", content, count=1)
                content = re.sub(MINOR_VER, rf"\g<1>{minor}", content, count=1)
                content = re.sub(PATCH_VER, rf"\g<1>{patch}", content, count=1)
                content = re.sub(
                    FILE_VER,
                    rf"\g<1>{major}.{minor}.{patch}.0\g<2>",
                    content,
                    count=1,
                )
                content = re.sub(
                    PROD_VER,
                    rf"\g<1>{major}.{minor}.{patch}.0\g<2>",
                    content,
                    count=1,
                )
                content = re.sub(
                    COPYRIGHT,
                    rf"\g<1>Anton Chernov, {YEAR}\g<2>",
                    content,
                    count=1,
                )
            elif file == self.README_FILE:
                if self.skip_readme:
                    continue
                content = re.sub(
                    MD_LIB_VER,
                    "version of the application is {}.{}.{}"
                    .format(VER[0], VER[1], VER[2]),
                    content,
                    count=1,
                )

            try:
                with File(file, "w") as opened_file:
                    opened_file.write(content)
            except OSError:
                print("{} is not available".format(file))

###############################################################################

# Build the project after the version metadata has been updated.


def build_prj() -> int:
    import subprocess

    TIMEOUT = 90  # in seconds
    print("OS defined as: {}".format(OS))

    if OS == "windows":
        command = "win_build.bat -c -a"
    elif OS == "linux":
        command = "./linux_build.sh -c"
    else:
        print("Unknown OS")
        return -2

    return subprocess.run(command, shell=True, timeout=TIMEOUT).returncode

###############################################################################


def main(args: tuple) -> int:
    # Step 1: Create an instance of the configuration class and check for
    # the presence of the files required for operation, as well as check
    # the passed command line arguments.
    err_level = -1
    upd = Updater(args)

    if upd.validate():
        # Stage 2. Create a tuple of files for future processing.
        file_tuple = upd.findLibFiles()
        # Stage 3. Create backup copies of source files.
        upd.reserveFiles(file_tuple)
        # Stage 4. Update files specified in the tuple.
        upd.updateFiles(file_tuple)

        # Stage 5. Checking the functionality of the changes with the build.
        if upd.skip_build:
            print("Skipping build step (--skip-build).")
            err_level = 0
        else:
            err_level = build_prj()

        # Stage 6. Removing spare files.
        upd.clean_up(err_level, file_tuple)
    return err_level

###############################################################################


if __name__ == "__main__":
    user_args = tuple(sys.argv[1:])
    exit(main(user_args))
