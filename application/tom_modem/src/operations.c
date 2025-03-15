#include "operations.h"

int at(PROFILE_T *profile,FDS_T *fds)
{
    int w_ret,r_ret;
    char output[COMMON_BUF_SIZE] = {0};
    if (profile->at_cmd == NULL)
    {
        err_msg("AT command is empty");
        return INVALID_PARAM;
    }
    w_ret = tty_write(fds->fdo, profile->at_cmd);
    if (w_ret)
    {
        return w_ret;
    }
    
    r_ret = tty_read(fds->fdi, output, COMMON_BUF_SIZE,profile->timeout);
    if (r_ret)
    {
        dbg_msg("Error sending AT command, error code: %d", r_ret);
        if (r_ret == COMM_ERROR)
            return r_ret;
    }
    user_msg("%s", output);
    return SUCCESS;
}

int binary_at(PROFILE_T *profile,FDS_T *fds)
{
    int w_ret,r_ret,hex_convert_ret;
    int binary_at_cmd_len;
    char *binary_at_cmd;
    char output[COMMON_BUF_SIZE] = {0};
    if (profile->at_cmd == NULL)
    {
        err_msg("AT command is empty");
        return INVALID_PARAM;
    }

    if (strlen(profile->at_cmd) % 2 != 0)
    {
        err_msg("Invalid AT command length");
        return INVALID_PARAM;
    }
    binary_at_cmd = (char *)malloc(strlen(profile->at_cmd) / 2 + 1);
    hex_convert_ret = str_to_hex(profile->at_cmd, binary_at_cmd);
    if (binary_at_cmd == NULL || hex_convert_ret)
    {
        err_msg("Binary AT command is empty");
        return INVALID_PARAM;
    }

    w_ret = tty_write_raw(fds->fdo, binary_at_cmd);
    if (w_ret)
    {
        return w_ret;
    }
    r_ret = tty_read_keyword(fds->fdi, output, COMMON_BUF_SIZE, "OK",profile->timeout);
    if (r_ret)
    {
        dbg_msg("Error sending AT command, error code: %d", r_ret);
        if (r_ret == COMM_ERROR)
            return r_ret;
    }
    user_msg("%s", output);
    return SUCCESS;
}

