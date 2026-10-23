#!/usr/bin/env bash
# Builds the Mac emulator of the DUO's audio code: tools/emu/build/emu
set -euo pipefail
cd "$(dirname "$0")"
ROOT=../..
LIB=$ROOT/brains2/libraries/Teensy_Audio
mkdir -p build
FLAGS=(-O2 -w -D__ARM_ARCH_7EM__=1 -D__IMXRT1011__=1 -include shim/host_platform.h
       -Ishim -I$LIB -I$ROOT/brains2/core/custom_teensy_audio -I$ROOT)
SRCS=(emu.cpp
  $ROOT/brains2/core/custom_teensy_audio/AudioStream.cpp
  $ROOT/shared/duo/effect_custom_envelope.cpp
  $ROOT/shared/duo/seq.cpp
  $LIB/synth_waveform.cpp $LIB/filter_variable.cpp $LIB/mixer.cpp
  $LIB/effect_bitcrusher.cpp $LIB/effect_delay.cpp $LIB/effect_fade.cpp
  $LIB/effect_multiply.cpp $LIB/analyze_peak.cpp $LIB/synth_dc.cpp
  $LIB/synth_whitenoise.cpp $LIB/synth_pinknoise.cpp $LIB/synth_simple_drum.cpp
  $LIB/synth_sine.cpp)
CSRCS=($LIB/data_waveforms.c $LIB/data_bandlimit_step.c)
for c in "${CSRCS[@]}"; do clang -O2 -c "$c" -o "build/$(basename "$c" .c).o"; done
clang++ -std=c++17 "${FLAGS[@]}" "${SRCS[@]}" build/*.o -o build/emu
echo "built tools/emu/build/emu"
