#!/bin/bash
# cmake_sandbox.sh - Run CMake with child-process blocking

set -euo pipefail

# Default values
SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
SRC="."
SBC="$SCRIPT_DIR/no-children.sb"
CMAKEPATH=$SRC/build/bin/cmake
BDIR="/tmp/build_cmake_sandbox"
MODE="dylib"
# default to dylib (safer on macOS)
NICE="nice -n 20"
CVARS="-DCMAKE_C_COMPILER_WORKS=yes -DCMAKE_CXX_COMPILER_WORKS=yes -DCMAKE_C_COMPILER=$(which clang) -DCMAKE_CXX_COMPILER=$(which clang++)"
CVARS=$CVARS" -DCMake_HAVE_CXX_UNIQUE_PTR=yes -DCMAKE_USE_SYSTEM_LIBARCHIVE=on -DCMAKE_HAVE_LIBC_PTHREAD=on"
CROOT="-DCMAKE_PREFIX_PATH=/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk/usr"
CLROOT="-DLibArchive_ROOT=/opt/homebrew/opt/libarchive"
CPARS="--fresh"
CEXE="-DCMAKE_EXECUTE_PROCESS_COMMAND_ERROR_IS_FATAL=ANY"
CGEN="Unix Makefiles"

# Parse arguments
while [[ $# -gt 0 ]]; do
    case "$1" in
        sandbox|dylib)
            MODE="$1"
            shift
            ;;
        -G|--generator)
            if [[ $# -lt 2 ]]; then
                echo "Error: $1 requires a generator name"
                echo "Usage: $0 [sandbox|dylib] [-G generator] [-S source_directory]"
                exit 1
            fi
            CGEN="$2"
            shift 2
            ;;
        -h|--help)
            echo "Usage: $0 [sandbox|dylib] [-G generator] [-S source_directory]"
            exit 0
            ;;
        -S)
            if [[ $# -lt 2 ]]; then
                echo "Error: $1 requires a source directory"
                echo "Usage: $0 [sandbox|dylib] [-G generator] [-S source_directory]"
                exit 1
            fi
            SRC="$2"
            shift 2
            ;;
        -v|--verbose)
            CPARS=$CPARS" --trace-expand --debug-output"
            shift
            ;;
        -*)
            echo "Error: Unknown option '$1'"
            echo "Usage: $0 [sandbox|dylib] [-G generator] [-S source_directory]"
            exit 1
            ;;
        *)
            if [[ "$SRC" != "." ]]; then
                echo "Error: Unexpected extra argument '$1'"
                echo "Usage: $0 [sandbox|dylib] [-G generator] [-S source_directory]"
                exit 1
            fi
            SRC="$1"
            shift
            ;;
    esac
done

# Choose sandbox method
if [[ "$MODE" == "sandbox" ]]; then
    SND="sandbox-exec -f $SBC"
    echo "Using sandbox-exec"
else
    SND="env DYLD_INSERT_LIBRARIES=$SCRIPT_DIR/no-children.dylib DYLD_FORCE_FLAT_NAMESPACE=1"
    echo "Using DYLD interposer"
fi

# Build command using array (safest way)
cmd=( $NICE $SND "$CMAKEPATH" -B "$BDIR" -S "$SRC" -G "$CGEN" "$CROOT" "$CLROOT" $CVARS $CPARS $CEXE )

echo "Running: ${cmd[*]}"
"${cmd[@]}"
