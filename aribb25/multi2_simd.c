#include <stdlib.h>
#include <string.h>
#ifdef _MSC_VER
#include <intrin.h>
#else
#include <x86intrin.h>
#include <cpuid.h>
#endif
#include <emmintrin.h>

#ifdef _WIN32
#include <windows.h>
#include <VersionHelpers.h>
#endif

#include "multi2_simd.h"
#include "multi2_error_code.h"

#ifdef ENABLE_MULTI2_SSSE3
#include <tmmintrin.h>
#endif
#ifdef ENABLE_MULTI2_AVX2
#include <immintrin.h>
#endif

// optimization for pipeline
#define OPTIMIZE_MULTI2_FOR_PIPELINE

#if defined(USE_MULTI2_INTRINSIC) && defined(_MSC_VER)
#pragma intrinsic(_byteswap_ulong, _byteswap_uint64, _lrotl)
#endif

#define MM_SHUFFLE4(a, b, c, d) (((a) << 6) | ((b) << 4) | ((c) << 2) | (d))

//#define IMMEDIATE1 Immediate1
#define IMMEDIATE1 _mm_set1_epi32(1)
//static __m128i Immediate1;
#define IMMEDIATE1_M256 _mm256_set1_epi32(1)

#ifdef ENABLE_MULTI2_SSSE3
static __m128i byte_swap_mask;
static __m128i src_swap_mask;
static __m128i rotation_16_mask;
static __m128i rotation_8_mask;
#endif

#ifdef ENABLE_MULTI2_AVX2
static __m256i byte_swap_mask_avx2;
static __m256i src_swap_mask_avx2;
static __m256i rotation_16_mask_avx2;
static __m256i rotation_8_mask_avx2;
#endif

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 inner variables
 ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
#define MULTI2_SIMD_SCRAMBLE_ROUND 4
static uint32_t scramble_round = MULTI2_SIMD_SCRAMBLE_ROUND;
#define MAX_SCRAMBLE_ROUND MULTI2_SIMD_SCRAMBLE_ROUND
//#define MAX_SCRAMBLE_ROUND scramble_round
static enum INSTRUCTION_TYPE simd_instruction = INSTRUCTION_NORMAL;
static bool is_mask_initialized = false;

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 function prototypes (private method)
 ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
static inline const uint32_t left_rotate_uint32_for_simd(const uint32_t value, const uint32_t rotate);
static inline __m128i left_rotate_m128i(const __m128i *value, const int rotate);
static inline void round_pi1(uint32_t *left, uint32_t *right);
static inline void round_pi2(uint32_t *left, uint32_t *right, const uint32_t k1);
static inline void round_pi3(uint32_t *left, uint32_t *right, const uint32_t k2, const uint32_t k3);
static inline void round_pi4(uint32_t *left, uint32_t *right, const uint32_t k4);
static inline __m128i byte_swap_sse2(const __m128i *Value);
static inline void round_pi1_sse2(__m128i *left, __m128i *right);
static inline void round_pi2_sse2(__m128i *left, __m128i *right, const __m128i *key1);
static inline void round_pi3_sse2(__m128i *left, __m128i *right, const __m128i *key2, const __m128i *key3);
static inline void round_pi4_sse2(__m128i *left, __m128i *right, const __m128i *key4);

#ifdef OPTIMIZE_MULTI2_FOR_PIPELINE
static inline void round_pi1_sse2_with_3sets(__m128i *left1, __m128i *right1,
													__m128i *left2, __m128i *right2,
													__m128i *left3, __m128i *right3);
static inline void round_pi2_sse2_with_3sets(__m128i *left1, __m128i *right1,
													__m128i *left2, __m128i *right2,
													__m128i *left3, __m128i *right3,
													const __m128i *key1);
static inline void round_pi3_sse2_with_3sets(__m128i *left1, __m128i *right1,
													__m128i *left2, __m128i *right2,
													__m128i *left3, __m128i *right3,
													const __m128i *key2, const __m128i *key3);
static inline void round_pi4_sse2_with_3sets(__m128i *left1, __m128i *right1,
													__m128i *left2, __m128i *right2,
													__m128i *left3, __m128i *right3,
													const __m128i *key4);
#endif	// OPTIMIZE_MULTI2_FOR_PIPELINE

#ifdef ENABLE_MULTI2_SSSE3
static inline __m128i byte_swap_ssse3(const __m128i *value);
static inline void round_pi3_ssse3(__m128i *left, __m128i *right,
										  const __m128i *key2, const __m128i *key3);
#ifdef OPTIMIZE_MULTI2_FOR_PIPELINE
static inline void round_pi3_ssse3_with_3sets(__m128i *left1, __m128i *right1,
													 __m128i *left2, __m128i *right2,
													 __m128i *left3, __m128i *right3,
													 const __m128i *key2, const __m128i *key3);
#endif	// OPTIMIZE_MULTI2_FOR_PIPELINE
#endif	// ENABLE_MULTI2_SSSE3

#ifdef ENABLE_MULTI2_AVX2
static inline __m256i byte_swap_avx2(const __m256i *value);
static inline __m256i left_rotate_m256i(const __m256i *value, const int rotate);
static inline __m256i shift_leftsi64_m256i(__m256i value);
static inline __m256i shift_rightsi192_m256i(__m256i value);
static inline void round_pi1_avx2(__m256i *left, __m256i *right);
static inline void round_pi2_avx2(__m256i *left, __m256i *right, const __m256i *key1);
static inline void round_pi3_avx2(__m256i *left, __m256i *right, const __m256i *key2, const __m256i *key3);
static inline void round_pi4_avx2(__m256i *left, __m256i *right, const __m256i *key4);
#ifdef OPTIMIZE_MULTI2_FOR_PIPELINE
static inline void round_pi1_avx2_with_3sets(__m256i *left1, __m256i *right1,
													__m256i *left2, __m256i *right2,
													__m256i *left3, __m256i *right3);
static inline void round_pi2_avx2_with_3sets(__m256i *left1, __m256i *right1,
													__m256i *left2, __m256i *right2,
													__m256i *left3, __m256i *right3,
													const __m256i *key1);
static inline void round_pi3_avx2_with_3sets(__m256i *left1, __m256i *right1,
													__m256i *left2, __m256i *right2,
													__m256i *left3, __m256i *right3,
													const __m256i *key2, const __m256i *key3);
static inline void round_pi4_avx2_with_3sets(__m256i *left1, __m256i *right1,
													__m256i *left2, __m256i *right2,
													__m256i *left3, __m256i *right3,
													const __m256i *key4);
#endif	// OPTIMIZE_MULTI2_FOR_PIPELINE
#endif	// ENABLE_MULTI2_AVX2

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 private method implementation
 ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
static inline const uint32_t left_rotate_uint32_for_simd(const uint32_t value, const uint32_t rotate)
{
	return _lrotl(value, rotate);
}

static inline void round_pi1(uint32_t *left, uint32_t *right)
{
	// Elementary Encryption Function ƒÎ1
	*right ^= *left;
}

static inline void round_pi2(uint32_t *left, uint32_t *right, const uint32_t k1)
{
	// Elementary Encryption Function ƒÎ2
	const uint32_t y = *right + k1;
	const uint32_t z = left_rotate_uint32_for_simd(y, 1UL) + y - 1UL;
	*left ^= left_rotate_uint32_for_simd(z, 4UL) ^ z;
}

static inline void round_pi3(uint32_t *left, uint32_t *right, const uint32_t k2, const uint32_t k3)
{
	// Elementary Encryption Function ƒÎ3
	const uint32_t y = *left + k2;
	const uint32_t z = left_rotate_uint32_for_simd(y, 2UL) + y + 1UL;
	const uint32_t a = left_rotate_uint32_for_simd(z, 8UL) ^ z;
	const uint32_t b = a + k3;
	const uint32_t c = left_rotate_uint32_for_simd(b, 1UL) - b;
	*right ^= (left_rotate_uint32_for_simd(c, 16UL) ^ (c | *left));
}

static inline void round_pi4(uint32_t *left, uint32_t *right, const uint32_t k4)
{
	// Elementary Encryption Function ƒÎ4
	const uint32_t y = *right + k4;
	*left ^= (left_rotate_uint32_for_simd(y, 2UL) + y + 1UL);
}

static inline __m128i left_rotate_m128i(const __m128i *value, const int Rotate)
{
	return _mm_or_si128(_mm_slli_epi32(*value, Rotate), _mm_srli_epi32(*value, 32 - Rotate));
}

static inline __m128i byte_swap_sse2(const __m128i *value)
{
	__m128i t0 = _mm_srli_epi16(*value, 8);
	__m128i t1 = _mm_slli_epi16(*value, 8);
	__m128i t2 = _mm_or_si128(t0, t1);
	return left_rotate_m128i(&t2, 16);
}

static inline void round_pi1_sse2(__m128i *left, __m128i *right)
{
	*right = _mm_xor_si128(*right, *left);
}

static inline void round_pi2_sse2(__m128i *left, __m128i *right, const __m128i *key1)
{
	__m128i t;

	t = _mm_add_epi32(*right, *key1);
	t = _mm_sub_epi32(_mm_add_epi32(left_rotate_m128i(&t, 1), t), IMMEDIATE1);
	t = _mm_xor_si128(left_rotate_m128i(&t, 4), t);

	*left = _mm_xor_si128(*left, t);
}

static inline void round_pi3_sse2(__m128i *left, __m128i *right, const __m128i *key2, const __m128i *key3)
{
	__m128i t;

	t = _mm_add_epi32(*left, *key2);
	t = _mm_add_epi32(_mm_add_epi32(left_rotate_m128i(&t, 2), t), IMMEDIATE1);
	t = _mm_xor_si128(left_rotate_m128i(&t, 8), t);
	t = _mm_add_epi32(t, *key3);
	t = _mm_sub_epi32(left_rotate_m128i(&t, 1), t);
	t = _mm_xor_si128(left_rotate_m128i(&t, 16), _mm_or_si128(t, *left));

	*right = _mm_xor_si128(*right, t);
}

static inline void round_pi4_sse2(__m128i *left, __m128i *right, const __m128i *key4)
{
	__m128i t;

	t = _mm_add_epi32(*right, *key4);
	t = _mm_add_epi32(_mm_add_epi32(left_rotate_m128i(&t, 2), t), IMMEDIATE1);

	*left = _mm_xor_si128(*left, t);
}

#ifdef OPTIMIZE_MULTI2_FOR_PIPELINE

static inline void round_pi1_sse2_with_3sets(__m128i *left1, __m128i *right1,
													__m128i *left2, __m128i *right2,
													__m128i *left3, __m128i *right3)
{
	*right1 = _mm_xor_si128(*right1, *left1);
	*right2 = _mm_xor_si128(*right2, *left2);
	*right3 = _mm_xor_si128(*right3, *left3);
}

static inline void round_pi2_sse2_with_3sets(__m128i *left1, __m128i *right1,
													__m128i *left2, __m128i *right2,
													__m128i *left3, __m128i *right3,
													const __m128i *key1)
{
	__m128i t1, t2, t3;

	t1 = _mm_add_epi32(*right1, *key1);
	t2 = _mm_add_epi32(*right2, *key1);
	t3 = _mm_add_epi32(*right3, *key1);
	t1 = _mm_add_epi32(left_rotate_m128i(&t1, 1), t1);
	t2 = _mm_add_epi32(left_rotate_m128i(&t2, 1), t2);
	t3 = _mm_add_epi32(left_rotate_m128i(&t3, 1), t3);
	t1 = _mm_sub_epi32(t1, IMMEDIATE1);
	t2 = _mm_sub_epi32(t2, IMMEDIATE1);
	t3 = _mm_sub_epi32(t3, IMMEDIATE1);
	t1 = _mm_xor_si128(left_rotate_m128i(&t1, 4), t1);
	t2 = _mm_xor_si128(left_rotate_m128i(&t2, 4), t2);
	t3 = _mm_xor_si128(left_rotate_m128i(&t3, 4), t3);
	*left1 = _mm_xor_si128(*left1, t1);
	*left2 = _mm_xor_si128(*left2, t2);
	*left3 = _mm_xor_si128(*left3, t3);
}

