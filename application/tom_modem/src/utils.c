#include "utils.h"
#include "pdu_lib/pdu.h"
struct termios oldtio;

int match_option(char *option_name)
{
    char short_option;
    char *long_option;
    // if start with '-' then it is an single character option
    if (option_name[0] == '-' && option_name[1] != '-')
    {

        short_option = option_name[1];
        switch (short_option)
        {
        case AT_CMD_S:
            return AT_CMD;
        case TTY_DEV_S:
            return TTY_DEV;
        case BAUD_RATE_S:
            return BAUD_RATE;
        case DATA_BITS_S:
            return DATA_BITS;
        case PARITY_S:
            return PARITY;
        case STOP_BITS_S:
            return STOP_BITS;
        case FLOW_CONTROL_S:
            return FLOW_CONTROL;
        case TIMEOUT_S:
            return TIMEOUT;
        case OPERATION_S:
            return OPERATION;
        case DEBUG_S:
            return DEBUG;
        case SMS_PDU_S:
            return SMS_PDU;
        case SMS_INDEX_S:
            return SMS_INDEX;
        default:
            return -1;
        }
    }
    if (option_name[0] == '-' && option_name[1] == '-')
    {
        long_option = option_name + 2;
        if (strcmp(long_option, AT_CMD_L) == 0)
        {
            return AT_CMD;
        }
        else if (strcmp(long_option, TTY_DEV_L) == 0)
        {
            return TTY_DEV;
        }
        else if (strcmp(long_option, BAUD_RATE_L) == 0)
        {
            return BAUD_RATE;
        }
        else if (strcmp(long_option, DATA_BITS_L) == 0)
        {
            return DATA_BITS;
        }
        else if (strcmp(long_option, PARITY_L) == 0)
        {
            return PARITY;
        }
        else if (strcmp(long_option, STOP_BITS_L) == 0)
        {
            return STOP_BITS;
        }
        else if (strcmp(long_option, FLOW_CONTROL_L) == 0)
        {
            return FLOW_CONTROL;
        }
        else if (strcmp(long_option, TIMEOUT_L) == 0)
        {
            return TIMEOUT;
        }
        else if (strcmp(long_option, OPERATION_L) == 0)
        {
            return OPERATION;
        }
        else if (strcmp(long_option, DEBUG_L) == 0)
        {
            return DEBUG;
        }
        else if (strcmp(long_option, SMS_PDU_L) == 0)
        {
            return SMS_PDU;
        }
        else if (strcmp(long_option, SMS_INDEX_L) == 0)
        {
            return SMS_INDEX;
        }
        else
        {
            return -1;
        }
    }
    // if start with '--' then it is a long option
    return -1;
}

int match_operation(char *operation_name)
{

    char short_op;
    int opstr_len = strlen(operation_name);
    if (opstr_len == 1)
    {
        short_op = operation_name[0];
        switch (short_op)
        {
        case AT_OP_S:
            return AT_OP;
        case SMS_READ_OP_S:
            return SMS_READ_OP;
        case SMS_SEND_OP_S:
            return SMS_SEND_OP;
        case SMS_DELETE_OP_S:
            return SMS_DELETE_OP;
        default:
            return -1;
            break;
        }
    }
    else if (opstr_len > 1)
    {
        if (strcmp(operation_name, AT_OP_L) == 0)
        {
            return AT_OP;
        }
        else if (strcmp(operation_name, SMS_READ_OP_L) == 0)
        {
            return SMS_READ_OP;
        }
        else if (strcmp(operation_name, SMS_SEND_OP_L) == 0)
        {
            return SMS_SEND_OP;
        }
        else if (strcmp(operation_name, SMS_DELETE_OP_L) == 0)
        {
            return SMS_DELETE_OP;
        }
        else
        {
            return -1;
        }
    }
}

