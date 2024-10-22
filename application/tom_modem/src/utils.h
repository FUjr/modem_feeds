#ifndef _UTILS_H
#define _UTILS_H
#include <stdio.h>
#include <stdlib.h>
#include "modem_types.h"
#include "main.h"

extern PROFILE_T s_profile;
extern FILE *fdi;             // file descriptor for input
extern FILE *fdo;             // file descriptor for output
extern int tty_fd;            // file descriptor for tty device
extern struct termios oldtio; // old tty setting



#define dbg_msg(fmt, args...) do { \
    if (s_profile.debug) { \
    fprintf(stderr, "[DBG]" fmt, ##args); \
    fprintf(stderr, "\n"); \
	fflush(stderr); \
    } \
} while(0)

#define err_msg(fmt, args...) do { \
    if (1) { \
    fprintf(stderr, "[ERR]"  fmt , ##args); \
    fprintf(stderr, "\n"); \
	fflush(stderr); \
    } \
} while(0)

#define get_sms_index(line) \
    ({ \
        const char *index_str = (line) + 7; \
        const char *first_comma = strchr(index_str, ','); \
        int sms_index = -1; \
        if (first_comma) { \
            char temp[(size_t)(first_comma - index_str) + 1]; \
            memcpy(temp, index_str, first_comma - index_str); \
            temp[(size_t)(first_comma - index_str)] = '\0'; \
            sms_index = atoi(temp); \
        } \
        sms_index; \
    })

#define user_msg(fmt, args...) (fprintf(stdout, fmt , ##args))



int match_option(char *option_name);

int match_operation(char *operation_name);

int open_tty_device(PROFILE_T *profile);

static int set_tty_device(PROFILE_T *profile);

static void clean_up();
#endif
