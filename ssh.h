#ifndef SSH_H
#define SSH_H

#include "ssh_types.h"

const size_t MAX_IDENTIFICATION_STRING_LENGTH = 255;

int alloc_ssh_string(ssh_u8_array in_data, ssh_u8_array *out_data);

#endif
