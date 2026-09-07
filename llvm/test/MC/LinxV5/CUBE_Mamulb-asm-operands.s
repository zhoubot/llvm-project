// RUN: llvm-mc %s --triple=linx64v5 --show-encoding -linxv5-enable-compress-inst=false| FileCheck %s --dump-input always -vv

// imm <Size>
// CHECK: BSTART.CUBE      TMATMULMX, FP32
// CHECK: B.DATR           FP16, byte0, Zero
// CHECK: B.DIM            zero, 32,        ->lb0
// CHECK: B.DIM            zero, 24,        ->lb1
// CHECK: B.DIM            zero, 16,        ->lb2
// CHECK: B.IOT t#1, t#2, mask=1111
// CHECK: B.IOT t#3, t#4, mask=1111, last, ->u<64KB>
TMATMULMX <M:32, N:24, K:16, FP32, FP16> T#1, T#2, T#3, T#4, ->U<64KB>

// imm <Size>, same datatype
// CHECK: BSTART.CUBE      TMATMULMX, FP16
// CHECK-NOT: B.ATTR
// CHECK: B.DIM            zero, 32,        ->lb0
// CHECK: B.DIM            zero, 24,        ->lb1
// CHECK: B.DIM            zero, 16,        ->lb2
// CHECK: B.IOT t#1, t#3, mask=1111
// CHECK: B.IOT t#4, mask=1111, last, ->u<64KB>
TMATMULMX <M:32, N:24, K:16, FP16, FP16> T#1, T#3, T#4, ->U<64KB>

// imm <Size>
// CHECK: BSTART.CUBE      TMATMULMX.BIAS, FP32
// CHECK: B.DATR           FP16, byte0, Zero
// CHECK: B.DIM            zero, 32,        ->lb0
// CHECK: B.DIM            zero, 24,        ->lb1
// CHECK: B.DIM            zero, 16,        ->lb2
// CHECK: B.IOT t#1, t#2, mask=1111
// CHECK: B.IOT t#3, t#4, mask=1111
// CHECK: B.IOT u#1, mask=1111, last, ->u<64KB>
TMATMULMX.BIAS <M:32, N:24, K:16, FP32, FP16> T#1, T#2, T#3, T#4, U#1 ->U<64KB>

// imm <Size>, ACC source
// CHECK: BSTART.CUBE      TMATMULMX.ACC, FP32
// CHECK-NOT: B.DATR
// CHECK: B.DIM            zero, 32,        ->lb0
// CHECK: B.DIM            zero, 24,        ->lb1
// CHECK: B.DIM            zero, 16,        ->lb2
// CHECK: B.IOT t#1, t#2, mask=1111
// CHECK: B.IOT t#3, t#4, mask=1111
// CHECK: B.IOT u#1, mask=1111, last, ->u<64KB>
TMATMULMX.ACC <M:32, N:24, K:16, FP32, FP32> T#1, T#2, T#3, T#4, U#1 ->U<64KB>

// imm <Size>
// CHECK: BSTART.CUBE      TMATMULMX.BIAS, FP32
// CHECK: B.DATR           FP16, byte0, Zero
// CHECK: B.DIM            zero, 32,        ->lb0
// CHECK: B.DIM            zero, 24,        ->lb1
// CHECK: B.DIM            zero, 16,        ->lb2
// CHECK: B.IOT t#1, t#3, mask=1111
// CHECK: B.IOT t#4, u#1, mask=1111, last, ->u<64KB>
TMATMULMX.BIAS <M:32, N:24, K:16, FP32, FP16> T#1, T#3, T#4, U#1 ->U<64KB>

// imm <Size>, ACC source
// CHECK: BSTART.CUBE      TMATMULMX.ACC, FP32
// CHECK: B.DATR           FP16, byte0, Zero
// CHECK: B.DIM            zero, 32,        ->lb0
// CHECK: B.DIM            zero, 24,        ->lb1
// CHECK: B.DIM            zero, 16,        ->lb2
// CHECK: B.IOT t#1, t#3, mask=1111
// CHECK: B.IOT t#4, u#1, mask=1111, last, ->u<64KB>
TMATMULMX.ACC <M:32, N:24, K:16, FP32, FP16> T#1, T#3, T#4, U#1 ->U<64KB>

// Reg+imm <Size>
// CHECK: BSTART.CUBE      TMATMULMX, FP32
// CHECK: B.DATR           FP16, byte0, Zero
// CHECK: B.DIM            a0, 32,          ->lb0
// CHECK: B.DIM            zero, 24,        ->lb1
// CHECK: B.DIM            zero, 16,        ->lb2
// CHECK: B.IOT t#1, t#2, mask=1111
// CHECK: B.IOT t#3, t#4, mask=1111, last, ->u<64KB>
TMATMULMX <M:R2+32, N:24, K:16, FP32, FP16> T#1, T#2, T#3, T#4, ->U<64KB>

// imm <Size>
// CHECK: BSTART.CUBE      TMATMULMX, FP32
// CHECK: B.DATR           FP16, byte0, Zero
// CHECK: B.DIM            zero, 32,        ->lb0
// CHECK: B.DIM            zero, 24,        ->lb1
// CHECK: B.DIM            zero, 16,        ->lb2
// CHECK: B.IOT t#1, t#2, mask=1111
// CHECK: B.IOT t#3, t#4, mask=1111, last, ->u<64KB>
TMATMULMX <M:32, N:24, K:16, FP32, FP16> T#1, T#2, T#3, T#4, ->U<64KB>

// imm <Size>
// CHECK-NOT: BSTART.CUBE      TCVT, FP32
// CHECK-NOT: B.ATTR   ZN2ZZ, e5m2
// CHECK-NOT: B.DIM   zero, 32,       ->lb0
// CHECK-NOT: B.DIM   zero, 24,       ->lb1
// CHECK-NOT: B.IOTI  [t#1], last,  ->t<64KB>
//TCVT e5m2, zn2zz, <Row:32, Col:24, FP32> T#1, ->t<64KB>

//Reg+imm <Size>
// CHECK-NOT: BSTART.CUBE      TCVT, FP32
// CHECK-NOT: B.ATTR   ZN2ZZ, e5m2
// CHECK-NOT: B.DIM   a0, 32,         ->lb0
// CHECK-NOT: B.DIM   zero, 24,       ->lb1
// CHECK-NOT: B.IOTI  [t#1], last,  ->t<64KB>
//TCVT e5m2, zn2zz, <Row:R2+32, Col:24, FP32> T#1, ->t<64KB>
