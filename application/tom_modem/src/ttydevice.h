#ifndef TTYDEVICE_H
#define TTYDEVICE_H
#include "modem_types.h"
#include "utils.h"

int tty_open_device(PROFILE_T *profile,FDS_T *fds);
int tty_write_raw(FILE *fdo, const void *input, size_t length);
int tty_write(FILE *fdo, const char *input);

#endif
