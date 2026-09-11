#ifndef QMODEM_VOIP_SIP_CREDENTIALS_H
#define QMODEM_VOIP_SIP_CREDENTIALS_H

#include "sip_gateway.h"

#define QMODEM_VOIP_SIP_PASSWORD_SIZE 33

int qmodem_voip_sip_credentials_sync(char username[QMODEM_VOIP_SIP_USERNAME_SIZE]);
int qmodem_voip_sip_credentials_generate(const char *username,
	char password[QMODEM_VOIP_SIP_PASSWORD_SIZE]);

#endif
