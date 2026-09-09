#ifndef QMODEM_SMS_LEGACY_MIGRATE_H
#define QMODEM_SMS_LEGACY_MIGRATE_H

#include "sms_db.h"

int sms_legacy_migrate(sms_db_t *db, const char *directory,
                       const char *modem_id, int *imported);

#endif