int sms_delete(PROFILE_T *profile,FDS_T *fds)
{
    int w_ret,r_ret;
    if (profile->sms_index < 0)
    {
        err_msg("SMS index is empty");
        return INVALID_PARAM;
    }
    char *delete_sms_cmd;
    delete_sms_cmd = (char *)malloc(32);
    snprintf(delete_sms_cmd, 32, DELETE_SMS, profile->sms_index);
    w_ret = tty_write(fds->fdo, delete_sms_cmd);
    if (w_ret)
    {
        return w_ret;
    }
    r_ret = tty_read_keyword(fds->fdi, NULL, COMMON_BUF_SIZE, "OK",profile->timeout);
    if (r_ret)
    {
        dbg_msg("Error deleting SMS, error code: %d", r_ret);
        if (r_ret == COMM_ERROR)
            return COMM_ERROR;
    }
    return SUCCESS;
}
int sms_read(PROFILE_T *profile,FDS_T *fds)
{
    SMS_T *sms_list[SMS_LIST_SIZE];
    SMS_T *sms;
    int w_ret,r_ret;
    char sms_pdu[SMS_BUF_SIZE] = {0};
    w_ret = tty_write(fds->fdo, SET_PDU_FORMAT);
    if (w_ret)
    {
        return w_ret;
    }
    r_ret = tty_read_keyword(fds->fdi, NULL, COMMON_BUF_SIZE, "OK",profile->timeout);
    if (r_ret)
    {
        dbg_msg("Error setting PDU format , error code: %d", r_ret);
        if (r_ret == COMM_ERROR)
            return r_ret;
    }
    dbg_msg("Set PDU format success");
    w_ret = tty_write(fds->fdo, READ_ALL_SMS);
    if (w_ret)
    {
        return w_ret;
    }
    r_ret = tty_read_keyword(fds->fdi, sms_pdu, SMS_BUF_SIZE, "OK",profile->timeout);
    if (r_ret)
    {
        dbg_msg("Error reading SMS , error code: %d", r_ret);
        if (r_ret == COMM_ERROR)
            return r_ret;
    }

    
    
    //遍历 sms_pdu 的每一行
    char *line = strtok(sms_pdu, "\n");
    int sms_count = 0;
    while (line != NULL)
    {
        if (strncmp(line, "+CMGL:", 6) == 0)
        {
            sms = (SMS_T *)malloc(sizeof(SMS_T));
            memset(sms, 0, sizeof(SMS_T));
            char *pdu = strtok(NULL, "\n");
            sms->sms_pdu = (char *)malloc(strlen(pdu));
            sms->sender = (char *)malloc(PHONE_NUMBER_SIZE);
            sms->sms_text = (char *)malloc(SMS_TEXT_SIZE);
            sms->sms_index = get_sms_index(line);
            memcpy(sms->sms_pdu, pdu, strlen(pdu));
            int sms_len = decode_pdu(sms);
            if (sms_len > 0)
            {
                sms_list[sms_count] = sms;
                sms_count++;
            }
            else
            {
                dbg_msg("Error decoding sms in line: %s", line);
                destroy_sms(sms);
            }
        }
        line = strtok(NULL, "\n");
    }

    
    display_sms_in_json(sms_list,sms_count);
    // for (int i = 1; i <= sms_count; i++)
    // {
    //     dump_sms(sms_list[i]);
    //     destroy_sms(sms_list[i]);
    // }
    dbg_msg("Read SMS success");
    dbg_msg("%s", sms_pdu);
    return SUCCESS;
}
int sms_send(PROFILE_T *profile,FDS_T *fds)
{
    int w_ret,r_ret;
    if (profile->sms_pdu == NULL)
    {
        err_msg("SMS PDU is empty");
        return INVALID_PARAM;
    }

    int pdu_len = strlen(profile->sms_pdu);
    int pdu_expected_len = (pdu_len) / 2 - 1;
    char *send_sms_cmd;
    char *write_pdu_cmd;
    w_ret = tty_write(fds->fdo, SET_PDU_FORMAT);
    if (w_ret)
    {
        return w_ret;
    }
    r_ret = tty_read_keyword(fds->fdi, NULL, COMMON_BUF_SIZE, "OK",profile->timeout);
    if (r_ret)
    {
        dbg_msg("Error setting PDU format , error code: %d", r_ret);
        if (r_ret == COMM_ERROR)
            return r_ret;
    }
    dbg_msg("Set PDU format success");
    send_sms_cmd = (char *)malloc(32);
    write_pdu_cmd = (char *)malloc(256);
    snprintf(send_sms_cmd, 32, SEND_SMS, pdu_expected_len);
    dbg_msg("Send SMS command: %s", send_sms_cmd); 
    snprintf(write_pdu_cmd, 256, "%s%c", profile->sms_pdu, 0x1A);
    dbg_msg("Write PDU command: %s", write_pdu_cmd);
    w_ret = tty_write(fds->fdo, send_sms_cmd);
    if (w_ret)
    {
        return w_ret;
    }
    r_ret = tty_read_keyword(fds->fdi, NULL, COMMON_BUF_SIZE, ">",profile->timeout);
    if (r_ret)
    {
        dbg_msg("Error sending SMS STEP 1, error code: %d", r_ret);
        if (r_ret == COMM_ERROR)
            return COMM_ERROR;
    }
    usleep(10000);
    w_ret = tty_write(fds->fdo, write_pdu_cmd);
    if (w_ret)
    {
        return w_ret;
    }
    r_ret = tty_read_keyword(fds->fdi, NULL, COMMON_BUF_SIZE, "+CMGS:",profile->timeout);
    if (r_ret)
    {
        dbg_msg("Error sending SMS STEP 2, error code: %d", r_ret);
        if (r_ret == COMM_ERROR)
            return COMM_ERROR;
    }

    free(send_sms_cmd);
    free(write_pdu_cmd);


    return 0;
}