int open_tty_device(PROFILE_T *profile)
{
    tty_fd = open(profile->tty_dev, O_RDWR | O_NOCTTY);
    if (tty_fd < 0)
    {
        err_msg("Error opening tty device: %s", profile->tty_dev);
        return -1;
    }

    if (set_tty_device(profile) != 0)
    {
        err_msg("Error setting tty device");
        return -1;
    }
    tcflush(tty_fd, TCIOFLUSH);
    atexit(clean_up);
    if (tty_fd >= 0)
        close(tty_fd);
    tty_fd = open(profile->tty_dev, O_RDWR | O_NOCTTY);
    fdi = fdopen(tty_fd, "r");
    fdo = fdopen(tty_fd, "w");
    if (fdi == NULL || fdo == NULL)
    {
        err_msg("Error opening file descriptor");
        return -1;
    }

    if (setvbuf(fdo, NULL, _IOFBF, 0))
    {
        err_msg("Error setting buffer for fdi");
        return -1;
    }

    if (setvbuf(fdi, NULL, _IOLBF, 0))
    {
        err_msg("Error setting buffer for fdi");
        return -1;
    }

    return 0;
}

static int set_tty_device(PROFILE_T *profile)
{
    int baud_rate, data_bits, stop_bits;
    char *flow_control;
    struct termios tty;
    baud_rate = profile->baud_rate;
    data_bits = profile->data_bits;
    stop_bits = profile->stop_bits;
    flow_control = profile->flow_control;
    if (tcgetattr(tty_fd, &tty) != 0)
    {
        err_msg("Error getting tty attributes");
        return -1;
    }
    memmove(&oldtio, &tty, sizeof(struct termios));
    cfmakeraw(&tty);
    tty.c_cflag |= CLOCAL; // 忽略调制解调器控制线，允许本地连接
    tty.c_cflag |= CREAD;  // 使能接收

    // clear flow control ,stop bits parity
    tty.c_cflag &= ~CRTSCTS;
    tty.c_cflag &= ~CSTOPB;
    tty.c_cflag &= ~PARENB;
    tty.c_oflag &= ~OPOST;
    tty.c_cc[VMIN] = 1;
    tty.c_cc[VTIME] = 0;

    // set data bits 5,6,7,8
    tty.c_cflag &= ~CSIZE; // 清除数据位设置
    switch (data_bits)
    {
    case 5:
        tty.c_cflag |= CS5;
        break;
    case 6:
        tty.c_cflag |= CS6;
        break;
    case 7:
        tty.c_cflag |= CS7;
        break;
    case 8:
        tty.c_cflag |= CS8;
        break;
    default:
        tty.c_cflag |= CS8;
        break;
    }

    // set baud rate
    switch (baud_rate)
    {
    case 4800:
        cfsetspeed(&tty, B4800);
        break;
    case 9600:
        cfsetspeed(&tty, B9600);
        break;
    case 19200:
        cfsetspeed(&tty, B19200);
        break;
    case 38400:
        cfsetspeed(&tty, B38400);
        break;
    case 57600:
        cfsetspeed(&tty, B57600);
        break;
    case 115200:
        cfsetspeed(&tty, B115200);
        break;

    default:
        cfsetspeed(&tty, B115200);
        break;
    }
    if (tcsetattr(tty_fd, TCSANOW, &tty) != 0)
    {
        err_msg("Error setting tty attributes");
        return -1;
    }
    return 0;
}

int char_to_hex(char c)
{
    // convert char to hex
    int is_digit, is_lower, is_upper;
    is_digit = c - '0';
    is_lower = c - 'a' + 10;
    is_upper = c - 'A' + 10;
    if (is_digit >= 0 && is_digit <= 9)
    {
        return is_digit;
    }
    else if (is_lower >= 10 && is_lower <= 15)
    {
        return is_lower;
    }
    else if (is_upper >= 10 && is_upper <= 15)
    {
        return is_upper;
    }
    else
    {
        return -1;
    }
}

