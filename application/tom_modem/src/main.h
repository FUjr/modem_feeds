#ifndef _MAIN_H_
#define _MAIN_H_
#include "modem_types.h"
#include "main.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <termios.h>
#include <signal.h>
#include <sys/select.h>
#include <errno.h>

#define DEFAULT_TIMEOUT 3
// 
extern FILE *fdi;             // file descriptor for input
extern FILE *fdo;             // file descriptor for output
extern int tty_fd;            // file descriptor for tty device
extern PROFILE_T s_profile;   // global profile     
extern char *self_name;       // program name
extern struct termios oldtio; // old tty setting

extern  int at(PROFILE_T *profile);

extern  int sms_read(PROFILE_T *profile);

extern  int sms_send(PROFILE_T *profile);

extern  int sms_delete(PROFILE_T *profile);

extern void dump_profile();

extern int match_option(char *option_name);

extern int match_operation(char *operation_name);

extern int open_tty_device(PROFILE_T *profile);

extern int usage();

#endif
