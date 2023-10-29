#include <string.h>
#include <stdlib.h>
#include "ssh.h"
#include "ssh_log.h"

#include "ssh_log.c"

int alloc_ssh_string(ssh_u8_array in_data, ssh_u8_array *out_data)
{
	// ssh strings are raw binary data, but preceded by 4 bytes to specify the number of bytes in the data.
	size_t size = in_data.size + 4;
	uint8_t *new_data = (uint8_t *)calloc(size, sizeof(uint8_t));
	if (new_data == NULL)
	{
		ssh_log_error("failed to allocate memory for ssh string");
		return -1;
	}

	if (memcpy_s(new_data+4, size-4, in_data.data, in_data.size) != 0)
	{
		ssh_log_error("failed to copy data into allocated memory for ssh string");
		free(new_data);
		return -1;
	}

	((uint32_t *)new_data)[0] = (uint32_t)size;
	out_data->size = size;
	out_data->data = new_data;
	return 0;
}