static void clean_up()
{
    if (tcsetattr(tty_fd, TCSANOW, &oldtio) != 0)
    {
        err_msg("Error restoring old tty attributes");
        return;
    }
    dbg_msg("Clean up success");
    tcflush(tty_fd, TCIOFLUSH);
    // if (tty_fd >= 0)
    //     close(tty_fd);
}

static void escape_json(char *input, char *output)
{
    char *p = input;
    char *q = output;
    while (*p)
    {
        if (*p == '"')
        {
            *q++ = '\\';
            *q++ = '"';
        }
        else if (*p == '\\')
        {
            *q++ = '\\';
            *q++ = '\\';
        }
        else if (*p == '/')
        {
            *q++ = '\\';
            *q++ = '/';
        }
        else if (*p == '\b')
        {
            *q++ = '\\';
            *q++ = 'b';
        }
        else if (*p == '\f')
        {
            *q++ = '\\';
            *q++ = 'f';
        }
        else if (*p == '\n')
        {
            *q++ = '\\';
            *q++ = 'n';
        }
        else if (*p == '\r')
        {
            *q++ = '\\';
            *q++ = 'r';
        }
        else if (*p == '\t')
        {
            *q++ = '\\';
            *q++ = 't';
        }
        else
        {
            *q++ = *p;
        }
        p++;
    }
    *q = '\0';
}

int usage()
{
    err_msg("Usage: %s [options]", self_name);
    err_msg("Options:");
    err_msg("  -c, --at_cmd <AT command>  AT command");
    err_msg("  -d, --tty_dev <TTY device>  TTY device **REQUIRED**");
    err_msg("  -b, --baud_rate <baud rate>  Baud rate Default: 115200 Supported: 4800,9600,19200,38400,57600,115200");
    err_msg("  -B, --data_bits <data bits>  Data bits Default: 8 Supported: 5,6,7,8");
    err_msg("  -t, --timeout <timeout>  Timeout Default: 3");
    err_msg("  -o, --operation <operation>  Operation(at[a:defualt], sms_read[r], sms_send[s], sms_delete[d])");
    err_msg("  -D, --debug Debug mode Default: off");
    err_msg("  -p, --sms_pdu <sms pdu>  SMS PDU");
    err_msg("  -i, --sms_index <sms index>  SMS index");
    err_msg("Example:");
    err_msg("  %s -c ATI -d /dev/ttyUSB2 -b 115200 -B 8 -o at #advance at mode set bautrate and data bit", self_name);
    err_msg("  %s -c ATI -d /dev/ttyUSB2 # normal at mode", self_name);
    err_msg("  %s -d /dev/mhi_DUN -o r # read sms", self_name);
    exit(-1);
}

void dump_profile()
{
    dbg_msg("AT command: %s", s_profile.at_cmd);
    dbg_msg("TTY device: %s", s_profile.tty_dev);
    dbg_msg("Baud rate: %d", s_profile.baud_rate);
    dbg_msg("Data bits: %d", s_profile.data_bits);
    dbg_msg("Parity: %s", s_profile.parity);
    dbg_msg("Stop bits: %d", s_profile.stop_bits);
    dbg_msg("Flow control: %s", s_profile.flow_control);
    dbg_msg("Timeout: %d", s_profile.timeout);
    dbg_msg("Operation: %d", s_profile.op);
    dbg_msg("Debug: %d", s_profile.debug);
    dbg_msg("SMS PDU: %s", s_profile.sms_pdu);
    dbg_msg("SMS index: %d", s_profile.sms_index);
}

int tty_read(FILE *fdi, char *output, int len, int timeout)
{
    return tty_read_keyword(fdi, output, len, timeout, NULL);
}

