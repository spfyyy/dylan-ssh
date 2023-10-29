#include <stdio.h>
#include "ssh_log.h"

void ssh_log_debug(const char *msg_format, ...)
{
   va_list arglist;
   va_start(arglist, msg_format);
   vfprintf(stdout, msg_format, arglist);
   fprintf(stdout, "\n");
   va_end(arglist);
}

void ssh_log_error(const char *msg_format, ...)
{
   va_list arglist;
   va_start(arglist, msg_format);
   vfprintf(stderr, msg_format, arglist);
   fprintf(stderr, "\n");
   va_end(arglist);
}
