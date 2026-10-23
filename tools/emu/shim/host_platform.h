// Mac emulator: force-included before every source file (-include).
//
// The audio code is compiled with __ARM_ARCH_7EM__ defined, so it takes the
// same code paths as on the DUO's Cortex-M7. The M7 DSP instructions those
// paths use (utility/dspinst.h) are replaced here by exact C equivalents,
// and interrupt / cycle-counter calls become no-ops: the emulator runs the
// audio update loop itself, one block at a time.
#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C++" {
#endif

// ---- utility/dspinst.h, with M7 semantics ---------------------------------
#define dspinst_h_  // the real header is skipped: these replace it

static inline int32_t emu_ssat(int64_t v, int bits) {
  const int64_t max = (1LL << (bits - 1)) - 1, min = -(1LL << (bits - 1));
  return (int32_t)(v > max ? max : (v < min ? min : v));
}
// SSAT with ASR: limit((val >> rshift), 2**bits)
#define signed_saturate_rshift(val, bits, rshift) emu_ssat(((int32_t)(val)) >> (rshift), (bits))

static inline int16_t saturate16(int32_t val) { return (int16_t)emu_ssat(val, 16); }

static inline int32_t lo16(uint32_t x) { return (int16_t)(x & 0xFFFF); }
static inline int32_t hi16(uint32_t x) { return (int16_t)(x >> 16); }

