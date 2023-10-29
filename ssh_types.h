#ifndef SSH_TYPES_H
#define SSH_TYPES_H

#include <stdint.h>

typedef struct ssh_u8_array
{
	size_t size;
	uint8_t *data;
} ssh_u8_array;

#endif
