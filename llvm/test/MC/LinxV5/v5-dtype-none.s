// PTO 0.58.1: code 31 is the canonical DTYPE_NONE inheritance sentinel.
// Work Package P0-5: token + numeric 31 must assemble; reserved codes
// The invalid out-of-range code 214 is rejected.
// RUN: sed '/B\.DATR 214/d' %s | llvm-mc --triple=linx64v5 --show-encoding | FileCheck %s
// RUN: not llvm-mc %s --triple=linx64v5 --show-encoding 2>&1 | FileCheck %s --check-prefix=NEG

// CHECK: B.DATR NORM.normal, Null
B.DATR dtype_none, RNONE, NOSAT
// CHECK: B.DATR NORM.normal, Null
B.DATR 31, RNONE, NOSAT
// CHECK: B.DATR FP32, byte0, Zero
B.DATR FP32, Zero, RNONE, NOSAT

// NEG: error: Match Instruction Error!
B.DATR 214, RNONE, NOSAT
