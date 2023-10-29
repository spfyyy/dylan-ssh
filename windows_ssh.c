#pragma comment(lib, "Ws2_32.lib")

#include "windows_ssh.h"
#include "ssh_config.h"

#include "ssh.c"

// utility functions
static int initialize_ssh_connection(SOCKET *out_ssh_socket);
static int exchange_identification_strings(SOCKET in_ssh_socket);
static int read_identification_string(SOCKET in_ssh_socket);
static int send_data(SOCKET in_ssh_socket, ssh_u8_array in_data);
static int read_string(SOCKET in_ssh_socket, ssh_u8_array *out_string);
static int cleanup(SOCKET in_ssh_socket);

int main(int argc, char **argv)
{
	int ret_value = 0;

	WSADATA wsa_data;
	if (WSAStartup(WINSOCK_VERSION, &wsa_data) != 0)
	{
		ssh_log_error("could not initialize windows sockets");
		ret_value = -1;
		return ret_value;
	}

	SOCKET ssh_socket;
	if (initialize_ssh_connection(&ssh_socket) != 0)
	{
		ssh_log_error("could not initialize ssh connection");
		ret_value = -1;
		goto cleanup;
	}

	if (exchange_identification_strings(ssh_socket) != 0)
	{
		ssh_log_error("could not exchange identification strings");
		ret_value = -1;
	}

	// todo: key exchange...

	cleanup:
	if (cleanup(ssh_socket) != 0)
	{
		ssh_log_error("could not clean up ssh socket");
		ret_value = -1;
	}

	return ret_value;
}

static int initialize_ssh_connection(SOCKET *out_ssh_socket)
{
	*out_ssh_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (*out_ssh_socket == INVALID_SOCKET)
	{
		ssh_log_error("unable to open ssh socket, error: %d", WSAGetLastError());
		return -1;
	}

	SOCKADDR_IN remote_address;
	remote_address.sin_family = AF_INET;
	remote_address.sin_addr.s_addr = inet_addr(SSH_CONF_IP);
	remote_address.sin_port = htons(SSH_CONF_PORT);

	if (connect(*out_ssh_socket, (SOCKADDR *) &remote_address, sizeof(remote_address)) == SOCKET_ERROR)
	{
		ssh_log_error("unable to connect to remote, error: %d", WSAGetLastError());
		if (closesocket(*out_ssh_socket) == SOCKET_ERROR)
		{
			ssh_log_error("unable to close ssh socket, error: %d", WSAGetLastError());
		}
		return -1;
	}

	return 0;
}

static int exchange_identification_strings(SOCKET in_ssh_socket)
{
	ssh_u8_array identification_string;
	identification_string.data = "SSH-2.0-dylanssh_win_1.0\r\n";
	identification_string.size = 26;
	if (send_data(in_ssh_socket, identification_string) != 0)
	{
		ssh_log_error("failed to send identification string");
		return -1;
	}

	ssh_log_debug("sent identification string: %s", identification_string.data);

	if (read_identification_string(in_ssh_socket) != 0)
	{
		ssh_log_error("failed to read remote identification string");
		return -1;
	}
	return 0;
}

static int send_data(SOCKET in_ssh_socket, ssh_u8_array in_data)
{
	char *data_pointer = (char *)(in_data.data);
	int remaining_size = (int)(in_data.size);
	while (remaining_size > 0)
	{
		int sendResult = send(in_ssh_socket, data_pointer, remaining_size, 0);
		if (sendResult == SOCKET_ERROR)
		{
			ssh_log_error("error sending %d bytes of data over socket, error: %d", remaining_size, WSAGetLastError());
			return -1;
		}

		remaining_size -= sendResult;
		data_pointer += sendResult;
	}

	return 0;
}

