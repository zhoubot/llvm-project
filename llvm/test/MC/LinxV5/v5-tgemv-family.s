// RUN: llvm-mc -triple=linx64v5 -show-encoding %s | FileCheck %s
// RUN: llvm-mc -triple=linx64v5 -filetype=obj %s | llvm-objdump -d - | FileCheck %s --check-prefix=DIS

// CHECK: BSTART.CUBE TGEMV, FP32
// CHECK-NEXT: B.FPATR 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
// CHECK: B.IOT t#1, t#2, mask=1111, last,{{.*}}->t<4KB>
// DIS: BSTART.CUBE TGEMV, FP32
TGEMV <M:1, N:32, K:64, FP32, FP32> T#1, T#2, ->T<4KB>

// CHECK: BSTART.CUBE TGEMV.BIAS, FP32
// CHECK-NEXT: B.FPATR 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
// CHECK: B.IOT u#1, mask=1111, last
// DIS: BSTART.CUBE TGEMV.BIAS, FP32
TGEMV.BIAS <M:1, N:32, K:64, FP32, FP32> T#1, T#2, U#1, ->T<4KB>

// CHECK: BSTART.CUBE TGEMV.ACC, FP32
// CHECK-NEXT: B.FPATR 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
// CHECK: B.IOT t#2, mask=1111, last
// DIS: BSTART.CUBE TGEMV.ACC, FP32
TGEMV.ACC <M:1, N:32, K:64, FP32, FP32> T#3, T#1, T#2, ->T<4KB>

// CHECK: BSTART.CUBE TGEMVMX, FP32
// CHECK-NEXT: B.DATR e4m3, byte0, Zero
// CHECK-NEXT: B.FPATR 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
// DIS: BSTART.CUBE TGEMVMX, FP32
TGEMVMX <M:1, N:32, K:64, FP32, E4M3> T#1, T#2, T#3, T#4, ->T<4KB>

// CHECK: BSTART.CUBE TGEMVMX.BIAS, FP32
// CHECK-NEXT: B.DATR e4m3, byte0, Zero
// CHECK-NEXT: B.FPATR 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
// CHECK: B.IOT u#1, mask=1111, last,{{.*}}->t<4KB>
// DIS: BSTART.CUBE TGEMVMX.BIAS, FP32
TGEMVMX.BIAS <M:1, N:32, K:64, FP32, E4M3> T#1, T#2, T#3, T#4, U#1, ->T<4KB>

// CHECK: BSTART.CUBE TGEMVMX.ACC, FP32
// CHECK-NEXT: B.DATR e4m3, byte0, Zero
// CHECK-NEXT: B.FPATR 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
// CHECK: B.IOT t#4, mask=1111, last,{{.*}}->t<4KB>
// DIS: BSTART.CUBE TGEMVMX.ACC, FP32
TGEMVMX.ACC <M:1, N:32, K:64, FP32, E4M3> T#5, T#1, T#2, T#3, T#4, ->T<4KB>
