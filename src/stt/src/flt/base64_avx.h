// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026, The OpenROAD Authors
//
// The 32-byte shuffle decoder follows the BSD implementations by
// Wojciech Mula, Alfred Klomp, and Daniel Lemire (fastbase64).

#pragma once

#include <cstddef>
#include <cstdint>

#if defined(__x86_64__)
#include <cpuid.h>
#include <immintrin.h>
#define STT_BASE64_AVX2 1
#endif

namespace stt::flt {

#if defined(__GNUC__)
__attribute__((target("xsave")))
#endif
inline bool base64Avx2()
{
#if defined(STT_BASE64_AVX2)
  static int ready = -1;
  if (ready >= 0) {
    return ready != 0;
  }
  ready = 0;
  unsigned int eax = 0;
  unsigned int ebx = 0;
  unsigned int ecx = 0;
  unsigned int edx = 0;
  if (__get_cpuid(1, &eax, &ebx, &ecx, &edx) != 0
      && (ecx & bit_OSXSAVE) != 0 && (ecx & bit_AVX) != 0) {
    const unsigned int xcr = static_cast<unsigned int>(_xgetbv(0));
    if ((xcr & 6U) == 6U
        && __get_cpuid_count(7, 0, &eax, &ebx, &ecx, &edx) != 0
        && (ebx & bit_AVX2) != 0) {
      ready = 1;
    }
  }
  return ready != 0;
#else
  return false;
#endif
}

// Decode complete 32-character blocks. Stops before padding or an
// invalid character. Returns input bytes consumed.
#if defined(__GNUC__)
__attribute__((target("avx2")))
#endif
inline std::size_t base64DecodeAvx2(char* out,
                                    const char* src,
                                    std::size_t srclen)
{
#if defined(STT_BASE64_AVX2)
  if (!base64Avx2()) {
    return 0;
  }
  std::size_t consumed = 0;
  while (srclen >= 32) {
    const __m256i str = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(src));
    const __m256i lut_lo = _mm256_setr_epi8(
        0x15, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x13, 0x1A,
        0x1B, 0x1B, 0x1B, 0x1A, 0x15, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11,
        0x11, 0x11, 0x13, 0x1A, 0x1B, 0x1B, 0x1B, 0x1A);
    const __m256i lut_hi = _mm256_setr_epi8(
        0x10, 0x10, 0x01, 0x02, 0x04, 0x08, 0x04, 0x08, 0x10, 0x10, 0x10, 0x10,
        0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x01, 0x02, 0x04, 0x08, 0x04, 0x08,
        0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10);
    const __m256i lut_roll = _mm256_setr_epi8(
        0, 16, 19, 4, -65, -65, -71, -71, 0, 0, 0, 0, 0, 0, 0, 0, 0, 16, 19, 4,
        -65, -65, -71, -71, 0, 0, 0, 0, 0, 0, 0, 0);
    const __m256i mask_2F = _mm256_set1_epi8(0x2f);

    __m256i hi_nibbles = _mm256_srli_epi32(str, 4);
    const __m256i lo_nibbles = _mm256_and_si256(str, mask_2F);
    const __m256i lo = _mm256_shuffle_epi8(lut_lo, lo_nibbles);
    const __m256i eq_2F = _mm256_cmpeq_epi8(str, mask_2F);
    hi_nibbles = _mm256_and_si256(hi_nibbles, mask_2F);
    const __m256i hi = _mm256_shuffle_epi8(lut_hi, hi_nibbles);
    const __m256i roll
        = _mm256_shuffle_epi8(lut_roll, _mm256_add_epi8(eq_2F, hi_nibbles));
    if (_mm256_testz_si256(lo, hi) == 0) {
      break;
    }

    __m256i values = _mm256_add_epi8(str, roll);
    const __m256i merge
        = _mm256_maddubs_epi16(values, _mm256_set1_epi32(0x01400140));
    __m256i packed = _mm256_madd_epi16(merge, _mm256_set1_epi32(0x00011000));
    packed = _mm256_shuffle_epi8(
        packed,
        _mm256_setr_epi8(2, 1, 0, 6, 5, 4, 10, 9, 8, 14, 13, 12, -1, -1, -1,
                         -1, 2, 1, 0, 6, 5, 4, 10, 9, 8, 14, 13, 12, -1, -1, -1,
                         -1));
    packed = _mm256_permutevar8x32_epi32(
        packed, _mm256_setr_epi32(0, 1, 2, 4, 5, 6, -1, -1));

    const __m128i low = _mm256_castsi256_si128(packed);
    const __m128i high = _mm256_extracti128_si256(packed, 1);
    _mm_storeu_si128(reinterpret_cast<__m128i*>(out), low);
    _mm_storel_epi64(reinterpret_cast<__m128i*>(out + 16), high);

    src += 32;
    out += 24;
    srclen -= 32;
    consumed += 32;
  }
  return consumed;
#else
  (void) out;
  (void) src;
  (void) srclen;
  return 0;
#endif
}

}  // namespace stt::flt
