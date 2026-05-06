#!/bin/bash
# cmake_sandbox.sh - Run CMake with child-process blocking (Linux + macOS)

set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"

# ====================== Defaults ======================
SRC="."
BDIR="/tmp/build_cmake_sandbox"
MODE="dylib"                    # dylib (dynamic library *.{dylib,so}) or sandbox
CGEN="Unix Makefiles"
CPARS="--fresh"
CEXE="-DCMAKE_EXECUTE_PROCESS_COMMAND_ERROR_IS_FATAL=ANY"

# ====================== Argument Parsing ======================
while [[ $# -gt 0 ]]; do
    case "$1" in
        sandbox|dylib)
            MODE="$1"
            shift
            ;;
        -G|--generator)
            CGEN="$2"
            shift 2
            ;;
        -c)
            CMAKEPATH="$2"
            shift 2
            ;;
        -S)
            SRC="$2"
            shift 2
            ;;
        -v|--verbose)
            CPARS="$CPARS --trace-expand"
            shift
            ;;
        -d|--debug)
            CPARS="$CPARS --debug-output"
            shift
            ;;
        -h|--help)
            echo "Usage: $0 [sandbox|dylib] [-G generator] [-S source_dir] [-c cmake_path] [-d|--debug] [-v|--verbose]"
            exit 0
            ;;
        -*)
            echo "Error: Unknown option '$1'" >&2
            exit 1
            ;;
        *)
            SRC="$1"
            shift
            ;;
    esac
done

# ====================== Platform Setup ======================
case "$OSTYPE" in
    darwin*)
        if [[ "$MODE" == "sandbox" ]]; then
            SND="sandbox-exec -f $SCRIPT_DIR/no-children.sb"
        else
            SND="env DYLD_INSERT_LIBRARIES=$SCRIPT_DIR/no-children.dylib DYLD_FORCE_FLAT_NAMESPACE=1"
        fi
        ;;
    linux*)
        SND="env LD_PRELOAD=$SCRIPT_DIR/no-children.so"
        ;;
    *)
        echo "Unsupported OS: $OSTYPE" >&2
        exit 1
        ;;
esac

# If not defined by -c, default to source-tree CMake, then fall back to PATH.
if [[ -z "${CMAKEPATH:-}" ]]; then
    CMAKEPATH="$SRC/build/bin/cmake"
    if [[ ! -x "$CMAKEPATH" ]]; then
        CMAKEPATH="$(which cmake)"
        if [[ -z "$CMAKEPATH" ]]; then
            echo "Error: cmake not found at '$SRC/build/bin/cmake' or in PATH. Please specify with -c." >&2
            exit 1
        fi
    fi
fi

# ====================== Execute ======================
cmd=( "$SND" "$CMAKEPATH" -B "$BDIR" -S "$SRC" -G "$CGEN" $CPARS "$CEXE" )

echo "Running: ${cmd[*]}"
"${cmd[@]}"