static inline void round_pi3_sse2_with_3sets(__m128i *left1, __m128i *right1,
													__m128i *left2, __m128i *right2,
													__m128i *left3, __m128i *right3,
													const __m128i *key2, const __m128i *key3)
{
	__m128i t1, t2, t3;

	t1 = _mm_add_epi32(*left1, *key2);
	t2 = _mm_add_epi32(*left2, *key2);
	t3 = _mm_add_epi32(*left3, *key2);
	t1 = _mm_add_epi32(left_rotate_m128i(&t1, 2), t1);
	t2 = _mm_add_epi32(left_rotate_m128i(&t2, 2), t2);
	t3 = _mm_add_epi32(left_rotate_m128i(&t3, 2), t3);
	t1 = _mm_add_epi32(t1, IMMEDIATE1);
	t2 = _mm_add_epi32(t2, IMMEDIATE1);
	t3 = _mm_add_epi32(t3, IMMEDIATE1);
	t1 = _mm_xor_si128(left_rotate_m128i(&t1, 8), t1);
	t2 = _mm_xor_si128(left_rotate_m128i(&t2, 8), t2);
	t3 = _mm_xor_si128(left_rotate_m128i(&t3, 8), t3);
	t1 = _mm_add_epi32(t1, *key3);
	t2 = _mm_add_epi32(t2, *key3);
	t3 = _mm_add_epi32(t3, *key3);
	t1 = _mm_sub_epi32(left_rotate_m128i(&t1, 1), t1);
	t2 = _mm_sub_epi32(left_rotate_m128i(&t2, 1), t2);
	t3 = _mm_sub_epi32(left_rotate_m128i(&t3, 1), t3);
	t1 = _mm_xor_si128(left_rotate_m128i(&t1, 16), _mm_or_si128(t1, *left1));
	t2 = _mm_xor_si128(left_rotate_m128i(&t2, 16), _mm_or_si128(t2, *left2));
	t3 = _mm_xor_si128(left_rotate_m128i(&t3, 16), _mm_or_si128(t3, *left3));
	*right1 = _mm_xor_si128(*right1, t1);
	*right2 = _mm_xor_si128(*right2, t2);
	*right3 = _mm_xor_si128(*right3, t3);
}

static inline void round_pi4_sse2_with_3sets(__m128i *left1, __m128i *right1,
													__m128i *left2, __m128i *right2,
													__m128i *left3, __m128i *right3,
													const __m128i *key4)
{
	__m128i t1, t2, t3;

	t1 = _mm_add_epi32(*right1, *key4);
	t2 = _mm_add_epi32(*right2, *key4);
	t3 = _mm_add_epi32(*right3, *key4);
	t1 = _mm_add_epi32(left_rotate_m128i(&t1, 2), t1);
	t2 = _mm_add_epi32(left_rotate_m128i(&t2, 2), t2);
	t3 = _mm_add_epi32(left_rotate_m128i(&t3, 2), t3);
	t1 = _mm_add_epi32(t1, IMMEDIATE1);
	t2 = _mm_add_epi32(t2, IMMEDIATE1);
	t3 = _mm_add_epi32(t3, IMMEDIATE1);
	*left1 = _mm_xor_si128(*left1, t1);
	*left2 = _mm_xor_si128(*left2, t2);
	*left3 = _mm_xor_si128(*left3, t3);
}

#endif	// OPTIMIZE_MULTI2_FOR_PIPELINE

#ifdef ENABLE_MULTI2_SSSE3

static inline __m128i byte_swap_ssse3(const __m128i *value)
{
	return _mm_shuffle_epi8(*value, byte_swap_mask);
}

#define round_pi1_ssse3 round_pi1_sse2
#define round_pi2_ssse3 round_pi2_sse2
#define round_pi4_ssse3 round_pi4_sse2

static inline void round_pi3_ssse3(__m128i *left, __m128i *right,
										  const __m128i *key2, const __m128i *key3)
{
	__m128i t;

	t = _mm_add_epi32(*left, *key2);
	t = _mm_add_epi32(_mm_add_epi32(left_rotate_m128i(&t, 2), t), IMMEDIATE1);
	t = _mm_xor_si128(_mm_shuffle_epi8(t, rotation_8_mask), t);
	t = _mm_add_epi32(t, *key3);
	t = _mm_sub_epi32(left_rotate_m128i(&t, 1), t);
	t = _mm_xor_si128(_mm_shuffle_epi8(t, rotation_16_mask), _mm_or_si128(t, *left));

	*right = _mm_xor_si128(*right, t);
}

#ifdef OPTIMIZE_MULTI2_FOR_PIPELINE

#define round_pi1_ssse3_with_3sets round_pi1_sse2_with_3sets
#define round_pi2_ssse3_with_3sets round_pi2_sse2_with_3sets
#define round_pi4_ssse3_with_3sets round_pi4_sse2_with_3sets

static inline void round_pi3_ssse3_with_3sets(__m128i *left1, __m128i *right1,
													 __m128i *left2, __m128i *right2,
													 __m128i *left3, __m128i *right3,
													 const __m128i *key2, const __m128i *key3)
{
	__m128i t1, t2, t3;

	t1 = _mm_add_epi32(*left1, *key2);
	t2 = _mm_add_epi32(*left2, *key2);
	t3 = _mm_add_epi32(*left3, *key2);
	t1 = _mm_add_epi32(left_rotate_m128i(&t1, 2), t1);
	t2 = _mm_add_epi32(left_rotate_m128i(&t2, 2), t2);
	t3 = _mm_add_epi32(left_rotate_m128i(&t3, 2), t3);
	t1 = _mm_add_epi32(t1, IMMEDIATE1);
	t2 = _mm_add_epi32(t2, IMMEDIATE1);
	t3 = _mm_add_epi32(t3, IMMEDIATE1);
	t1 = _mm_xor_si128(_mm_shuffle_epi8(t1, rotation_8_mask), t1);
	t2 = _mm_xor_si128(_mm_shuffle_epi8(t2, rotation_8_mask), t2);
	t3 = _mm_xor_si128(_mm_shuffle_epi8(t3, rotation_8_mask), t3);
	t1 = _mm_add_epi32(t1, *key3);
	t2 = _mm_add_epi32(t2, *key3);
	t3 = _mm_add_epi32(t3, *key3);
	t1 = _mm_sub_epi32(left_rotate_m128i(&t1, 1), t1);
	t2 = _mm_sub_epi32(left_rotate_m128i(&t2, 1), t2);
	t3 = _mm_sub_epi32(left_rotate_m128i(&t3, 1), t3);
	t1 = _mm_xor_si128(_mm_shuffle_epi8(t1, rotation_16_mask), _mm_or_si128(t1, *left1));
	t2 = _mm_xor_si128(_mm_shuffle_epi8(t2, rotation_16_mask), _mm_or_si128(t2, *left2));
	t3 = _mm_xor_si128(_mm_shuffle_epi8(t3, rotation_16_mask), _mm_or_si128(t3, *left3));
	*right1 = _mm_xor_si128(*right1, t1);
	*right2 = _mm_xor_si128(*right2, t2);
	*right3 = _mm_xor_si128(*right3, t3);
}

#endif	// OPTIMIZE_MULTI2_FOR_PIPELINE

#endif	// ENABLE_MULTI2_SSSE3

#ifdef ENABLE_MULTI2_AVX2

static inline __m256i byte_swap_avx2(const __m256i *value)
{
	return _mm256_shuffle_epi8(*value, byte_swap_mask_avx2);
}

static inline __m256i left_rotate_m256i(const __m256i *value, const int rotate)
{
	return _mm256_or_si256(_mm256_slli_epi32(*value, rotate), _mm256_srli_epi32(*value, 32 - rotate));
}

static inline __m256i shift_leftsi64_m256i(__m256i value)
{
	__m256i mask = _mm256_permute2x128_si256(value, value, 0x08);
	return _mm256_alignr_epi8(value, mask, 8);
}

static inline __m256i shift_rightsi192_m256i(__m256i value)
{
	__m256i t = _mm256_srli_si256(value, 24 - 16);
	return _mm256_permute2x128_si256(t, t, 0x81);
}

static inline void round_pi1_avx2(__m256i *left, __m256i *right)
{
	*right = _mm256_xor_si256(*right, *left);
}

static inline void round_pi2_avx2(__m256i *left, __m256i *right, const __m256i *key1)
{
	__m256i t;

	t = _mm256_add_epi32(*right, *key1);
	t = _mm256_sub_epi32(_mm256_add_epi32(left_rotate_m256i(&t, 1), t), IMMEDIATE1_M256);
	t = _mm256_xor_si256(left_rotate_m256i(&t, 4), t);

	*left = _mm256_xor_si256(*left, t);
}

/*
static inline void round_pi3_avx2(__m256i *left, __m256i *right, const __m256i *key2, const __m256i *key3)
{
	__m256i t;

	t = _mm256_add_epi32(*left, *key2);
	t = _mm256_add_epi32(_mm256_add_epi32(left_rotate_m256i(&t, 2), t), IMMEDIATE1_M256);
	t = _mm256_xor_si256(left_rotate_m256i(&t, 8), t);
	t = _mm256_add_epi32(t, *key3);
	t = _mm256_sub_epi32(left_rotate_m256i(&t, 1), t);
	t = _mm256_xor_si256(left_rotate_m256i(&t, 16), _mm256_or_si256(t, *left));

	*right = _mm256_xor_si256(*right, t);
}
*/

static inline void round_pi3_avx2(__m256i *left, __m256i *right, const __m256i *key2, const __m256i *key3)
{
	__m256i t;

	t = _mm256_add_epi32(*left, *key2);
	t = _mm256_add_epi32(_mm256_add_epi32(left_rotate_m256i(&t, 2), t), IMMEDIATE1_M256);
	t = _mm256_xor_si256(_mm256_shuffle_epi8(t, rotation_8_mask_avx2), t);
	t = _mm256_add_epi32(t, *key3);
	t = _mm256_sub_epi32(left_rotate_m256i(&t, 1), t);
	t = _mm256_xor_si256(_mm256_shuffle_epi8(t, rotation_16_mask_avx2), _mm256_or_si256(t, *left));

	*right = _mm256_xor_si256(*right, t);
}

static inline void round_pi4_avx2(__m256i *left, __m256i *right, const __m256i *key4)
{
	__m256i t;

	t = _mm256_add_epi32(*right, *key4);
	t = _mm256_add_epi32(_mm256_add_epi32(left_rotate_m256i(&t, 2), t), IMMEDIATE1_M256);

	*left = _mm256_xor_si256(*left, t);
}

#ifdef OPTIMIZE_MULTI2_FOR_PIPELINE

static inline void round_pi1_avx2_with_3sets(__m256i *left1, __m256i *right1,
													__m256i *left2, __m256i *right2,
													__m256i *left3, __m256i *right3)
{
	*right1 = _mm256_xor_si256(*right1, *left1);
	*right2 = _mm256_xor_si256(*right2, *left2);
	*right3 = _mm256_xor_si256(*right3, *left3);
}

