#!/bin/bash

EXITCODE=0

function errmsg () {
    msg="$1"
    echo "$msg"
    echo "$msg" >> $GITHUB_STEP_SUMMARY
}

function echotest () {
    echo
    echo "## Test $TEST_NUMBER ($1): "
    TEST_NAME="$1"
    TEST_NUMBER=$((TEST_NUMBER+1))
}

function check_exit_code () {
    ret=$?
    current_test_number=$((TEST_NUMBER-1))
    if [ $ret -ne 0 ]; then
        errmsg "Test #$current_test_number ($TEST_NAME) returned non zero exit code"
        EXITCODE=1
    fi
}

TARBALL="$1"
DIR="$2"
ARCHS="$3 $4"

cd "$DIR"
tar xf "$TARBALL"
ISPC="$(find . -name ispc)"

TEST_NUMBER=0
TEST_NAME=""
echotest "run -v"
"$ISPC" -v
check_exit_code

echotest "run --help"
for arch in $ARCHS; do
    arch -$arch "$ISPC" --help
    check_exit_code
done

echotest "run --support-matrix"
for arch in $ARCHS; do
    arch -$arch "$ISPC" --support-matrix
    check_exit_code
done

echotest "compile dummy program"
for arch in $ARCHS; do
    echo "int foo() { return 1; }" | arch -$arch "$ISPC" --target=host --nostdlib --emit-llvm-text - -o -
    check_exit_code
done

echotest "compile and run* Hello, Wolrd!"
for arch in $ARCHS; do
    echo 'extern "C" uniform int main() { print("Hello, World!\n"); return 0; }' | arch -$arch "$ISPC" --target=host --pic - -o hello_world.o
    check_exit_code
    if which cc; then
        arch -$arch cc hello_world.o -o hello_world && ./hello_world
        check_exit_code
    else
        echo "[SKIP] No cc found"
    fi
done

echotest "compile and run* simple foreach"
for arch in $ARCHS; do
    echo 'extern "C" uniform int main() { foreach (i = 0 ... 100) { print("%\n", i); } return 0; }' | arch -$arch "$ISPC" --target=host --pic - -o foreach.o
    check_exit_code
    if which cc; then
        arch -$arch cc foreach.o -o foreach && ./foreach
        check_exit_code
    else
        echo "[SKIP] No cc found"
    fi
done

BINARIES="$ISPC $(find -P . -type f -name "lib*dylib")"
for bin in $BINARIES; do
    bin_name=$(basename $bin)

    echotest "file format and arch for $bin_name"
    for arch in $ARCHS; do
        file "$bin" | grep "Mach-O.*$arch"
        check_exit_code
    done

    echotest "check signature"
    codesign -vv "$bin"
    check_exit_code

    # TODO!
    # echotest "check no symbols for $bin_name"
    # if readelf -SW "$bin" | grep "symtab"; then
    #     errmsg "[FAIL] Symbols are there."
    #     EXITCODE=1
    # else
    #     echo "[OK]"
    # fi

    echotest "check no debug info for $bin_name"
    for arch in $ARCHS; do
        if dsymutil --statistics "$bin" 2>&1 | grep "no debug" | grep $arch; then
            echo "[OK]"
        else
            errmsg "[FAIL] Debug infos are there."
            EXITCODE=1
        fi
    done

    echotest "print shared libraries used by $bin_name"
    otool -L "$bin"
    check_exit_code

    echotest "check min macOS version of $bin_name"
    otool -l "$bin" | grep -E -A4 '(LC_VERSION_MIN_MACOSX|LC_BUILD_VERSION)'
    check_exit_code
done

exit $EXITCODE
