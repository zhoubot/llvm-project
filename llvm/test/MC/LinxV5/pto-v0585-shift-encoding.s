# RUN: llvm-mc -triple=linx64v5 -show-encoding %s | FileCheck %s --check-prefix=ENC
# RUN: llvm-mc -triple=linx64v5 -filetype=obj %s -o %t
# RUN: llvm-objdump -d %t | FileCheck %s --check-prefix=DIS

sll a0, a1, ->a2
sllw a0, a1, ->a2
sra a0, a1, ->a2
sraw a0, a1, ->a2
srl a0, a1, ->a2
srlw a0, a1, ->a2

# ENC: sll{{[[:space:]]+}}a0, a1,{{[[:space:]]+}}->a2{{.*}}encoding: [0x05,0x72,0x31,0x00]
# ENC: sllw{{[[:space:]]+}}a0, a1,{{[[:space:]]+}}->a2{{.*}}encoding: [0x25,0x72,0x31,0x00]
# ENC: sra{{[[:space:]]+}}a0, a1,{{[[:space:]]+}}->a2{{.*}}encoding: [0x05,0x62,0x31,0x00]
# ENC: sraw{{[[:space:]]+}}a0, a1,{{[[:space:]]+}}->a2{{.*}}encoding: [0x25,0x62,0x31,0x00]
# ENC: srl{{[[:space:]]+}}a0, a1,{{[[:space:]]+}}->a2{{.*}}encoding: [0x05,0x52,0x31,0x00]
# ENC: srlw{{[[:space:]]+}}a0, a1,{{[[:space:]]+}}->a2{{.*}}encoding: [0x25,0x52,0x31,0x00]

# DIS: sll{{[[:space:]]+}}a0, a1,{{[[:space:]]+}}->a2
# DIS: sllw{{[[:space:]]+}}a0, a1,{{[[:space:]]+}}->a2
# DIS: sra{{[[:space:]]+}}a0, a1,{{[[:space:]]+}}->a2
# DIS: sraw{{[[:space:]]+}}a0, a1,{{[[:space:]]+}}->a2
# DIS: srl{{[[:space:]]+}}a0, a1,{{[[:space:]]+}}->a2
# DIS: srlw{{[[:space:]]+}}a0, a1,{{[[:space:]]+}}->a2
