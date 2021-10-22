#ifndef MULTI2_H
#define MULTI2_H

#include "portable.h"
#include "simd_instruction_type.h"

typedef struct {

	void *private_data;

	void (* release)(void *m2);
	int (* add_ref)(void *m2);

	int (* set_round)(void *m2, int32_t val);
	int (* set_simd)(void *m2, enum INSTRUCTION_TYPE);

	int (* set_system_key)(void *m2, uint8_t *val);
	int (* set_init_cbc)(void *m2, uint8_t *val);
	int (* set_scramble_key)(void *m2, uint8_t *val);
	int (* clear_scramble_key)(void *m2);

	int (* encrypt)(void *m2, int32_t type, uint8_t *buf, int32_t size);
#ifdef ENABLE_MULTI2_SIMD
	int (* decrypt)(void *m2, int32_t type, uint8_t *buf, intptr_t size);
#else
	int (* decrypt)(void *m2, int32_t type, uint8_t *buf, int32_t size);
#endif

} MULTI2;

#ifdef __cplusplus
extern "C" {
#endif

extern MULTI2 *create_multi2(void);

#ifdef __cplusplus
}
#endif

#endif /* MULTI2_H */
