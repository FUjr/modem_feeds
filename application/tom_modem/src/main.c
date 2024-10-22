#include "modem_types.h"
#include "main.h"
#include "utils.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <termios.h>
#include <signal.h>
#include <sys/select.h>
#include <errno.h>

FILE *fdi;             // file descriptor for input
FILE *fdo;             // file descriptor for output
int tty_fd;            // file descriptor for tty device

PROFILE_T s_profile;   // global profile     
char *self_name; // program name
void _timeout(int signo)
{
    err_msg("Exit with Signal %d", signo);
    kill(getpid(), SIGINT);
}

int parse_user_input(int argc, char *argv[], PROFILE_T *profile)
{
    int opt = 1;
    int option;
    profile->sms_index = -1;
#define has_more_argv() (opt < argc ? 1 : 0)
    while (opt < argc)
    {
        option = match_option(argv[opt]);
        if (option == -1)
        {
            usage();
            return -1;
        }
        opt++;
        switch (option)
        {
        case AT_CMD:
            if (!has_more_argv())
            {
                usage();
                return -1;
            }
            profile->at_cmd = argv[opt++];
            break;
        case TTY_DEV:
            if (!has_more_argv())
            {
                usage();
                return -1;
            }
            profile->tty_dev = argv[opt++];
            break;
        case BAUD_RATE:
            if (!has_more_argv())
            {
                usage();
                return -1;
            }
            profile->baud_rate = atoi(argv[opt++]);
            break;
        case DATA_BITS:
            if (!has_more_argv())
            {
                usage();
                return -1;
            }
            profile->data_bits = atoi(argv[opt++]);
            break;
        case PARITY:
            if (!has_more_argv())
            {
                usage();
                return -1;
            }
            profile->parity = argv[opt++];
            break;
        case STOP_BITS:
            if (!has_more_argv())
            {
                usage();
                return -1;
            }
            profile->stop_bits = atoi(argv[opt++]);
            break;
        case FLOW_CONTROL:
            if (!has_more_argv())
            {
                usage();
                return -1;
            }
            profile->flow_control = argv[opt++];
            break;
        case TIMEOUT:
            if (!has_more_argv())
            {
                usage();
                return -1;
            }
            profile->timeout = atoi(argv[opt++]);
            break;
        case OPERATION:
            if (!has_more_argv())
            {
                usage();
                return -1;
            }
            profile->op = match_operation(argv[opt++]);
            break;
        case DEBUG:
            profile->debug = 1;
            break;
        case SMS_PDU:
            if (!has_more_argv())
            {
                usage();
                return -1;
            }
            profile->sms_pdu = argv[opt++];
            break;
        case SMS_INDEX:
            if (!has_more_argv())
            {
                usage();
                return -1;
            }
            profile->sms_index = atoi(argv[opt++]);
            break;
        default:
            err_msg("Invalid option: %s", argv[opt]);
            break;
        }
    }
    // default settings:
    if (profile->baud_rate == 0 )
    {
        profile->baud_rate = 115200;
    }
    if (profile->data_bits == 0)
    {
        profile->data_bits = 8;
    }
    if (profile->timeout == 0)
    {
        profile->timeout = 3;
    }
    if (profile->op == 0 || profile->op == -1)
    {
        profile->op = AT_OP;
    }
  
}

int run_op(PROFILE_T *profile)
{
    switch (profile->op)
    {
    case AT_OP:
        at(profile);
        break;
    case SMS_READ_OP:
        sms_read(profile);
        break;
    case SMS_SEND_OP:
        sms_send(profile);
        break;
    case SMS_DELETE_OP:
        sms_delete(profile);
        break;
    default:
        err_msg("Invalid operation");
        break;
    }
}

int main(int argc, char *argv[])
{
    int ret;
    // init
    self_name = argv[0];
    PROFILE_T *profile = &s_profile;
    parse_user_input(argc, argv, profile);
    dump_profile();
    signal(SIGALRM, _timeout);

    // try open tty devices
    if (open_tty_device(profile))
    {
        err_msg("Failed to open tty device");
        return -1;
    }
    if (run_op(profile))
    {
        err_msg("Failed to run operation %d", profile->op);
        return -1;
    }
    
    dbg_msg("Exit");
    return 0;
}
