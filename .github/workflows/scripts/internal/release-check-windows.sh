#!/bin/bash
# Rely on cygwin or msys presence

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

FIND=/usr/bin/find
SIGNTOOL="/cygdrive/c/Program Files (x86)/Windows Kits/10/bin/x64/signtool.exe"

TARBALL="$1"
DIR="$2"

cd "$DIR"
unzip "$TARBALL"
ISPC="$($FIND . -name ispc.exe)"

TEST_NUMBER=0
TEST_NAME=""
echotest "run -v"
"$ISPC" -v
check_exit_code

echotest "run --help"
"$ISPC" --help
check_exit_code

echotest "run --support-matrix"
"$ISPC" --support-matrix
check_exit_code

echotest "compile dummy program"
echo "int foo() { return 1; }" | "$ISPC" --target=host --nostdlib --emit-llvm-text - -o -
check_exit_code

echotest "compile and run* Hello, Wolrd!"
echo 'extern "C" uniform int main() { print("Hello, World!\n"); return 0; }' | "$ISPC" --target=host --pic - -o hello_world.o
check_exit_code
if which cc; then
    cc hello_world.o -o hello_world && ./hello_world
    check_exit_code
else
    echo "[SKIP] No cc found"
fi

echotest "compile and run* simple foreach"
echo 'extern "C" uniform int main() { foreach (i = 0 ... 100) { print("%\n", i); } return 0; }' | "$ISPC" --target=host --pic - -o foreach.o
check_exit_code
if which cc; then
    cc foreach.o -o foreach && ./foreach
    check_exit_code
else
    echo "[SKIP] No cc found"
fi

BINARIES="$ISPC $($FIND -P . -type f -name "*.dll")"
for bin in $BINARIES; do
    bin_name=$(basename $bin)

    echotest "file format and arch for $bin_name"
    file "$bin"
    check_exit_code

    echotest "check no symbols for $bin_name"
    if nm "$bin" 2>&1 | grep "no symbols"; then
        echo "[OK]"
    else
        errmsg "[FAIL] Symbols are there."
        EXITCODE=1
    fi

    echotest "check no debug info for $bin_name"
    if objdump --section-headers "$bin" | grep "debug"; then
        errmsg "[FAIL] Debug infos are there."
        EXITCODE=1
    else
        echo "[OK]"
    fi

    echotest "list imported dlls for $bin_name"
    objdump -x "$bin" | grep "DLL Name"
    check_exit_code

    echotest "verify signature for $bin_name"
    "$SIGNTOOL" verify /pa /v "$bin"
    check_exit_code
done

exit $EXITCODE