int tty_read_keyword(FILE *fdi, char *output, int len, int timeout, char *key_word)
{
    int ret, fd;
    fd_set rfds;
    struct timeval tv;
    char tmp[LINE_BUF] = {0};
    int msg_len = 0;
    int key_word_len = 0;
    fd = fileno(fdi);
    tv.tv_sec = 0;
    tv.tv_usec = timeout;

    FD_ZERO(&rfds);
    FD_SET(fd, &rfds);
    if (key_word != NULL)
    {
        key_word_len = strlen(key_word);
    }
    while (1)
    {
        ret = select(fd + 1, &rfds, NULL, NULL, &tv);
        if (ret == -1)
        {
            if (errno == EINTR)
            {
                err_msg("Interrupted by signal");
                return -1;
            }
            err_msg("Error in select");
            return -1;
        }
        else
        {
            fgets(tmp, LINE_BUF, fdi);
            if (output != NULL)
                msg_len += snprintf(output + msg_len, len - msg_len, "%s", tmp);
            dbg_msg("%s", tmp);
        }
        if (key_word != NULL){
            if (strncmp(tmp, key_word, key_word_len) == 0)
            {
                dbg_msg("Received end sign: %s", tmp);
                return 0;
            }
        }
        if (strncmp(tmp, "OK", 2) == 0 ||
            strncmp(tmp, "ERROR", 5) == 0 ||
            strncmp(tmp, "+CMS ERROR:", 11) == 0 ||
            strncmp(tmp, "+CME ERROR:", 11) == 0 ||
            strncmp(tmp, "NO CARRIER", 10) == 0){
                dbg_msg("Received end sign: %s", tmp);
                if (key_word == NULL){
                    return 0;
                }
                break;
            }
    }

    return -1;
}

int tty_write(FILE *fdo, char *input)
{
    int cmd_len, ret;
    char *cmd_line;
    cmd_len = strlen(input) + 3;
    cmd_line = (char *)malloc(cmd_len);
    if (cmd_line == NULL)
    {
        err_msg("Error allocating memory");
        return -1;
    }
    snprintf(cmd_line, cmd_len, "%s\r\n", input);
    ret = fputs(cmd_line, fdo);
    free(cmd_line);
    fflush(fdo);
    usleep(100);
    if (ret < 0)
    {
        err_msg("Error writing to tty %d" , ret);
        return -1;
    }
    return 0;
}

int at(PROFILE_T *profile)
{
    char output[COMMON_BUF_SIZE] = {0};
    if (profile->at_cmd == NULL)
    {
        err_msg("AT command is empty");
        return -1;
    }
    alarm(profile->timeout);

    if (tty_write(fdo, profile->at_cmd))
    {
        err_msg("Error writing to tty");
        return -1;
    }

    if (tty_read(fdi, output, COMMON_BUF_SIZE, 100))
    {
        err_msg("Error reading from tty");
        return -1;
    }
    user_msg("%s", output);
    return 0;
}

int sms_read(PROFILE_T *profile)
{
    SMS_T *sms_list[SMS_LIST_SIZE];
    SMS_T *sms;
    char sms_pdu[SMS_BUF_SIZE] = {0};
    tty_write(fdo, SET_PDU_FORMAT);
    alarm(profile->timeout);
    if (tty_read_keyword(fdi, NULL, COMMON_BUF_SIZE, 100, "OK"))
    {
        err_msg("Error setting PDU format");
        return -1;
    }
    dbg_msg("Set PDU format success");
    tty_write(fdo, READ_ALL_SMS);
    alarm(profile->timeout);
    if (tty_read_keyword(fdi, sms_pdu, SMS_BUF_SIZE, 100, "OK"))
    {
        err_msg("Error reading SMS");
        return -1;
    }
    alarm(0);
    
    
    //遍历 sms_pdu 的每一行
    char *line = strtok(sms_pdu, "\n");
    int sms_count = 0;
    while (line != NULL)
    {
        
        if (strncmp(line, "+CMGL:", 6) == 0)
        {
            //解析 line +CMGL: 2,1,,102 获取短信索引
            sms = (SMS_T *)malloc(sizeof(SMS_T));
            memset(sms, 0, sizeof(SMS_T));
            char *pdu = strtok(NULL, "\n");
            sms->sms_pdu = (char *)malloc(strlen(pdu));
            sms->sender = (char *)malloc(PHONE_NUMBER_SIZE);
            sms->sms_text = (char *)malloc(SMS_TEXT_SIZE);
            sms->sms_index = get_sms_index(line);
            memcpy(sms->sms_pdu, pdu, strlen(pdu));
            int sms_len = decode(sms);
            if (sms_len > 0)
            {
                sms_list[sms_count] = sms;
                sms_count++;
            }
            else
            {
                destroy_sms(sms);
            }
        }
        line = strtok(NULL, "\n");
    }

    // for (int i = 1; i <= sms_count; i++)
    // {
    //     dump_sms(sms_list[i]);
    //     //destroy_sms(sms_list[i]);
    // }
    display_sms_in_json(sms_list,sms_count);
    dbg_msg("Read SMS success");
    dbg_msg("%s", sms_pdu);
    return 0;
}