static int read_identification_string(SOCKET in_ssh_socket)
{
	char *identification_string = (char *)(calloc(MAX_IDENTIFICATION_STRING_LENGTH+1, sizeof(char)));
	if (identification_string == NULL)
	{
		ssh_log_error("could not allocate memory for storing remote identification string");
		return -1;
	}

	while (1) {
		int32_t encountered_cr_and_newline = 0;
		int length = 0;
		char last_character = -1;
		for (int i = 0; i < MAX_IDENTIFICATION_STRING_LENGTH; ++i) {
			char c;
			int read_result = recv(in_ssh_socket, &c, 1, 0);
			if (read_result == SOCKET_ERROR)
			{
				ssh_log_error("error while trying to read line from socket, error: %d, progress: %s", WSAGetLastError(), identification_string);
				free(identification_string);
				return -1;
			}
			else if (read_result == 0)
			{
				ssh_log_error("socket closed while trying to read line, progress: %s", identification_string);
				free(identification_string);
				return -1;
			}
			identification_string[i] = c;
			if (last_character == '\r' && c == '\n')
			{
				length = i+1;
				encountered_cr_and_newline = 1;
				break;
			}
			last_character = c;
		}

		if (!encountered_cr_and_newline)
		{
			ssh_log_error("reached end of line buffer without reading CR and newline, progress: %s", identification_string);
			free(identification_string);
			return -1;
		}

		char *line = identification_string;
		if (length >= 4 && line[0] == 'S' && line[1] == 'S' && line[2] == 'H' && line[3] == '-')
		{
			ssh_log_debug("received identification string: %s", identification_string);
			break;
		}
		else
		{
			ssh_log_debug("%s", identification_string);
		}
	}

	free(identification_string);
	return 0;
}

static int read_string(SOCKET in_ssh_socket, ssh_u8_array *out_string)
{
	uint32_t size;
	char *data_pointer = (char *)(&size);
	int remaining_size = sizeof(size);
	while (remaining_size > 0)
	{
		int read_result = recv(in_ssh_socket, data_pointer, remaining_size, 0);
		if (read_result == SOCKET_ERROR)
		{
			ssh_log_error("could not read string size from socket, error: %d", WSAGetLastError());
			return -1;
		}
		if (read_result == 0)
		{
			ssh_log_error("socket closed while trying to read string size");
			return -1;
		}
		ssh_log_debug("read %d bytes from socket", read_result);
		remaining_size -= read_result;
		data_pointer += read_result;
	}

	ssh_log_debug("about to read string from socket of size %d bytes", size);
	uint8_t *new_data = (uint8_t *)calloc(size, sizeof(uint8_t));
	if (new_data == NULL)
	{
		ssh_log_error("failed to allocate memory for receiving ssh string");
		return -1;
	}

	out_string->size = size;
	out_string->data = new_data;
	data_pointer = (char *)(new_data);
	remaining_size = size;
	while (remaining_size > 0)
	{
		int read_result = recv(in_ssh_socket, data_pointer, remaining_size, 0);
		if (read_result == SOCKET_ERROR)
		{
			ssh_log_error("could not read string data from socket, error: %d", WSAGetLastError());
			free(new_data);
			return -1;
		}
		if (read_result == 0)
		{
			ssh_log_error("socket closed while trying to read string data");
			free(new_data);
			return -1;
		}
		ssh_log_debug("read %d bytes from socket", read_result);
		remaining_size -= read_result;
		data_pointer += read_result;
	}

	return 0;
}

static int cleanup(SOCKET in_ssh_socket)
{
	int ret_value = 0;

	if (shutdown(in_ssh_socket, SD_BOTH) == SOCKET_ERROR)
	{
		ssh_log_error("unable to shutdown ssh socket, error: %d", WSAGetLastError());
		ret_value = -1;
	}

	if (closesocket(in_ssh_socket) == SOCKET_ERROR)
	{
		ssh_log_error("unable to close ssh socket, error: %d", WSAGetLastError());
		ret_value = -1;
	}

	WSACleanup();
}
