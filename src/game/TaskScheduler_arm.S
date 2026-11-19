// TaskScheduler_arm.S - Cooperative ARMv6 (A32) stack switching for Zeebo.
//
// ArmTaskContext layout (10 x uint32_t = 40 bytes):
//   [0..7] = r4..r11
//   [8]    = sp
//   [9]    = lr (resume PC)
    .syntax unified
    .arm
    .text

    .global TaskSwitch_ArmSwap
    .type   TaskSwitch_ArmSwap, %function
// void TaskSwitch_ArmSwap(ArmTaskContext* from, const ArmTaskContext* to);
// r0 = from, r1 = to
TaskSwitch_ArmSwap:
    stmia   r0, {r4-r11, sp, lr}
    ldmia   r1, {r4-r11, sp, lr}
    bx      lr
    .size   TaskSwitch_ArmSwap, . - TaskSwitch_ArmSwap