int sms_send(PROFILE_T *profile)
{
    if (profile->sms_pdu == NULL)
    {
        err_msg("SMS PDU is empty");
        return -1;
    }

    int pdu_len = strlen(profile->sms_pdu);
    int pdu_expected_len = (pdu_len) / 2 - 1;
    char *send_sms_cmd;
    char *write_pdu_cmd;
    tty_write(fdo, SET_PDU_FORMAT);
    alarm(profile->timeout);
    if (tty_read_keyword(fdi, NULL, COMMON_BUF_SIZE, 100, "OK"))
    {
        err_msg("Error setting PDU format");
        return -1;
    }
    dbg_msg("Set PDU format success");
    send_sms_cmd = (char *)malloc(32);
    write_pdu_cmd = (char *)malloc(256);
    snprintf(send_sms_cmd, 32, SEND_SMS, pdu_expected_len);
    dbg_msg("Send SMS command: %s", send_sms_cmd); 
    snprintf(write_pdu_cmd, 256, "%s%c", profile->sms_pdu, 0x1A);
    dbg_msg("Write PDU command: %s", write_pdu_cmd);
    free(send_sms_cmd);
    free(write_pdu_cmd);
    alarm(0);
    tty_write(fdo, send_sms_cmd);
    usleep(10000);
    tty_write(fdo, write_pdu_cmd);
    alarm(profile->timeout);
    if (tty_read_keyword(fdi, NULL, COMMON_BUF_SIZE, 100, "+CMGS:"))
    {
        err_msg("Error sending SMS STEP 2");
        return -1;
    }




    return 0;
}

int sms_delete(PROFILE_T *profile)
{
    if (profile->sms_index < 0)
    {
        err_msg("SMS index is empty");
        return -1;
    }
    char *delete_sms_cmd;
    delete_sms_cmd = (char *)malloc(32);
    snprintf(delete_sms_cmd, 32, DELETE_SMS, profile->sms_index);
    tty_write(fdo, delete_sms_cmd);
    alarm(profile->timeout);
    if (tty_read_keyword(fdi, NULL, COMMON_BUF_SIZE, 100, "OK"))
    {
        err_msg("Error deleting SMS");
        return -1;
    }
    return 0;
}

