#!/usr/bin/env bash

function print_help {
    echo "Usage: $0"
    echo "   or: $0 [--clean] [--directory Output]"
    echo "   or: $0 [-c] [-d Output]"
    echo "   or: $0 -h"

    echo "Options:"
    echo "    -h, --help        Print help message"
    echo "    -c, --clean       Clean target directories"
    echo "    -d, --debug       Debug build type"
    echo "    -o, --output      Set application directory"
}

function bad_exit {
    echo "[ERR] The build failed. Completing the script." >&2
    exit 42 
}

SCRIPT_HOME_DIR=$(pwd)
TARGET_DIR="Output"
BUILD_TYPE="Release"
CLEAN_FLAG=0

if [ -n "$1" ]; then
    case "$1" in
    -h | --help) print_help
    exit 0;;
    esac
fi

while [ -n "$1" ]; do
    case "$1" in
    -c | --clean) CLEAN_FLAG=1;;
    -d | --debug) BUILD_TYPE="Debug";;
    -o | --output) if [ -n "$2" ]
        then TARGET_DIR="$2/"
        else echo "[ERR] Application directory is not specified." >&2
        fi
        shift ;;
    --) shift
        break ;;
    *)  echo "$1 is not an option";;
    esac
shift
done

echo "Target directory is $TARGET_DIR" >&1

if [ "$CLEAN_FLAG" -eq 1 ]; then
    echo "Started cleaning"
    rm -r -f $TARGET_DIR
    echo "Finished"
fi

cmake . -B $TARGET_DIR -DCMAKE_BUILD_TYPE=$BUILD_TYPE -DCMAKE_CXX_COMPILER=g++ -G "Unix Makefiles"
if [ $? -ne 0 ]; then
    bad_exit
fi
cmake --build $TARGET_DIR

exit 0
