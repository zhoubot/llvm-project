; RUN: llc < %s --march=linx64v5 -O2 | FileCheck %s --dump-input always -vv --check-prefix=CHECK

declare double @llvm.sqrt.f64(double) nounwind readnone
declare float @llvm.sqrt.f32(float) nounwind readnone
declare half @llvm.sqrt.f16(half) nounwind readnone

; CHECK-LABEL: fsqrt.fd:
; CHECK: fsqrt.fd
; CHECK-NOT: CALL
define double @fsqrt.fd(double %a) {
  %sqrt = call double @llvm.sqrt.f64(double %a)
  ret double %sqrt
}

; CHECK-LABEL: fsqrt.fs:
; CHECK: fsqrt.fs
; CHECK-NOT: CALL
define float @fsqrt.fs(float %a) {
  %sqrt = call float @llvm.sqrt.f32(float %a)
  ret float %sqrt
}

; CHECK-LABEL: fsqrt.fh:
; CHECK: fsqrt.fh
; CHECK-NOT: CALL
define half @fsqrt.fh(half %a) {
  %sqrt = call half @llvm.sqrt.f16(half %a)
  ret half %sqrt
}