int decode(SMS_T *sms)
{
    char sms_text[SMS_TEXT_SIZE] = {0};
    int tp_dcs;
    int skip_bytes;
    int pdu_str_len;
    time_t sms_time;
    unsigned char hex_pdu[SMS_PDU_HEX_SIZE] = {0};
    pdu_str_len = strlen(sms->sms_pdu);
    for (int i = 0; i < pdu_str_len; i += 2)
    {
        hex_pdu[i / 2] = char_to_hex(sms->sms_pdu[i]) << 4;
        hex_pdu[i / 2] |= char_to_hex(sms->sms_pdu[i + 1]);
    }
    int sms_len = pdu_decode(hex_pdu, pdu_str_len/2,
                             &sms->timestamp,
                             sms->sender, PHONE_NUMBER_SIZE,
                             sms_text, SMS_TEXT_SIZE,
                             &tp_dcs,
                             &sms->ref_number,
                             &sms->total_segments,
                             &sms->segment_number,
                             &skip_bytes);
    if (sms_len <= 0)
    {
        err_msg("Error decoding pdu");
        return sms_len;
    }
    sms->sms_lenght = sms_len;

    switch ((tp_dcs / 4) % 4)
    {
    case 0:
        { 
            // GSM 7 bit
            sms->type = SMS_CHARSET_7BIT;
            int i;
            i = skip_bytes;
            if (skip_bytes > 0)
                i = (skip_bytes * 8 + 6) / 7;
            for (; i < strlen(sms_text); i++)
            {
                sprintf(sms->sms_text + i, "%c", sms_text[i]);
            }
            i++;
            sprintf(sms->sms_text + i, "%c", '\0');
            break;
        }
    case 2:
        { 
            // UCS2
            sms->type = SMS_CHARSET_UCS2;
            int offset = 0;
            for (int i = skip_bytes; i < SMS_TEXT_SIZE; i += 2)
            {
                int ucs2_char = 0x000000FF & sms_text[i + 1];
                ucs2_char |= (0x0000FF00 & (sms_text[i] << 8));
                unsigned char utf8_char[5];
                int len = ucs2_to_utf8(ucs2_char, utf8_char);
                int j;
                for (j = 0; j < len; j++)
                {
                    sprintf(sms->sms_text + offset, "%c", utf8_char[j]);
                    if (utf8_char[j] != '\0')
                    {
                        offset++;
                    }
                    
                }
            }
            offset++;
            sprintf(sms->sms_text + offset, "%c", '\0');
            break;
        }
    default:
        break;
    }
    return sms_len;
}

int display_sms_in_json(SMS_T **sms,int num)
{

    char msg_json[SMS_BUF_SIZE];
    int offset;
    offset = sprintf(msg_json, "{\"msg\":[");
    for (int i = 0; i < num; i++)
    {
        char escaped_text[SMS_TEXT_SIZE];
        escape_json(sms[i]->sms_text, escaped_text);
        if (sms[i]->ref_number)
            offset += sprintf(msg_json + offset, "{\"index\":%d,\"sender\":\"%s\",\"timestamp\":%d,\"content\":\"%s\",\"reference\":%d,\"total\":%d,\"part\":%d},",
                          sms[i]->sms_index, sms[i]->sender, sms[i]->timestamp, escaped_text, sms[i]->ref_number, sms[i]->total_segments, sms[i]->segment_number);
        else
            offset += sprintf(msg_json + offset, "{\"index\":%d,\"sender\":\"%s\",\"timestamp\":%d,\"content\":\"%s\"},",
                          sms[i]->sms_index, sms[i]->sender, sms[i]->timestamp, escaped_text);
    }
    
    //if not empty msg_json,remove the last ','
    if (offset > 10)
    {
        offset--;
    }
    offset += sprintf(msg_json + offset, "]}");
    user_msg("%s\n", msg_json);
    return 0;

    
}

int dump_sms(SMS_T *sms)
{
    dbg_msg("SMS Index: %d", sms->sms_index);
    dbg_msg("SMS Text: %s", sms->sms_text);
    dbg_msg("SMS Sender: %s", sms->sender);
    dbg_msg("SMS Timestamp: %d", sms->timestamp);
    dbg_msg("SMS Segment: %d/%d", sms->segment_number, sms->total_segments);
    return 0;
}

int destroy_sms(SMS_T *sms)
{
    if (sms->sms_pdu != NULL)
    {
        free(sms->sms_pdu);
    }
    if (sms->sender != NULL)
    {
        free(sms->sender);
    }
    if (sms->sms_text != NULL)
    {
        free(sms->sms_text);
    }
    free(sms);
    return 0;
}