static inline void round_pi2_avx2_with_3sets(__m256i *left1, __m256i *right1,
													__m256i *left2, __m256i *right2,
													__m256i *left3, __m256i *right3,
													const __m256i *key1)
{
	__m256i t1, t2, t3;

	t1 = _mm256_add_epi32(*right1, *key1);
	t2 = _mm256_add_epi32(*right2, *key1);
	t3 = _mm256_add_epi32(*right3, *key1);
	t1 = _mm256_add_epi32(left_rotate_m256i(&t1, 1), t1);
	t2 = _mm256_add_epi32(left_rotate_m256i(&t2, 1), t2);
	t3 = _mm256_add_epi32(left_rotate_m256i(&t3, 1), t3);
	t1 = _mm256_sub_epi32(t1, IMMEDIATE1_M256);
	t2 = _mm256_sub_epi32(t2, IMMEDIATE1_M256);
	t3 = _mm256_sub_epi32(t3, IMMEDIATE1_M256);
	t1 = _mm256_xor_si256(left_rotate_m256i(&t1, 4), t1);
	t2 = _mm256_xor_si256(left_rotate_m256i(&t2, 4), t2);
	t3 = _mm256_xor_si256(left_rotate_m256i(&t3, 4), t3);
	*left1 = _mm256_xor_si256(*left1, t1);
	*left2 = _mm256_xor_si256(*left2, t2);
	*left3 = _mm256_xor_si256(*left3, t3);
}

/*
static inline void round_pi3_avx2_with_3sets(__m256i *left1, __m256i *right1,
													__m256i *left2, __m256i *right2,
													__m256i *left3, __m256i *right3,
													const __m256i *key2, const __m256i *key3)
{
	__m256i t1, t2, t3;

	t1 = _mm256_add_epi32(*left1, *key2);
	t2 = _mm256_add_epi32(*left2, *key2);
	t3 = _mm256_add_epi32(*left3, *key2);
	t1 = _mm256_add_epi32(left_rotate_m256i(&t1, 2), t1);
	t2 = _mm256_add_epi32(left_rotate_m256i(&t2, 2), t2);
	t3 = _mm256_add_epi32(left_rotate_m256i(&t3, 2), t3);
	t1 = _mm256_add_epi32(t1, IMMEDIATE1_M256);
	t2 = _mm256_add_epi32(t2, IMMEDIATE1_M256);
	t3 = _mm256_add_epi32(t3, IMMEDIATE1_M256);
	t1 = _mm256_xor_si256(left_rotate_m256i(&t1, 8), t1);
	t2 = _mm256_xor_si256(left_rotate_m256i(&t2, 8), t2);
	t3 = _mm256_xor_si256(left_rotate_m256i(&t3, 8), t3);
	t1 = _mm256_add_epi32(t1, *key3);
	t2 = _mm256_add_epi32(t2, *key3);
	t3 = _mm256_add_epi32(t3, *key3);
	t1 = _mm256_sub_epi32(left_rotate_m256i(&t1, 1), t1);
	t2 = _mm256_sub_epi32(left_rotate_m256i(&t2, 1), t2);
	t3 = _mm256_sub_epi32(left_rotate_m256i(&t3, 1), t3);
	t1 = _mm256_xor_si256(left_rotate_m256i(&t1, 16), _mm256_or_si256(t1, *left1));
	t2 = _mm256_xor_si256(left_rotate_m256i(&t2, 16), _mm256_or_si256(t2, *left2));
	t3 = _mm256_xor_si256(left_rotate_m256i(&t3, 16), _mm256_or_si256(t3, *left3));
	*right1 = _mm256_xor_si256(*right1, t1);
	*right2 = _mm256_xor_si256(*right2, t2);
	*right3 = _mm256_xor_si256(*right3, t3);
}
*/

static inline void round_pi3_avx2_with_3sets(__m256i *left1, __m256i *right1,
													__m256i *left2, __m256i *right2,
													__m256i *left3, __m256i *right3,
													const __m256i *key2, const __m256i *key3)
{
	__m256i t1, t2, t3;

	t1 = _mm256_add_epi32(*left1, *key2);
	t2 = _mm256_add_epi32(*left2, *key2);
	t3 = _mm256_add_epi32(*left3, *key2);
	t1 = _mm256_add_epi32(left_rotate_m256i(&t1, 2), t1);
	t2 = _mm256_add_epi32(left_rotate_m256i(&t2, 2), t2);
	t3 = _mm256_add_epi32(left_rotate_m256i(&t3, 2), t3);
	t1 = _mm256_add_epi32(t1, IMMEDIATE1_M256);
	t2 = _mm256_add_epi32(t2, IMMEDIATE1_M256);
	t3 = _mm256_add_epi32(t3, IMMEDIATE1_M256);
	t1 = _mm256_xor_si256(_mm256_shuffle_epi8(t1, rotation_8_mask_avx2), t1);
	t2 = _mm256_xor_si256(_mm256_shuffle_epi8(t2, rotation_8_mask_avx2), t2);
	t3 = _mm256_xor_si256(_mm256_shuffle_epi8(t3, rotation_8_mask_avx2), t3);
	t1 = _mm256_add_epi32(t1, *key3);
	t2 = _mm256_add_epi32(t2, *key3);
	t3 = _mm256_add_epi32(t3, *key3);
	t1 = _mm256_sub_epi32(left_rotate_m256i(&t1, 1), t1);
	t2 = _mm256_sub_epi32(left_rotate_m256i(&t2, 1), t2);
	t3 = _mm256_sub_epi32(left_rotate_m256i(&t3, 1), t3);
	t1 = _mm256_xor_si256(_mm256_shuffle_epi8(t1, rotation_16_mask_avx2), _mm256_or_si256(t1, *left1));
	t2 = _mm256_xor_si256(_mm256_shuffle_epi8(t2, rotation_16_mask_avx2), _mm256_or_si256(t2, *left2));
	t3 = _mm256_xor_si256(_mm256_shuffle_epi8(t3, rotation_16_mask_avx2), _mm256_or_si256(t3, *left3));
	*right1 = _mm256_xor_si256(*right1, t1);
	*right2 = _mm256_xor_si256(*right2, t2);
	*right3 = _mm256_xor_si256(*right3, t3);
}

static inline void round_pi4_avx2_with_3sets(__m256i *left1, __m256i *right1,
													__m256i *left2, __m256i *right2,
													__m256i *left3, __m256i *right3,
													const __m256i *key4)
{
	__m256i t1, t2, t3;

	t1 = _mm256_add_epi32(*right1, *key4);
	t2 = _mm256_add_epi32(*right2, *key4);
	t3 = _mm256_add_epi32(*right3, *key4);
	t1 = _mm256_add_epi32(left_rotate_m256i(&t1, 2), t1);
	t2 = _mm256_add_epi32(left_rotate_m256i(&t2, 2), t2);
	t3 = _mm256_add_epi32(left_rotate_m256i(&t3, 2), t3);
	t1 = _mm256_add_epi32(t1, IMMEDIATE1_M256);
	t2 = _mm256_add_epi32(t2, IMMEDIATE1_M256);
	t3 = _mm256_add_epi32(t3, IMMEDIATE1_M256);
	*left1 = _mm256_xor_si256(*left1, t1);
	*left2 = _mm256_xor_si256(*left2, t2);
	*left3 = _mm256_xor_si256(*left3, t3);
}

#endif // OPTIMIZE_MULTI2_FOR_PIPELINE

#endif // ENABLE_MULTI2_AVX2

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 global function implementation
 ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
bool is_simd_enabled()
{
#ifdef ENABLE_MULTI2_SIMD
#ifdef _MSC_VER
	__assume(simd_instruction != INSTRUCTION_NORMAL);
	return simd_instruction != INSTRUCTION_NORMAL;
#else
	return __builtin_expect(simd_instruction != INSTRUCTION_NORMAL, 1);
#endif
#else
	return false;
#endif
}

bool is_sse2_available()
{
#if defined(_M_IX86)
#ifdef _MSC_VER
	bool b;

	__asm {
		mov		eax, 1
		cpuid
		bt		edx, 26
		setc	b
	}

	return b;
#else
	int Info[4];
	__cpuid(1, Info[0], Info[1], Info[2], Info[3]);

	if (Info[3] & 0x4000000)	// bt edx, 26
		return true;
#endif
#elif defined(_M_AMD64) || defined(_M_X64)
	return true;
#else
	return ::IsProcessorFeaturePresent(PF_XMMI64_INSTRUCTIONS_AVAILABLE) != FALSE;
#endif
}

bool is_ssse3_available()
{
	int Info[4];
#ifdef _MSC_VER
	__cpuid(Info, 1);
#else
	__cpuid(1, Info[0], Info[1], Info[2], Info[3]);
#endif

	if (Info[2] & 0x200)	// bt ecx, 9
		return true;

	return false;
}

bool is_avx2_available()
{
	int Info[4];
#ifdef _MSC_VER
	__cpuidex(Info, 7, 0);
#else
	__cpuid_count(7, 0, Info[0], Info[1], Info[2], Info[3]);
#endif

	if (Info[1] & 0x020)	// bt ebx, 5
#ifdef _WIN32
		return (bool)IsWindows7SP1OrGreater();
#else
		return true;
#endif

	return false;
}

bool initialize_multi2_simd(enum INSTRUCTION_TYPE instruction, void* m2)
{
	if (!is_sse2_available() || instruction == INSTRUCTION_NORMAL) {
		set_simd_instruction(INSTRUCTION_NORMAL);
		return false;
	}

	enum INSTRUCTION_TYPE supported_instruction = get_supported_simd_instruction();
	if (!is_mask_initialized) {
#ifdef ENABLE_MULTI2_AVX2
		if (supported_instruction >= INSTRUCTION_AVX2) {
			byte_swap_mask_avx2   = _mm256_set_epi8(
				12, 13, 14, 15,  8,  9, 10, 11,  4,  5,  6,  7,  0,  1,  2,  3,
				12, 13, 14, 15,  8,  9, 10, 11,  4,  5,  6,  7,  0,  1,  2,  3
			);
			src_swap_mask_avx2    = _mm256_set_epi8(
				12, 13, 14, 15,  4,  5,  6,  7,  8,  9, 10, 11,  0,  1,  2,  3,
				12, 13, 14, 15,  4,  5,  6,  7,  8,  9, 10, 11,  0,  1,  2,  3
			);
			rotation_16_mask_avx2 = _mm256_set_epi8(
				13, 12, 15, 14,  9,  8, 11, 10,  5,  4,  7,  6,  1,  0,  3,  2,
				13, 12, 15, 14,  9,  8, 11, 10,  5,  4,  7,  6,  1,  0,  3,  2
			);
			rotation_8_mask_avx2  = _mm256_set_epi8(
				14, 13, 12, 15, 10,  9,  8, 11,  6,  5,  4,  7,  2,  1,  0,  3,
				14, 13, 12, 15, 10,  9,  8, 11,  6,  5,  4,  7,  2,  1,  0,  3
			);
		}
#endif
#if defined(ENABLE_MULTI2_SSSE3)// || defined(ENABLE_MULTI2_AVX2)
		if (supported_instruction >= INSTRUCTION_SSSE3) {
			byte_swap_mask   = _mm_set_epi8(12, 13, 14, 15, 8, 9, 10, 11, 4, 5, 6, 7, 0, 1, 2, 3);
			src_swap_mask    = _mm_set_epi8(12, 13, 14, 15, 4, 5, 6, 7, 8, 9, 10, 11, 0, 1, 2, 3);
			rotation_16_mask = _mm_set_epi8(13, 12, 15, 14, 9, 8, 11, 10, 5, 4, 7, 6, 1, 0, 3, 2);
			rotation_8_mask  = _mm_set_epi8(14, 13, 12, 15, 10, 9, 8, 11, 6, 5, 4, 7, 2, 1, 0, 3);
		}
#endif
		is_mask_initialized = true;
	}

	if (instruction <= supported_instruction) {
		set_simd_instruction(instruction);
	} else {
		set_simd_instruction(supported_instruction);
	}
	return true;
}

void set_simd_instruction(enum INSTRUCTION_TYPE instruction)
{
	simd_instruction = instruction;
}

enum INSTRUCTION_TYPE get_simd_instruction()
{
	return simd_instruction;
}

