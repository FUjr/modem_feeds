#ifndef OPERATION_H
#define OPERATION_H
#include "modem_types.h"
#include "ttydevice.h"
#include "utils.h"
int tty_open_device(PROFILE_T *profile, FDS_T *fds);
int tty_read(FILE *fdi, char *output, int len, int soft_timeout);
int tty_read_keyword(FILE *fdi, char *output, int len, char *key_word, int soft_timeout);
int tty_write(FILE *fdo, char *input);
#endif
