// RUN: not %{ispc} %s -o %t.o --nostdlib --target=host --nowrap --enable-specconst --specconst-map /dev/null 2>&1 | FileCheck %s

// CHECK: Specialization constants with varying types or aggregates are not yet supported.

struct MyAggregate {
    uniform int x;
    uniform float y;
};

specconst uniform MyAggregate A = {10, 3.14f};