enum INSTRUCTION_TYPE get_supported_simd_instruction()
{
	if (is_avx2_available()) {
		return INSTRUCTION_AVX2;
	} else if (is_ssse3_available()) {
		return INSTRUCTION_SSSE3;
	} else if (is_sse2_available()) {
		return INSTRUCTION_SSE2;
	}
	return INSTRUCTION_NORMAL;
}

void alloc_work_key_for_simd(MULTI2_SIMD_WORK_KEY **work_key_odd, MULTI2_SIMD_WORK_KEY **work_key_even)
{
	*work_key_odd = (MULTI2_SIMD_WORK_KEY*)_mm_malloc(sizeof(MULTI2_SIMD_WORK_KEY) * 2, 16);
	*work_key_even = *work_key_odd + 1;
}

void free_work_key_for_simd(MULTI2_SIMD_WORK_KEY **work_key_odd, MULTI2_SIMD_WORK_KEY **work_key_even)
{
	if (*work_key_odd) {
		_mm_free(*work_key_odd);
		*work_key_odd = NULL;
		*work_key_even = NULL;
	}
}

void set_work_key_for_simd(MULTI2_SIMD_WORK_KEY *work_key, const MULTI2_SIMD_SYS_KEY *src_key)
{
	work_key->key[0] = _mm_set1_epi32(src_key->key1);
	work_key->key[1] = _mm_set1_epi32(src_key->key2);
	work_key->key[2] = _mm_set1_epi32(src_key->key3);
	work_key->key[3] = _mm_set1_epi32(src_key->key4);
	work_key->key[4] = _mm_set1_epi32(src_key->key5);
	work_key->key[5] = _mm_set1_epi32(src_key->key6);
	work_key->key[6] = _mm_set1_epi32(src_key->key7);
	work_key->key[7] = _mm_set1_epi32(src_key->key8);
}

void set_work_key_for_avx2(MULTI2_SIMD_WORK_KEY *work_key, const MULTI2_SIMD_SYS_KEY *src_key)
{
#ifdef ENABLE_MULTI2_AVX2
	work_key->key256[0] = _mm256_set1_epi32(src_key->key1);
	work_key->key256[1] = _mm256_set1_epi32(src_key->key2);
	work_key->key256[2] = _mm256_set1_epi32(src_key->key3);
	work_key->key256[3] = _mm256_set1_epi32(src_key->key4);
	work_key->key256[4] = _mm256_set1_epi32(src_key->key5);
	work_key->key256[5] = _mm256_set1_epi32(src_key->key6);
	work_key->key256[6] = _mm256_set1_epi32(src_key->key7);
	work_key->key256[7] = _mm256_set1_epi32(src_key->key8);
#else
	set_work_key_for_simd(work_key, src_key);
#endif
}

void set_round_for_simd(const uint32_t round)
{
	scramble_round = round;
}

void set_system_key_with_bswap(MULTI2_SIMD_SYS_KEY *sys_key, const uint8_t *hex_data)
{
	// reverse byte order
#ifndef USE_MULTI2_INTRINSIC
	uint8_t *data = sys_key->data;
	data[ 3] = hex_data[ 0];	data[ 2] = hex_data[ 1];	data[ 1] = hex_data[ 2];	data[ 0] = hex_data[ 3];
	data[ 7] = hex_data[ 4];	data[ 6] = hex_data[ 5];	data[ 5] = hex_data[ 6];	data[ 4] = hex_data[ 7];
	data[11] = hex_data[ 8];	data[10] = hex_data[ 9];	data[ 9] = hex_data[10];	data[ 8] = hex_data[11];
	data[15] = hex_data[12];	data[14] = hex_data[13];	data[13] = hex_data[14];	data[12] = hex_data[15];
	data[19] = hex_data[16];	data[18] = hex_data[17];	data[17] = hex_data[18];	data[16] = hex_data[19];
	data[23] = hex_data[20];	data[22] = hex_data[21];	data[21] = hex_data[22];	data[20] = hex_data[23];
	data[27] = hex_data[24];	data[26] = hex_data[25];	data[25] = hex_data[26];	data[24] = hex_data[27];
	data[31] = hex_data[28];	data[30] = hex_data[29];	data[29] = hex_data[30];	data[28] = hex_data[31];
#else
//#ifndef _M_X64
#if defined(_M_X64) || !defined(_M_X64)
	const uint32_t *p = (const uint32_t *)hex_data;
	sys_key->key1 = _byteswap_ulong(p[0]);
	sys_key->key2 = _byteswap_ulong(p[1]);
	sys_key->key3 = _byteswap_ulong(p[2]);
	sys_key->key4 = _byteswap_ulong(p[3]);
	sys_key->key5 = _byteswap_ulong(p[4]);
	sys_key->key6 = _byteswap_ulong(p[5]);
	sys_key->key7 = _byteswap_ulong(p[6]);
	sys_key->key8 = _byteswap_ulong(p[7]);
#else
	const uint64_t *p = (const uint64_t *)hex_data;
	sys_key->data64[0] = _byteswap_uint64(p[0]);
	sys_key->data64[1] = _byteswap_uint64(p[1]);
	sys_key->data64[2] = _byteswap_uint64(p[2]);
	sys_key->data64[3] = _byteswap_uint64(p[3]);
#endif
#endif
}

void get_system_key_with_bswap(const MULTI2_SIMD_SYS_KEY *sys_key, uint8_t *hex_data)
{
	// reverse byte order
#ifndef USE_MULTI2_INTRINSIC
	const uint8_t *data = sys_key->data;
	hex_data[ 0] = data[ 3];	hex_data[ 1] = data[ 2];	hex_data[ 2] = data[ 1];	hex_data[ 3] = data[ 0];
	hex_data[ 4] = data[ 7];	hex_data[ 5] = data[ 6];	hex_data[ 6] = data[ 5];	hex_data[ 7] = data[ 4];
	hex_data[ 8] = data[11];	hex_data[ 9] = data[10];	hex_data[10] = data[ 9];	hex_data[11] = data[ 8];
	hex_data[12] = data[15];	hex_data[13] = data[14];	hex_data[14] = data[13];	hex_data[15] = data[12];
	hex_data[16] = data[19];	hex_data[17] = data[18];	hex_data[18] = data[17];	hex_data[19] = data[16];
	hex_data[20] = data[23];	hex_data[21] = data[22];	hex_data[22] = data[21];	hex_data[23] = data[20];
	hex_data[24] = data[27];	hex_data[25] = data[26];	hex_data[26] = data[25];	hex_data[27] = data[24];
	hex_data[28] = data[31];	hex_data[29] = data[30];	hex_data[30] = data[29];	hex_data[31] = data[28];
#else
//#ifndef _M_X64
#if defined(_M_X64) || !defined(_M_X64)
	uint32_t *p = (uint32_t *)hex_data;
	p[0] = _byteswap_ulong(sys_key->key1);
	p[1] = _byteswap_ulong(sys_key->key2);
	p[2] = _byteswap_ulong(sys_key->key3);
	p[3] = _byteswap_ulong(sys_key->key4);
	p[4] = _byteswap_ulong(sys_key->key5);
	p[5] = _byteswap_ulong(sys_key->key6);
	p[6] = _byteswap_ulong(sys_key->key7);
	p[7] = _byteswap_ulong(sys_key->key8);
#else
	uint64_t *p = (uint64_t *)hex_data;
	p[0] = _byteswap_uint64(sys_key->data64[0]);
	p[1] = _byteswap_uint64(sys_key->data64[1]);
	p[2] = _byteswap_uint64(sys_key->data64[2]);
	p[3] = _byteswap_uint64(sys_key->data64[3]);
#endif
#endif
}

void set_data_key_with_bswap(MULTI2_SIMD_DATA_KEY *data_key, const uint8_t *hex_data)
{
	// reverse byte order
#ifndef USE_MULTI2_INTRINSIC
	uint8_t *data = data_key->data;
	data[7] = hex_data[0];	data[6] = hex_data[1];	data[5] = hex_data[2];	data[4] = hex_data[3];
	data[3] = hex_data[4];	data[2] = hex_data[5];	data[1] = hex_data[6];	data[0] = hex_data[7];
#else
#ifndef _M_X64
	data_key->left  = _byteswap_ulong(*(const uint32_t *)(hex_data + 0));
	data_key->right = _byteswap_ulong(*(const uint32_t *)(hex_data + 4));
#else
	data_key->data64 = _byteswap_uint64(*(const uint64_t *)(hex_data));
#endif
#endif
}

void get_data_key_with_bswap(const MULTI2_SIMD_DATA_KEY *data_key, uint8_t *hex_data)
{
	// reverse byte order
#ifndef USE_MULTI2_INTRINSIC
	const uint8_t *data = data_key->data;
	hex_data[0] = data[7];	hex_data[1] = data[6];	hex_data[2] = data[5];	hex_data[3] = data[4];
	hex_data[4] = data[3];	hex_data[5] = data[2];	hex_data[6] = data[1];	hex_data[7] = data[0];
#else
#ifndef _M_X64
	*(uint32_t *)(hex_data + 0) = _byteswap_ulong(data_key->left);
	*(uint32_t *)(hex_data + 4) = _byteswap_ulong(data_key->right);
#else
	*(uint64_t *)(hex_data) = _byteswap_uint64(data_key->data64);
#endif
#endif
}

void decrypt_multi2_without_simd(uint8_t * __restrict data, const uint32_t size,
								 const MULTI2_SIMD_SYS_KEY * __restrict work_key,
								 const MULTI2_SIMD_WORK_KEY * __restrict packed_work_key,
								 const MULTI2_SIMD_DATA_KEY * __restrict cbc_init)
{
#ifdef _MSC_VER
	__assume(size <= 184);
#endif

	uint8_t * __restrict p = data;
	uint32_t cbc_left = cbc_init->left, cbc_right = cbc_init->right;

	for (uint8_t *ptr_end = p + (size & 0xFFFFFFF8UL); p < ptr_end; p += 8) {
		uint32_t src1, src2, left, right;

		src1 = _byteswap_ulong(*(uint32_t*)(p + 0));
		src2 = _byteswap_ulong(*(uint32_t*)(p + 4));
		left  = src1;
		right = src2;

#if defined(__INTEL_COMPILER) && MULTI2_SIMD_SCRAMBLE_ROUND <= 4
#pragma unroll(4)
#endif
		for (uint32_t round = 0U; round < MAX_SCRAMBLE_ROUND; ++round) {
			round_pi4(&left, &right, work_key->key8);
			round_pi3(&left, &right, work_key->key6, work_key->key7);
			round_pi2(&left, &right, work_key->key5);
			round_pi1(&left, &right);
			round_pi4(&left, &right, work_key->key4);
			round_pi3(&left, &right, work_key->key2, work_key->key3);
			round_pi2(&left, &right, work_key->key1);
			round_pi1(&left, &right);
		}

		*(uint32_t*)(p + 0) = _byteswap_ulong(left  ^ cbc_left);
		*(uint32_t*)(p + 4) = _byteswap_ulong(right ^ cbc_right);
		cbc_left  = src1;
		cbc_right = src2;
	}

	// OFB mode
	uint32_t remain_size = size & 0x00000007UL;
	if (remain_size) {
		for (uint32_t round = 0U; round < MAX_SCRAMBLE_ROUND; ++round) {
			round_pi1(&cbc_left, &cbc_right);
			round_pi2(&cbc_left, &cbc_right, work_key->key1);
			round_pi3(&cbc_left, &cbc_right, work_key->key2, work_key->key3);
			round_pi4(&cbc_left, &cbc_right, work_key->key4);
			round_pi1(&cbc_left, &cbc_right);
			round_pi2(&cbc_left, &cbc_right, work_key->key5);
			round_pi3(&cbc_left, &cbc_right, work_key->key6, work_key->key7);
			round_pi4(&cbc_left, &cbc_right, work_key->key8);
		}

		uint8_t remain[8];
		*(uint32_t*)(remain + 0) = cbc_left;
		*(uint32_t*)(remain + 4) = cbc_right;
		switch (remain_size) {
#ifdef _MSC_VER
		default: __assume(0);
#else
		default:
#endif
		case 7: p[6] ^= remain[5];
		case 6: p[5] ^= remain[6];
		case 5: p[4] ^= remain[7];
		case 4: p[3] ^= remain[0];
		case 3: p[2] ^= remain[1];
		case 2: p[1] ^= remain[2];
		case 1: p[0] ^= remain[3];
		}
	}
}