// SMULWB / SMULWT
static inline int32_t signed_multiply_32x16b(int32_t a, uint32_t b) {
  return (int32_t)(((int64_t)a * lo16(b)) >> 16);
}
static inline int32_t signed_multiply_32x16t(int32_t a, uint32_t b) {
  return (int32_t)(((int64_t)a * hi16(b)) >> 16);
}
// SMLAWB / SMLAWT
static inline int32_t signed_multiply_accumulate_32x16b(int32_t sum, int32_t a, uint32_t b) {
  return sum + signed_multiply_32x16b(a, b);
}
static inline int32_t signed_multiply_accumulate_32x16t(int32_t sum, int32_t a, uint32_t b) {
  return sum + signed_multiply_32x16t(a, b);
}
// SMMUL / SMMULR / SMMLAR / SMMLSR
static inline int32_t multiply_32x32_rshift32(int32_t a, int32_t b) {
  return (int32_t)(((int64_t)a * b) >> 32);
}
static inline int32_t multiply_32x32_rshift32_rounded(int32_t a, int32_t b) {
  return (int32_t)(((int64_t)a * b + 0x80000000LL) >> 32);
}
static inline int32_t multiply_accumulate_32x32_rshift32_rounded(int32_t sum, int32_t a, int32_t b) {
  return (int32_t)((((int64_t)sum << 32) + (int64_t)a * b + 0x80000000LL) >> 32);
}
static inline int32_t multiply_subtract_32x32_rshift32_rounded(int32_t sum, int32_t a, int32_t b) {
  return (int32_t)((((int64_t)sum << 32) - (int64_t)a * b + 0x80000000LL) >> 32);
}
// PKHTB / PKHBT
static inline uint32_t pack_16t_16t(int32_t a, int32_t b) {
  return ((uint32_t)a & 0xFFFF0000u) | (((uint32_t)(b >> 16)) & 0xFFFFu);
}
static inline uint32_t pack_16t_16b(int32_t a, int32_t b) {
  return ((uint32_t)a & 0xFFFF0000u) | ((uint32_t)b & 0xFFFFu);
}
static inline uint32_t pack_16b_16b(int32_t a, int32_t b) {
  return ((uint32_t)a << 16) | ((uint32_t)b & 0xFFFFu);
}
static inline uint32_t pack_16x16(int32_t a, int32_t b) { return pack_16b_16b(a, b); }
// QADD16 / QSUB16 / SHADD16 / SHSUB16
static inline uint32_t emu_pack(int32_t hi, int32_t lo) {
  return ((uint32_t)(uint16_t)hi << 16) | (uint16_t)lo;
}
static inline uint32_t signed_add_16_and_16(uint32_t a, uint32_t b) {
  return emu_pack(emu_ssat(hi16(a) + hi16(b), 16), emu_ssat(lo16(a) + lo16(b), 16));
}
static inline int32_t signed_subtract_16_and_16(int32_t a, int32_t b) {
  return (int32_t)emu_pack(emu_ssat(hi16(a) - hi16(b), 16), emu_ssat(lo16(a) - lo16(b), 16));
}
static inline int32_t signed_halving_add_16_and_16(int32_t a, int32_t b) {
  return (int32_t)emu_pack((hi16(a) + hi16(b)) >> 1, (lo16(a) + lo16(b)) >> 1);
}
static inline int32_t signed_halving_subtract_16_and_16(int32_t a, int32_t b) {
  return (int32_t)emu_pack((hi16(a) - hi16(b)) >> 1, (lo16(a) - lo16(b)) >> 1);
}
static inline uint32_t logical_and(uint32_t a, uint32_t b) { return a & b; }
// SMUAD / SMUADX / SMLALD / SMLALDX / SMULxy
static inline int32_t multiply_16tx16t_add_16bx16b(uint32_t a, uint32_t b) {
  return lo16(a) * lo16(b) + hi16(a) * hi16(b);
}
static inline int32_t multiply_16tx16b_add_16bx16t(uint32_t a, uint32_t b) {
  return lo16(a) * hi16(b) + hi16(a) * lo16(b);
}
static inline int64_t multiply_accumulate_16tx16t_add_16bx16b(int64_t sum, uint32_t a, uint32_t b) {
  return sum + (int64_t)lo16(a) * lo16(b) + (int64_t)hi16(a) * hi16(b);
}
static inline int64_t multiply_accumulate_16tx16b_add_16bx16t(int64_t sum, uint32_t a, uint32_t b) {
  return sum + (int64_t)lo16(a) * hi16(b) + (int64_t)hi16(a) * lo16(b);
}
static inline int32_t multiply_16bx16b(uint32_t a, uint32_t b) { return lo16(a) * lo16(b); }
static inline int32_t multiply_16bx16t(uint32_t a, uint32_t b) { return lo16(a) * hi16(b); }
static inline int32_t multiply_16tx16b(uint32_t a, uint32_t b) { return hi16(a) * lo16(b); }
static inline int32_t multiply_16tx16t(uint32_t a, uint32_t b) { return hi16(a) * hi16(b); }
// QSUB, with the CPU's sticky Q (saturation) flag
extern uint32_t emu_q_flag;
static inline int32_t substract_32_saturate(uint32_t a, uint32_t b) {
  const int64_t r = (int64_t)(int32_t)a - (int64_t)(int32_t)b;
  const int32_t s = emu_ssat(r, 32);
  if (s != r) emu_q_flag = 1;
  return s;
}
static inline uint32_t get_q_psr(void) { return emu_q_flag; }
static inline void clr_q_psr(void) { emu_q_flag = 0; }

#ifdef __cplusplus
}
#endif

// ---- Interrupts and cycle counter: no-ops ----------------------------------
#define __disable_irq() ((void)0)
#define __enable_irq() ((void)0)
#define NVIC_SetPendingIRQ(x) ((void)0)
#define NVIC_SetVector(x, y) ((void)0)
#define NVIC_SetPriority(x, y) ((void)0)
#define NVIC_EnableIRQ(x) ((void)0)
#define NVIC_DisableIRQ(x) ((void)0)
struct emu_dwt_t { uint32_t CYCCNT; };
extern struct emu_dwt_t emu_dwt;
#define DWT (&emu_dwt)
#define SystemCoreClock 500000000u
// AudioStream.cpp ends its update loop with asm("DSB") (a memory barrier).
#define asm(...) ((void)0)
// Puts a function in RAM on the DUO; ordinary code on the Mac.
#define RAMFUNCTION_SECTION_CODE(func) func
