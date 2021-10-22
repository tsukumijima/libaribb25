#ifndef MULTI2_SIMD_H
#define MULTI2_SIMD_H

#include <stdint.h>
#include <stdbool.h>
#ifdef _MSC_VER
#include <intrin.h>
#else
#include <x86intrin.h>
#endif

#include "portable.h"
#include "simd_instruction_type.h"

#define USE_MULTI2_INTRINSIC	// use intrinsic functions
// #define ENABLE_MULTI2_SIMD		// enable SIMD instructions

#ifdef ENABLE_MULTI2_SIMD

#define ENABLE_MULTI2_SSE2		// enable SSE2 instructions
#define ENABLE_MULTI2_SSSE3		// enable SSSE3 instructions

#ifdef ENABLE_MULTI2_SSSE3
#define ENABLE_MULTI2_AVX2		// enable AVX2 instructions
#endif

//#define USE_MULTI2_SIMD_ICC	// use Intel C++ Compiler

#endif	// ENABLE_MULTI2_SIMD


#ifdef ENABLE_MULTI2_AVX2

typedef union {
	__m256i key256[8];
	__m128i key[8];
} MULTI2_SIMD_WORK_KEY;

#else

typedef struct {
	__m128i key[8];
} MULTI2_SIMD_WORK_KEY;

#endif

typedef struct {
	union {
//#if !defined(USE_MULTI2_INTRINSIC) || !defined(_M_X64)
#if defined(_M_X64) || !defined(USE_MULTI2_INTRINSIC) || !defined(_M_X64)
		struct {
			uint32_t key1, key2, key3, key4, key5, key6, key7, key8;
		};
#else
		struct {
			uint32_t key2, key1, key4, key3, key6, key5, key8, key7;
		};
		uint64_t data64[4];
#endif
		uint8_t data[32];
	};
} MULTI2_SIMD_SYS_KEY /* system key(Sk), expanded key(Wk) 256bit */;

typedef struct {
	union {
		struct {
			uint32_t right, left;
		};
		uint64_t data64;
		uint8_t data[8];
	};
} MULTI2_SIMD_DATA_KEY /* data key(Dk) 64bit */;

typedef struct {

	MULTI2_SIMD_WORK_KEY wrk[2]; /* 0: odd, 1: even */
	void (* decrypt)(uint8_t * __restrict data, const uint32_t size,
					 const MULTI2_SIMD_SYS_KEY * __restrict work_key,
					 const MULTI2_SIMD_WORK_KEY * __restrict packed_work_key,
					 const MULTI2_SIMD_DATA_KEY * __restrict cbc_init);

} MULTI2_SIMD_DATA /* data set for SIMD */;


#ifdef __cplusplus
extern "C" {
#endif

extern bool is_simd_enabled();
extern bool is_sse2_available();
extern bool is_ssse3_available();
extern bool is_avx2_available();
extern bool initialize_multi2_simd(enum INSTRUCTION_TYPE instruction, void* m2);

extern void set_simd_instruction(enum INSTRUCTION_TYPE instruction);
extern enum INSTRUCTION_TYPE get_simd_instruction();
extern enum INSTRUCTION_TYPE get_supported_simd_instruction();

extern void alloc_work_key_for_simd(MULTI2_SIMD_WORK_KEY **work_key_odd, MULTI2_SIMD_WORK_KEY **work_key_even);
extern void free_work_key_for_simd(MULTI2_SIMD_WORK_KEY **work_key_odd, MULTI2_SIMD_WORK_KEY **work_key_even);
extern void set_work_key_for_simd(MULTI2_SIMD_WORK_KEY *work_key, const MULTI2_SIMD_SYS_KEY *src_key);
extern void set_work_key_for_avx2(MULTI2_SIMD_WORK_KEY *work_key, const MULTI2_SIMD_SYS_KEY *src_key);
extern void set_round_for_simd(const uint32_t round);
extern void set_system_key_with_bswap(MULTI2_SIMD_SYS_KEY *sys_key, const uint8_t *hex_data);
extern void get_system_key_with_bswap(const MULTI2_SIMD_SYS_KEY *sys_key, uint8_t *hex_data);
extern void set_data_key_with_bswap(MULTI2_SIMD_DATA_KEY *data_key, const uint8_t *hex_data);
extern void get_data_key_with_bswap(const MULTI2_SIMD_DATA_KEY *data_key, uint8_t *hex_data);

extern void decrypt_multi2_without_simd(uint8_t * __restrict data, const uint32_t size,
										const MULTI2_SIMD_SYS_KEY * __restrict work_key,
										const MULTI2_SIMD_WORK_KEY * __restrict packed_work_key,
										const MULTI2_SIMD_DATA_KEY * __restrict cbc_init);
#ifdef ENABLE_MULTI2_SSE2
extern void decrypt_multi2_with_sse2(uint8_t * __restrict data, const uint32_t size,
									 const MULTI2_SIMD_SYS_KEY * __restrict work_key,
									 const MULTI2_SIMD_WORK_KEY * __restrict packed_work_key,
									 const MULTI2_SIMD_DATA_KEY * __restrict cbc_init);
#endif
#ifdef ENABLE_MULTI2_SSSE3
extern void decrypt_multi2_with_ssse3(uint8_t * __restrict data, const uint32_t size,
									  const MULTI2_SIMD_SYS_KEY * __restrict work_key,
									  const MULTI2_SIMD_WORK_KEY * __restrict packed_work_key,
									  const MULTI2_SIMD_DATA_KEY * __restrict cbc_init);
#endif
#ifdef ENABLE_MULTI2_AVX2
extern void decrypt_multi2_with_avx2(uint8_t * __restrict data, const uint32_t size,
									 const MULTI2_SIMD_SYS_KEY * __restrict work_key,
									 const MULTI2_SIMD_WORK_KEY * __restrict packed_work_key,
									 const MULTI2_SIMD_DATA_KEY * __restrict cbc_init);
#endif

#ifdef __cplusplus
}
#endif

#endif /* MULTI2_SIMD_H */