#ifdef ENABLE_MULTI2_SSE2

void decrypt_multi2_with_sse2(uint8_t * __restrict data, const uint32_t size,
							  const MULTI2_SIMD_SYS_KEY * __restrict work_key,
							  const MULTI2_SIMD_WORK_KEY * __restrict packed_work_key,
							  const MULTI2_SIMD_DATA_KEY * __restrict cbc_init)
{
#ifdef _MSC_VER
	__assume(size <= 184);
#endif

	uint8_t * __restrict p = data;
	__m128i cbc = _mm_set_epi32(0, 0, cbc_init->right, cbc_init->left);

	// 99% of TS packets which should be descrambled are 184 bytes
#ifdef _MSC_VER
	if (size == 184) {
#else
	if (__builtin_expect(size == 184, 1)) {
#endif
		// copy and zero-fill last 8 bytes, because this proccess descrambles 192 bytes
		ALIGNAS(16) uint8_t backup[8];
		memcpy(backup, data + 184, 8);
		memset(data + 184, 0, 8);

#ifndef OPTIMIZE_MULTI2_FOR_PIPELINE

		for (int i = 0; i < 6; i++) {
			__m128i src1, src2, left, right;

			// r2 l2 r1 l1
			src1 = _mm_loadu_si128((__m128i*)(p +  0));
			src1 = byte_swap_sse2(&src1);
			// r4 l4 r3 l3
			src2 = _mm_loadu_si128((__m128i*)(p + 16));
			src2 = byte_swap_sse2(&src2);

			// r2 r1 l2 l1
			__m128i x = _mm_shuffle_epi32(src1, MM_SHUFFLE4(3, 1, 2, 0));
			// r4 r3 l4 l3
			__m128i y = _mm_shuffle_epi32(src2, MM_SHUFFLE4(3, 1, 2, 0));

			// l4 l3 l2 l1
			left  = _mm_unpacklo_epi64(x, y);
			// r4 r3 r2 r1
			right = _mm_unpackhi_epi64(x, y);

#if defined(__INTEL_COMPILER) && MULTI2_SIMD_SCRAMBLE_ROUND <= 4
#pragma unroll(4)
#endif
			for (uint32_t i = 0; i < MAX_SCRAMBLE_ROUND; i++) {
				round_pi4_sse2(&left, &right, &(packed_work_key->key[7]));
				round_pi3_sse2(&left, &right, &(packed_work_key->key[5]), &(packed_work_key->key[6]));
				round_pi2_sse2(&left, &right, &(packed_work_key->key[4]));
				round_pi1_sse2(&left, &right);
				round_pi4_sse2(&left, &right, &(packed_work_key->key[3]));
				round_pi3_sse2(&left, &right, &(packed_work_key->key[1]), &(packed_work_key->key[2]));
				round_pi2_sse2(&left, &right, &(packed_work_key->key[0]));
				round_pi1_sse2(&left, &right);
			}

			// r2 l2 r1 l1
			x = _mm_unpacklo_epi32(left, right);
			// r4 l4 r3 l3
			y = _mm_unpackhi_epi32(left, right);

			x = _mm_xor_si128(x, _mm_unpacklo_epi64(cbc, src1));
			cbc = _mm_shuffle_epi32(src2, MM_SHUFFLE4(1, 0, 3, 2));
			y = _mm_xor_si128(y, _mm_unpackhi_epi64(src1, cbc));

			_mm_storeu_si128((__m128i*)(p +  0), byte_swap_sse2(&x));
			_mm_storeu_si128((__m128i*)(p + 16), byte_swap_sse2(&y));

			p += 32;
		}

#else	// OPTIMIZE_MULTI2_FOR_PIPELINE

		// optimize for pipeline
		for (int i = 0; i < 2; ++i) {
			__m128i src1, src2, src3, src4, src5, src6;
			__m128i left1, right1, left2, right2, left3, right3;
			__m128i x1, y1, x2, y2, x3, y3;

			src1 = _mm_loadu_si128((__m128i*)(p +  0));
			src2 = _mm_loadu_si128((__m128i*)(p + 16));
			src3 = _mm_loadu_si128((__m128i*)(p + 32));
			src4 = _mm_loadu_si128((__m128i*)(p + 48));
			src5 = _mm_loadu_si128((__m128i*)(p + 64));
			src6 = _mm_loadu_si128((__m128i*)(p + 80));

			src1 = byte_swap_sse2(&src1);
			src2 = byte_swap_sse2(&src2);
			src3 = byte_swap_sse2(&src3);
			src4 = byte_swap_sse2(&src4);
			src5 = byte_swap_sse2(&src5);
			src6 = byte_swap_sse2(&src6);

			x1 = _mm_shuffle_epi32(src1, MM_SHUFFLE4(3, 1, 2, 0));
			y1 = _mm_shuffle_epi32(src2, MM_SHUFFLE4(3, 1, 2, 0));
			x2 = _mm_shuffle_epi32(src3, MM_SHUFFLE4(3, 1, 2, 0));
			y2 = _mm_shuffle_epi32(src4, MM_SHUFFLE4(3, 1, 2, 0));
			x3 = _mm_shuffle_epi32(src5, MM_SHUFFLE4(3, 1, 2, 0));
			y3 = _mm_shuffle_epi32(src6, MM_SHUFFLE4(3, 1, 2, 0));

			left1  = _mm_unpacklo_epi64(x1, y1);
			right1 = _mm_unpackhi_epi64(x1, y1);
			left2  = _mm_unpacklo_epi64(x2, y2);
			right2 = _mm_unpackhi_epi64(x2, y2);
			left3  = _mm_unpacklo_epi64(x3, y3);
			right3 = _mm_unpackhi_epi64(x3, y3);

#if defined(__INTEL_COMPILER) && MULTI2_SIMD_SCRAMBLE_ROUND <= 4
#pragma unroll(4)
#endif
			for (uint32_t i = 0U; i < MAX_SCRAMBLE_ROUND; i++) {
				round_pi4_sse2_with_3sets(&left1, &right1, &left2, &right2, &left3, &right3, &(packed_work_key->key[7]));
				round_pi3_sse2_with_3sets(&left1, &right1, &left2, &right2, &left3, &right3,
										  &(packed_work_key->key[5]), &(packed_work_key->key[6]));
				round_pi2_sse2_with_3sets(&left1, &right1, &left2, &right2, &left3, &right3, &(packed_work_key->key[4]));
				round_pi1_sse2_with_3sets(&left1, &right1, &left2, &right2, &left3, &right3);
				round_pi4_sse2_with_3sets(&left1, &right1, &left2, &right2, &left3, &right3, &(packed_work_key->key[3]));
				round_pi3_sse2_with_3sets(&left1, &right1, &left2, &right2, &left3, &right3,
										  &(packed_work_key->key[1]), &(packed_work_key->key[2]));
				round_pi2_sse2_with_3sets(&left1, &right1, &left2, &right2, &left3, &right3, &(packed_work_key->key[0]));
				round_pi1_sse2_with_3sets(&left1, &right1, &left2, &right2, &left3, &right3);
			}

			x1 = _mm_unpacklo_epi32(left1, right1);
			y1 = _mm_unpackhi_epi32(left1, right1);
			x2 = _mm_unpacklo_epi32(left2, right2);
			y2 = _mm_unpackhi_epi32(left2, right2);
			x3 = _mm_unpacklo_epi32(left3, right3);
			y3 = _mm_unpackhi_epi32(left3, right3);

			src2 = _mm_shuffle_epi32(src2, MM_SHUFFLE4(1, 0, 3, 2));
			src4 = _mm_shuffle_epi32(src4, MM_SHUFFLE4(1, 0, 3, 2));
			src6 = _mm_shuffle_epi32(src6, MM_SHUFFLE4(1, 0, 3, 2));
			x1 = _mm_xor_si128(x1, _mm_unpacklo_epi64(cbc, src1));
			y1 = _mm_xor_si128(y1, _mm_unpackhi_epi64(src1, src2));
			x2 = _mm_xor_si128(x2, _mm_unpacklo_epi64(src2, src3));
			y2 = _mm_xor_si128(y2, _mm_unpackhi_epi64(src3, src4));
			x3 = _mm_xor_si128(x3, _mm_unpacklo_epi64(src4, src5));
			y3 = _mm_xor_si128(y3, _mm_unpackhi_epi64(src5, src6));
			cbc = src6;

			x1 = byte_swap_sse2(&x1);
			y1 = byte_swap_sse2(&y1);
			x2 = byte_swap_sse2(&x2);
			y2 = byte_swap_sse2(&y2);
			x3 = byte_swap_sse2(&x3);
			y3 = byte_swap_sse2(&y3);

			_mm_storeu_si128((__m128i*)(p +  0), x1);
			_mm_storeu_si128((__m128i*)(p + 16), y1);
			_mm_storeu_si128((__m128i*)(p + 32), x2);
			_mm_storeu_si128((__m128i*)(p + 48), y2);
			_mm_storeu_si128((__m128i*)(p + 64), x3);
			_mm_storeu_si128((__m128i*)(p + 80), y3);

			p += 32 * 3;
		}

#endif	// OPTIMIZE_MULTI2_FOR_PIPELINE

		// restore last 8 bytes from backup
		memcpy(data + 184, backup, 8);
		return;
	}

	// CBC mode
	for (uint8_t *ptr_end = p + (size & 0xFFFFFFE0UL); p < ptr_end; p += 32) {
		__m128i src1, src2, left, right;

		// r2 l2 r1 l1
		src1 = _mm_loadu_si128((__m128i*)p);
		src1 = byte_swap_sse2(&src1);
		// r4 l4 r3 l3
		src2 = _mm_loadu_si128((__m128i*)(p + 16));
		src2 = byte_swap_sse2(&src2);

		// r2 r1 l2 l1
		__m128i x = _mm_shuffle_epi32(src1, MM_SHUFFLE4(3, 1, 2, 0));
		// r4 r3 l4 l3
		__m128i y = _mm_shuffle_epi32(src2, MM_SHUFFLE4(3, 1, 2, 0));

		// l4 l3 l2 l1
		left  = _mm_unpacklo_epi64(x, y);
		// r4 r3 r2 r1
		right = _mm_unpackhi_epi64(x, y);

		for (uint32_t i = 0U; i < MAX_SCRAMBLE_ROUND; i++) {
			round_pi4_sse2(&left, &right, &(packed_work_key->key[7]));
			round_pi3_sse2(&left, &right, &(packed_work_key->key[5]), &(packed_work_key->key[6]));
			round_pi2_sse2(&left, &right, &(packed_work_key->key[4]));
			round_pi1_sse2(&left, &right);
			round_pi4_sse2(&left, &right, &(packed_work_key->key[3]));
			round_pi3_sse2(&left, &right, &(packed_work_key->key[1]), &(packed_work_key->key[2]));
			round_pi2_sse2(&left, &right, &(packed_work_key->key[0]));
			round_pi1_sse2(&left, &right);
		}

		// r2 l2 r1 l1
		x = _mm_unpacklo_epi32(left, right);
		// r4 l4 r3 l3
		y = _mm_unpackhi_epi32(left, right);

#if 0
		cbc = _mm_or_si128(_mm_slli_si128(src1, 8), cbc);
		x = _mm_xor_si128(x, cbc);

		cbc = _mm_or_si128(_mm_slli_si128(src2, 8), _mm_srli_si128(src1, 8));
		y = _mm_xor_si128(y, cbc);

		cbc = _mm_srli_si128(src2, 8);
#else
		x = _mm_xor_si128(x, _mm_unpacklo_epi64(cbc, src1));
		cbc = _mm_shuffle_epi32(src2, MM_SHUFFLE4(1, 0, 3, 2));
		y = _mm_xor_si128(y, _mm_unpackhi_epi64(src1, cbc));
#endif

		_mm_storeu_si128((__m128i*)p,        byte_swap_sse2(&x));
		_mm_storeu_si128((__m128i*)(p + 16), byte_swap_sse2(&y));
	}

	uint32_t cbc_left, cbc_right;
	ALIGNAS(16) uint32_t temp_data[4];
	_mm_storeu_si128((__m128i*)temp_data, cbc);
	cbc_left  = temp_data[0];
	cbc_right = temp_data[1];

	for (uint8_t *ptr_end = p + (size & 0x00000018UL); p < ptr_end; p += 8) {
		uint32_t src1, src2, left, right;

		src1 = _byteswap_ulong(*(uint32_t*)(p + 0));
		src2 = _byteswap_ulong(*(uint32_t*)(p + 4));
		left  = src1;
		right = src2;

		for (uint32_t round = 0U; round < MAX_SCRAMBLE_ROUND; ++round) {
			round_pi4(&left, &right, work_key->key8);
			round_pi3(&left, &right, work_key->key6, work_key->key7);
			round_pi2(&left, &right, work_key->key5);
			round_pi1(&left, &right);
			round_pi4(&left, &right, work_key->key4);
			round_pi3(&left, &right, work_key->key2, work_key->key3);
			round_pi2(&left, &right, work_key->key1);
			round_pi1(&left, &right);
		}

		*(uint32_t*)(p + 0) = _byteswap_ulong(left  ^ cbc_left);
		*(uint32_t*)(p + 4) = _byteswap_ulong(right ^ cbc_right);
		cbc_left  = src1;
		cbc_right = src2;
	}

	// OFB mode
	uint32_t remain_size = size & 0x00000007UL;
	if (remain_size) {
		for (uint32_t round = 0U; round < MAX_SCRAMBLE_ROUND; ++round) {
			round_pi1(&cbc_left, &cbc_right);
			round_pi2(&cbc_left, &cbc_right, work_key->key1);
			round_pi3(&cbc_left, &cbc_right, work_key->key2, work_key->key3);
			round_pi4(&cbc_left, &cbc_right, work_key->key4);
			round_pi1(&cbc_left, &cbc_right);
			round_pi2(&cbc_left, &cbc_right, work_key->key5);
			round_pi3(&cbc_left, &cbc_right, work_key->key6, work_key->key7);
			round_pi4(&cbc_left, &cbc_right, work_key->key8);
		}

		ALIGNAS(16) uint8_t remain[8];
		*(uint32_t*)(remain + 0) = cbc_left;
		*(uint32_t*)(remain + 4) = cbc_right;
		switch (remain_size) {
#ifdef _MSC_VER
		default: __assume(0);
#else
		default:
#endif
		case 7: p[6] ^= remain[5];
		case 6: p[5] ^= remain[6];
		case 5: p[4] ^= remain[7];
		case 4: p[3] ^= remain[0];
		case 3: p[2] ^= remain[1];
		case 2: p[1] ^= remain[2];
		case 1: p[0] ^= remain[3];
		}
	}
}

#endif	// ENABLE_MULTI2_SSE2

#ifdef ENABLE_MULTI2_SSSE3

void decrypt_multi2_with_ssse3(uint8_t * __restrict data, const uint32_t size,
							   const MULTI2_SIMD_SYS_KEY * __restrict work_key,
							   const MULTI2_SIMD_WORK_KEY * __restrict packed_work_key,
							   const MULTI2_SIMD_DATA_KEY * __restrict cbc_init)
{
#ifdef _MSC_VER
	__assume(size <= 184);
#endif

	uint8_t * __restrict p = data;
	__m128i cbc = _mm_set_epi32(0, 0, cbc_init->right, cbc_init->left);
	cbc = byte_swap_ssse3(&cbc);

	// 99% of TS packets which should be descrambled are 184 bytes
#ifdef _MSC_VER
	if (size == 184) {
#else
	if (__builtin_expect(size == 184, 1)) {
#endif
		// copy and zero-fill last 8 bytes, because this proccess descrambles 192 bytes
		ALIGNAS(16) uint8_t backup[8];
		memcpy(backup, data + 184, 8);
		memset(data + 184, 0, 8);

#ifndef OPTIMIZE_MULTI2_FOR_PIPELINE

		for (int i = 0; i < 6; i++) {
			__m128i src1, src2, left, right, x, y;

			// r2 l2 r1 l1
			src1 = _mm_loadu_si128((__m128i*)(p +  0));
			// r4 l4 r3 l3
			src2 = _mm_loadu_si128((__m128i*)(p + 16));

			// r2 r1 l2 l1
			x = _mm_shuffle_epi8(src1, src_swap_mask);
			// r4 r3 l4 l3
			y = _mm_shuffle_epi8(src2, src_swap_mask);

			// l4 l3 l2 l1
			left  = _mm_unpacklo_epi64(x, y);
			// r4 r3 r2 r1
			right = _mm_unpackhi_epi64(x, y);

#if defined(__INTEL_COMPILER) && MULTI2_SIMD_SCRAMBLE_ROUND <= 4
#pragma unroll(4)
#endif
			for (uint32_t i = 0; i < MAX_SCRAMBLE_ROUND; ++i) {
				round_pi4_ssse3(&left, &right, &(packed_work_key->key[7]));
				round_pi3_ssse3(&left, &right, &(packed_work_key->key[5]), &(packed_work_key->key[6]));
				round_pi2_ssse3(&left, &right, &(packed_work_key->key[4]));
				round_pi1_ssse3(&left, &right);
				round_pi4_ssse3(&left, &right, &(packed_work_key->key[3]));
				round_pi3_ssse3(&left, &right, &(packed_work_key->key[1]), &(packed_work_key->key[2]));
				round_pi2_ssse3(&left, &right, &(packed_work_key->key[0]));
				round_pi1_ssse3(&left, &right);
			}

			// r2 l2 r1 l1
			x = _mm_unpacklo_epi32(left, right);
			x = byte_swap_ssse3(&x);
			// r4 l4 r3 l3
			y = _mm_unpackhi_epi32(left, right);
			y = byte_swap_ssse3(&y);

			x = _mm_xor_si128(x, _mm_unpacklo_epi64(cbc, src1));
			cbc = _mm_shuffle_epi32(src2, MM_SHUFFLE4(1, 0, 3, 2));
			y = _mm_xor_si128(y, _mm_unpackhi_epi64(src1, cbc));

			_mm_storeu_si128((__m128i*)(p +  0), x);
			_mm_storeu_si128((__m128i*)(p + 16), y);

			p += 32;
		}

#else	// OPTIMIZE_MULTI2_FOR_PIPELINE

		// optimize for pipeline
		for (int i = 0; i < 2; ++i) {
			__m128i src1, src2, src3, src4, src5, src6;
			__m128i left1, right1, left2, right2, left3, right3;
			__m128i x1, y1, x2, y2, x3, y3;

			src1 = _mm_loadu_si128((__m128i*)(p +  0));
			src2 = _mm_loadu_si128((__m128i*)(p + 16));
			src3 = _mm_loadu_si128((__m128i*)(p + 32));
			src4 = _mm_loadu_si128((__m128i*)(p + 48));
			src5 = _mm_loadu_si128((__m128i*)(p + 64));
			src6 = _mm_loadu_si128((__m128i*)(p + 80));

			x1 = _mm_shuffle_epi8(src1, src_swap_mask);
			y1 = _mm_shuffle_epi8(src2, src_swap_mask);
			x2 = _mm_shuffle_epi8(src3, src_swap_mask);
			y2 = _mm_shuffle_epi8(src4, src_swap_mask);
			x3 = _mm_shuffle_epi8(src5, src_swap_mask);
			y3 = _mm_shuffle_epi8(src6, src_swap_mask);

			left1  = _mm_unpacklo_epi64(x1, y1);
			right1 = _mm_unpackhi_epi64(x1, y1);
			left2  = _mm_unpacklo_epi64(x2, y2);
			right2 = _mm_unpackhi_epi64(x2, y2);
			left3  = _mm_unpacklo_epi64(x3, y3);
			right3 = _mm_unpackhi_epi64(x3, y3);

#if defined(__INTEL_COMPILER) && MULTI2_SIMD_SCRAMBLE_ROUND <= 4
#pragma unroll(4)
#endif
			for (uint32_t i = 0U; i < MAX_SCRAMBLE_ROUND; ++i) {
				round_pi4_ssse3_with_3sets(&left1, &right1, &left2, &right2, &left3, &right3, &(packed_work_key->key[7]));
				round_pi3_ssse3_with_3sets(&left1, &right1, &left2, &right2, &left3, &right3,
										   &(packed_work_key->key[5]), &(packed_work_key->key[6]));
				round_pi2_ssse3_with_3sets(&left1, &right1, &left2, &right2, &left3, &right3, &(packed_work_key->key[4]));
				round_pi1_ssse3_with_3sets(&left1, &right1, &left2, &right2, &left3, &right3);
				round_pi4_ssse3_with_3sets(&left1, &right1, &left2, &right2, &left3, &right3, &(packed_work_key->key[3]));
				round_pi3_ssse3_with_3sets(&left1, &right1, &left2, &right2, &left3, &right3,
										   &(packed_work_key->key[1]), &(packed_work_key->key[2]));
				round_pi2_ssse3_with_3sets(&left1, &right1, &left2, &right2, &left3, &right3, &(packed_work_key->key[0]));
				round_pi1_ssse3_with_3sets(&left1, &right1, &left2, &right2, &left3, &right3);
			}

			x1 = _mm_unpacklo_epi32(left1, right1);
			y1 = _mm_unpackhi_epi32(left1, right1);
			x2 = _mm_unpacklo_epi32(left2, right2);
			y2 = _mm_unpackhi_epi32(left2, right2);
			x3 = _mm_unpacklo_epi32(left3, right3);
			y3 = _mm_unpackhi_epi32(left3, right3);

			x1 = byte_swap_ssse3(&x1);
			y1 = byte_swap_ssse3(&y1);
			x2 = byte_swap_ssse3(&x2);
			y2 = byte_swap_ssse3(&y2);
			x3 = byte_swap_ssse3(&x3);
			y3 = byte_swap_ssse3(&y3);

			src2 = _mm_shuffle_epi32(src2, MM_SHUFFLE4(1, 0, 3, 2));
			src4 = _mm_shuffle_epi32(src4, MM_SHUFFLE4(1, 0, 3, 2));
			src6 = _mm_shuffle_epi32(src6, MM_SHUFFLE4(1, 0, 3, 2));
			x1 = _mm_xor_si128(x1, _mm_unpacklo_epi64(cbc, src1));
			y1 = _mm_xor_si128(y1, _mm_unpackhi_epi64(src1, src2));
			x2 = _mm_xor_si128(x2, _mm_unpacklo_epi64(src2, src3));
			y2 = _mm_xor_si128(y2, _mm_unpackhi_epi64(src3, src4));
			x3 = _mm_xor_si128(x3, _mm_unpacklo_epi64(src4, src5));
			y3 = _mm_xor_si128(y3, _mm_unpackhi_epi64(src5, src6));
			cbc = src6;

			_mm_storeu_si128((__m128i*)(p +  0), x1);
			_mm_storeu_si128((__m128i*)(p + 16), y1);
			_mm_storeu_si128((__m128i*)(p + 32), x2);
			_mm_storeu_si128((__m128i*)(p + 48), y2);
			_mm_storeu_si128((__m128i*)(p + 64), x3);
			_mm_storeu_si128((__m128i*)(p + 80), y3);

			p += 32 * 3;
		}

#endif	// OPTIMIZE_MULTI2_FOR_PIPELINE

		// restore last 8 bytes from backup
		memcpy(data + 184, backup, 8);
		return;
	}

	// CBC mode
	for (uint8_t *ptr_end = p + (size & 0xFFFFFFE0UL); p < ptr_end; p += 32) {
		__m128i src1, src2, left, right, x, y;

		// r2 l2 r1 l1
		src1 = _mm_loadu_si128((__m128i*)p);
		// r4 l4 r3 l3
		src2 = _mm_loadu_si128((__m128i*)(p + 16));

		// r2 r1 l2 l1
		x = _mm_shuffle_epi8(src1, src_swap_mask);
		// r4 r3 l4 l3
		y = _mm_shuffle_epi8(src2, src_swap_mask);

		// l4 l3 l2 l1
		left  = _mm_unpacklo_epi64(x, y);
		// r4 r3 r2 r1
		right = _mm_unpackhi_epi64(x, y);

		for (uint32_t i = 0U; i < MAX_SCRAMBLE_ROUND; ++i) {
			round_pi4_ssse3(&left, &right, &(packed_work_key->key[7]));
			round_pi3_ssse3(&left, &right, &(packed_work_key->key[5]), &(packed_work_key->key[6]));
			round_pi2_ssse3(&left, &right, &(packed_work_key->key[4]));
			round_pi1_ssse3(&left, &right);
			round_pi4_ssse3(&left, &right, &(packed_work_key->key[3]));
			round_pi3_ssse3(&left, &right, &(packed_work_key->key[1]), &(packed_work_key->key[2]));
			round_pi2_ssse3(&left, &right, &(packed_work_key->key[0]));
			round_pi1_ssse3(&left, &right);
		}

		// r2 l2 r1 l1
		x = _mm_unpacklo_epi32(left, right);
		x = byte_swap_ssse3(&x);
		// r4 l4 r3 l3
		y = _mm_unpackhi_epi32(left, right);
		y = byte_swap_ssse3(&y);

		x = _mm_xor_si128(x, _mm_unpacklo_epi64(cbc, src1));
		cbc = _mm_shuffle_epi32(src2, MM_SHUFFLE4(1, 0, 3, 2));
		y = _mm_xor_si128(y, _mm_unpackhi_epi64(src1, cbc));

		_mm_storeu_si128((__m128i*)p,        x);
		_mm_storeu_si128((__m128i*)(p + 16), y);
	}

	uint32_t cbc_left, cbc_right;
	ALIGNAS(16) uint32_t temp_data[4];
	_mm_storeu_si128((__m128i*)temp_data, byte_swap_ssse3(&cbc));
	cbc_left  = temp_data[0];
	cbc_right = temp_data[1];

	for (uint8_t *ptr_end = p + (size & 0x00000018UL); p < ptr_end; p += 8) {
		uint32_t src1, src2, left, right;

		src1 = _byteswap_ulong(*(uint32_t*)(p + 0));
		src2 = _byteswap_ulong(*(uint32_t*)(p + 4));
		left  = src1;
		right = src2;

		for (uint32_t round = 0U; round < MAX_SCRAMBLE_ROUND; ++round) {
			round_pi4(&left, &right, work_key->key8);
			round_pi3(&left, &right, work_key->key6, work_key->key7);
			round_pi2(&left, &right, work_key->key5);
			round_pi1(&left, &right);
			round_pi4(&left, &right, work_key->key4);
			round_pi3(&left, &right, work_key->key2, work_key->key3);
			round_pi2(&left, &right, work_key->key1);
			round_pi1(&left, &right);
		}

		*(uint32_t*)(p + 0) = _byteswap_ulong(left  ^ cbc_left);
		*(uint32_t*)(p + 4) = _byteswap_ulong(right ^ cbc_right);
		cbc_left  = src1;
		cbc_right = src2;
	}

	// OFB mode
	uint32_t remain_size = size & 0x00000007UL;
	if (remain_size) {
		for (uint32_t round = 0U; round < MAX_SCRAMBLE_ROUND; ++round) {
			round_pi1(&cbc_left, &cbc_right);
			round_pi2(&cbc_left, &cbc_right, work_key->key1);
			round_pi3(&cbc_left, &cbc_right, work_key->key2, work_key->key3);
			round_pi4(&cbc_left, &cbc_right, work_key->key4);
			round_pi1(&cbc_left, &cbc_right);
			round_pi2(&cbc_left, &cbc_right, work_key->key5);
			round_pi3(&cbc_left, &cbc_right, work_key->key6, work_key->key7);
			round_pi4(&cbc_left, &cbc_right, work_key->key8);
		}

		ALIGNAS(16) uint8_t remain[8];
		*(uint32_t*)(remain + 0) = cbc_left;
		*(uint32_t*)(remain + 4) = cbc_right;
		switch (remain_size) {
#ifdef _MSC_VER
		default: __assume(0);
#else
		default:
#endif
		case 7: p[6] ^= remain[5];
		case 6: p[5] ^= remain[6];
		case 5: p[4] ^= remain[7];
		case 4: p[3] ^= remain[0];
		case 3: p[2] ^= remain[1];
		case 2: p[1] ^= remain[2];
		case 1: p[0] ^= remain[3];
		}
	}
}

#endif	// ENABLE_MULTI2_SSSE3

#ifdef ENABLE_MULTI2_AVX2

void decrypt_multi2_with_avx2(uint8_t * __restrict data, const uint32_t size,
							  const MULTI2_SIMD_SYS_KEY * __restrict work_key,
							  const MULTI2_SIMD_WORK_KEY * __restrict packed_work_key,
							  const MULTI2_SIMD_DATA_KEY * __restrict cbc_init)
{
#ifdef _MSC_VER
	__assume(size <= 184);
#endif

	uint8_t * __restrict p = data;
	__m256i cbc = _mm256_set_epi32(0, 0, 0, 0, 0, 0, cbc_init->right, cbc_init->left);
	cbc = byte_swap_avx2(&cbc);

	// 99% of TS packets which should be descrambled are 184 bytes
#ifdef _MSC_VER
	if (size == 184) {
#else
	if (__builtin_expect(size == 184, 1)) {
#endif

#ifndef OPTIMIZE_MULTI2_FOR_PIPELINE

		for (int i = 0; i < 3; ++i) {
			__m256i src1, src2, src3, left, right, x, y;

			// r4 l4 r3 l3 r2 l2 r1 l1
			src1 = _mm256_loadu_si256((__m256i*)(p +  0));
			// r8 l8 r7 l7 r6 l6 r5 l5
			src2 = _mm256_loadu_si256((__m256i*)(p + 32));
			// r7 l7 r6 l6 r5 l5 r4 l4
			src3 = _mm256_loadu_si256((__m256i*)(p + 32 - 8));

			// r4 r3 l4 l3 r2 r1 l2 l1
			x = _mm256_shuffle_epi8(src1, src_swap_mask_avx2);
			// r8 r7 l8 l7 r6 r5 l6 l5
			y = _mm256_shuffle_epi8(src2, src_swap_mask_avx2);

			// l8 l7 l6 l5 l4 l3 l2 l1
			left  = _mm256_unpacklo_epi64(x, y);
			// r8 r7 r6 r5 r4 r3 r2 r1
			right = _mm256_unpackhi_epi64(x, y);

#if defined(__INTEL_COMPILER) && MULTI2_SIMD_SCRAMBLE_ROUND <= 4
#pragma unroll(4)
#endif
			for (uint32_t i = 0; i < MAX_SCRAMBLE_ROUND; ++i) {
				round_pi4_avx2(&left, &right, &(packed_work_key->key256[7]));
				round_pi3_avx2(&left, &right, &(packed_work_key->key256[5]), &(packed_work_key->key256[6]));
				round_pi2_avx2(&left, &right, &(packed_work_key->key256[4]));
				round_pi1_avx2(&left, &right);
				round_pi4_avx2(&left, &right, &(packed_work_key->key256[3]));
				round_pi3_avx2(&left, &right, &(packed_work_key->key256[1]), &(packed_work_key->key256[2]));
				round_pi2_avx2(&left, &right, &(packed_work_key->key256[0]));
				round_pi1_avx2(&left, &right);
			}

			// r4 l4 r3 l3 r2 l2 r1 l1
			x = _mm256_unpacklo_epi32(left, right);
			x = byte_swap_avx2(&x);
			// r8 l8 r7 l7 r6 l6 r5 l5
			y = _mm256_unpackhi_epi32(left, right);
			y = byte_swap_avx2(&y);

			x = _mm256_xor_si256(x, _mm256_or_si256(cbc, shift_leftsi64_m256i(src1)));
			//y = _mm256_xor_si256(y, _mm256_or_si256(shift_rightsi192_m256i(src1), shift_leftsi64_m256i(src2)));
			y = _mm256_xor_si256(y, src3);
			cbc = shift_rightsi192_m256i(src2);

			_mm256_storeu_si256((__m256i*)(p +  0), x);
			_mm256_storeu_si256((__m256i*)(p + 32), y);

			p += 64;
		}

#else	// OPTIMIZE_MULTI2_FOR_PIPELINE

		// optimize for pipeline
		__m256i src1, src2, src3, src4, src5, src6;
		__m256i left1, right1, left2, right2, left3, right3;
		__m256i x1, y1, x2, y2, x3, y3;

		src1 = _mm256_loadu_si256((__m256i*)(p +   0));
		src2 = _mm256_loadu_si256((__m256i*)(p +  32));
		src3 = _mm256_loadu_si256((__m256i*)(p +  64));
		src4 = _mm256_loadu_si256((__m256i*)(p +  96));
		src5 = _mm256_loadu_si256((__m256i*)(p + 128 - 8));
		src6 = _mm256_loadu_si256((__m256i*)(p + 160 - 8));

		x1 = _mm256_shuffle_epi8(src1, src_swap_mask_avx2);
		y1 = _mm256_shuffle_epi8(src2, src_swap_mask_avx2);
		x2 = _mm256_shuffle_epi8(src3, src_swap_mask_avx2);
		y2 = _mm256_shuffle_epi8(src4, src_swap_mask_avx2);
		x3 = _mm256_shuffle_epi8(src5, src_swap_mask_avx2);
		y3 = _mm256_shuffle_epi8(src6, src_swap_mask_avx2);

		left1  = _mm256_unpacklo_epi64(x1, y1);
		right1 = _mm256_unpackhi_epi64(x1, y1);
		left2  = _mm256_unpacklo_epi64(x2, y2);
		right2 = _mm256_unpackhi_epi64(x2, y2);
		left3  = _mm256_unpacklo_epi64(x3, y3);
		right3 = _mm256_unpackhi_epi64(x3, y3);

#if defined(__INTEL_COMPILER) && MULTI2_SIMD_SCRAMBLE_ROUND <= 4
#pragma unroll(4)
#endif
		for (uint32_t i = 0U; i < MAX_SCRAMBLE_ROUND; ++i) {
			round_pi4_avx2_with_3sets(&left1, &right1, &left2, &right2, &left3, &right3, &(packed_work_key->key256[7]));
			round_pi3_avx2_with_3sets(&left1, &right1, &left2, &right2, &left3, &right3,
									  &(packed_work_key->key256[5]), &(packed_work_key->key256[6]));
			round_pi2_avx2_with_3sets(&left1, &right1, &left2, &right2, &left3, &right3, &(packed_work_key->key256[4]));
			round_pi1_avx2_with_3sets(&left1, &right1, &left2, &right2, &left3, &right3);
			round_pi4_avx2_with_3sets(&left1, &right1, &left2, &right2, &left3, &right3, &(packed_work_key->key256[3]));
			round_pi3_avx2_with_3sets(&left1, &right1, &left2, &right2, &left3, &right3,
									  &(packed_work_key->key256[1]), &(packed_work_key->key256[2]));
			round_pi2_avx2_with_3sets(&left1, &right1, &left2, &right2, &left3, &right3, &(packed_work_key->key256[0]));
			round_pi1_avx2_with_3sets(&left1, &right1, &left2, &right2, &left3, &right3);
		}

		x1 = _mm256_unpacklo_epi32(left1, right1);
		y1 = _mm256_unpackhi_epi32(left1, right1);
		x2 = _mm256_unpacklo_epi32(left2, right2);
		y2 = _mm256_unpackhi_epi32(left2, right2);
		x3 = _mm256_unpacklo_epi32(left3, right3);
		y3 = _mm256_unpackhi_epi32(left3, right3);

		x1 = byte_swap_avx2(&x1);
		y1 = byte_swap_avx2(&y1);
		x2 = byte_swap_avx2(&x2);
		y2 = byte_swap_avx2(&y2);
		x3 = byte_swap_avx2(&x3);
		y3 = byte_swap_avx2(&y3);

		/* bit shift version *
		x1 = _mm256_xor_si256(x1, _mm256_or_si256(cbc, shift_leftsi64_m256i(src1)));
		y1 = _mm256_xor_si256(y1, _mm256_or_si256(shift_rightsi192_m256i(src1), shift_leftsi64_m256i(src2)));
		x2 = _mm256_xor_si256(x2, _mm256_or_si256(shift_rightsi192_m256i(src2), shift_leftsi64_m256i(src3)));
		y2 = _mm256_xor_si256(y2, _mm256_or_si256(shift_rightsi192_m256i(src3), shift_leftsi64_m256i(src4)));
		x3 = _mm256_xor_si256(x3, _mm256_or_si256(shift_rightsi192_m256i(src4), shift_leftsi64_m256i(src5)));
		y3 = _mm256_xor_si256(y3, _mm256_or_si256(shift_rightsi192_m256i(src5), shift_leftsi64_m256i(src6)));
		*/
		/* shifted load version */
		src2 = _mm256_loadu_si256((__m256i*)(p +  32 - 8));
		src3 = _mm256_loadu_si256((__m256i*)(p +  64 - 8));
		src4 = _mm256_loadu_si256((__m256i*)(p +  96 - 8));
		src5 = _mm256_loadu_si256((__m256i*)(p + 128 - 8 - 8));
		src6 = _mm256_loadu_si256((__m256i*)(p + 160 - 8 - 8));
		x1 = _mm256_xor_si256(x1, _mm256_or_si256(cbc, shift_leftsi64_m256i(src1)));
		y1 = _mm256_xor_si256(y1, src2);
		x2 = _mm256_xor_si256(x2, src3);
		y2 = _mm256_xor_si256(y2, src4);
		x3 = _mm256_xor_si256(x3, src5);
		y3 = _mm256_xor_si256(y3, src6);

		_mm256_storeu_si256((__m256i*)(p +  0),      x1);
		_mm256_storeu_si256((__m256i*)(p + 32),      y1);
		_mm256_storeu_si256((__m256i*)(p + 64),      x2);
		_mm256_storeu_si256((__m256i*)(p + 96),      y2);
		_mm256_storeu_si256((__m256i*)(p + 128 - 8), x3);
		_mm256_storeu_si256((__m256i*)(p + 160 - 8), y3);

#endif	// OPTIMIZE_MULTI2_FOR_PIPELINE

		return;
	}

	// CBC mode
	for (uint8_t *ptr_end = p + (size & 0xFFFFFFC0UL); p < ptr_end; p += 64) {
		__m256i src1, src2, src3, left, right, x, y;

		// r4 l4 r3 l3 r2 l2 r1 l1
		src1 = _mm256_loadu_si256((__m256i*)p);
		// r8 l8 r7 l7 r6 l6 r5 l5
		src2 = _mm256_loadu_si256((__m256i*)(p + 32));
		// r7 l7 r6 l6 r5 l5 r4 l4
		src3 = _mm256_loadu_si256((__m256i*)(p + 32 - 8));

		// r4 r3 l4 l3 r2 r1 l2 l1
		x = _mm256_shuffle_epi8(src1, src_swap_mask_avx2);
		// r8 r7 l8 l7 r6 r5 l6 l5
		y = _mm256_shuffle_epi8(src2, src_swap_mask_avx2);

		// l8 l7 l6 l5 l4 l3 l2 l1
		left  = _mm256_unpacklo_epi64(x, y);
		// r8 r7 r6 r5 r4 r3 r2 r1
		right = _mm256_unpackhi_epi64(x, y);

		for (uint32_t i = 0U; i < MAX_SCRAMBLE_ROUND; ++i) {
			round_pi4_avx2(&left, &right, &(packed_work_key->key256[7]));
			round_pi3_avx2(&left, &right, &(packed_work_key->key256[5]), &(packed_work_key->key256[6]));
			round_pi2_avx2(&left, &right, &(packed_work_key->key256[4]));
			round_pi1_avx2(&left, &right);
			round_pi4_avx2(&left, &right, &(packed_work_key->key256[3]));
			round_pi3_avx2(&left, &right, &(packed_work_key->key256[1]), &(packed_work_key->key256[2]));
			round_pi2_avx2(&left, &right, &(packed_work_key->key256[0]));
			round_pi1_avx2(&left, &right);
		}

		// r4 l4 r3 l3 r2 l2 r1 l1
		x = _mm256_unpacklo_epi32(left, right);
		x = byte_swap_avx2(&x);
		// r8 l8 r7 l7 r6 l6 r5 l5
		y = _mm256_unpackhi_epi32(left, right);
		y = byte_swap_avx2(&y);

		x = _mm256_xor_si256(x, _mm256_or_si256(cbc, shift_leftsi64_m256i(src1)));
		//y = _mm256_xor_si256(y, _mm256_or_si256(shift_rightsi192_m256i(src1), shift_leftsi64_m256i(src2)));
		y = _mm256_xor_si256(y, src3);
		cbc = shift_rightsi192_m256i(src2);

		_mm256_storeu_si256((__m256i*)p,        x);
		_mm256_storeu_si256((__m256i*)(p + 32), y);
	}

	/*__m128i cbc128;
	cbc128 = _mm256_castsi256_si128(cbc);
	if (p < p + (size & 0x00000020UL)) {
		__m128i src1, src2, left, right, x, y;

		// r2 l2 r1 l1
		src1 = _mm_loadu_si128((__m128i*)p);
		// r4 l4 r3 l3
		src2 = _mm_loadu_si128((__m128i*)(p + 16));

		// r2 r1 l2 l1
		x = _mm_shuffle_epi8(src1, src_swap_mask);
		// r4 r3 l4 l3
		y = _mm_shuffle_epi8(src2, src_swap_mask);

		// l4 l3 l2 l1
		left  = _mm_unpacklo_epi64(x, y);
		// r4 r3 r2 r1
		right = _mm_unpackhi_epi64(x, y);

		for (uint32_t i = 0U; i < MAX_SCRAMBLE_ROUND; ++i) {
			round_pi4_ssse3(&left, &right, &(packed_work_key->key[7]));
			round_pi3_ssse3(&left, &right, &(packed_work_key->key[5]), &(packed_work_key->key[6]));
			round_pi2_ssse3(&left, &right, &(packed_work_key->key[4]));
			round_pi1_ssse3(&left, &right);
			round_pi4_ssse3(&left, &right, &(packed_work_key->key[3]));
			round_pi3_ssse3(&left, &right, &(packed_work_key->key[1]), &(packed_work_key->key[2]));
			round_pi2_ssse3(&left, &right, &(packed_work_key->key[0]));
			round_pi1_ssse3(&left, &right);
		}

		// r2 l2 r1 l1
		x = _mm_unpacklo_epi32(left, right);
		x = byte_swap_ssse3(&x);
		// r4 l4 r3 l3
		y = _mm_unpackhi_epi32(left, right);
		y = byte_swap_ssse3(&y);

		x = _mm_xor_si128(x, _mm_unpacklo_epi64(cbc128, src1));
		cbc128 = _mm_shuffle_epi32(src2, MM_SHUFFLE4(1, 0, 3, 2));
		y = _mm_xor_si128(y, _mm_unpackhi_epi64(src1, cbc128));

		_mm_storeu_si128((__m128i*)p,        x);
		_mm_storeu_si128((__m128i*)(p + 16), y);

		p += 32;
	}

	uint32_t cbc_left, cbc_right;
	ALIGNAS(16) uint32_t temp_data[4];
	_mm_storeu_si128((__m128i*)temp_data, byte_swap_ssse3(&cbc128));*/
	uint32_t cbc_left, cbc_right;
	ALIGNAS(32) uint32_t temp_data[8];
	_mm256_storeu_si256((__m256i*)temp_data, byte_swap_avx2(&cbc));
	cbc_left  = temp_data[0];
	cbc_right = temp_data[1];

	//for (uint8_t *ptr_end = p + (size & 0x00000018UL); p < ptr_end; p += 8) {
	for (uint8_t *ptr_end = p + (size & 0x00000038UL); p < ptr_end; p += 8) {
		uint32_t src1, src2, left, right;

		src1 = _byteswap_ulong(*(uint32_t*)(p + 0));
		src2 = _byteswap_ulong(*(uint32_t*)(p + 4));
		left  = src1;
		right = src2;

		for (uint32_t round = 0U; round < MAX_SCRAMBLE_ROUND; ++round) {
			round_pi4(&left, &right, work_key->key8);
			round_pi3(&left, &right, work_key->key6, work_key->key7);
			round_pi2(&left, &right, work_key->key5);
			round_pi1(&left, &right);
			round_pi4(&left, &right, work_key->key4);
			round_pi3(&left, &right, work_key->key2, work_key->key3);
			round_pi2(&left, &right, work_key->key1);
			round_pi1(&left, &right);
		}

		*(uint32_t*)(p + 0) = _byteswap_ulong(left  ^ cbc_left);
		*(uint32_t*)(p + 4) = _byteswap_ulong(right ^ cbc_right);
		cbc_left  = src1;
		cbc_right = src2;
	}

	// OFB mode
	uint32_t remain_size = size & 0x00000007UL;
	if (remain_size) {
		for (uint32_t round = 0U; round < MAX_SCRAMBLE_ROUND; ++round) {
			round_pi1(&cbc_left, &cbc_right);
			round_pi2(&cbc_left, &cbc_right, work_key->key1);
			round_pi3(&cbc_left, &cbc_right, work_key->key2, work_key->key3);
			round_pi4(&cbc_left, &cbc_right, work_key->key4);
			round_pi1(&cbc_left, &cbc_right);
			round_pi2(&cbc_left, &cbc_right, work_key->key5);
			round_pi3(&cbc_left, &cbc_right, work_key->key6, work_key->key7);
			round_pi4(&cbc_left, &cbc_right, work_key->key8);
		}

		ALIGNAS(32) uint8_t remain[8];
		*(uint32_t*)(remain + 0) = cbc_left;
		*(uint32_t*)(remain + 4) = cbc_right;
		switch (remain_size) {
#ifdef _MSC_VER
		default: __assume(0);
#else
		default:
#endif
		case 7: p[6] ^= remain[5];
		case 6: p[5] ^= remain[6];
		case 5: p[4] ^= remain[7];
		case 4: p[3] ^= remain[0];
		case 3: p[2] ^= remain[1];
		case 2: p[1] ^= remain[2];
		case 1: p[0] ^= remain[3];
		}
	}
}

#endif	// ENABLE_MULTI2_SSSE3
