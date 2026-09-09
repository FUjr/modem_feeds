#include "sms_db.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int scalar(sqlite3 *sql, const char *query)
{
    sqlite3_stmt *stmt = NULL;
    int value = -1;
    assert(sqlite3_prepare_v2(sql, query, -1, &stmt, NULL) == SQLITE_OK);
    if (sqlite3_step(stmt) == SQLITE_ROW)
        value = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);
    return value;
}

int main(void)
{
    char path[] = "/tmp/qmodem-sms-db-XXXXXX";
    sms_db_t db;
    sms_import_result_t result;
    int fd = mkstemp(path);
    int published = 0;
    int gap = 0;
    sms_segment_t first = {
        .modem_id = "usb-1-1", .storage = "ME", .source_index = 7,
        .pdu = "PDU-A", .sender = "+100", .timestamp = 1000,
        .content = "hello ", .reference = 42, .total_parts = 2, .part_number = 1,
    };
    sms_segment_t second = {
        .modem_id = "usb-1-1", .storage = "ME", .source_index = 8,
        .pdu = "PDU-B", .sender = "+100", .timestamp = 1001,
        .content = "world", .reference = 42, .total_parts = 2, .part_number = 2,
    };

    assert(fd >= 0);
    close(fd);
    assert(sms_db_open(&db, path) == 0);
    assert(scalar(db.sql, "SELECT MAX(version) FROM schema_migrations") == 2);
    assert(sms_db_record_event(&db, "usb-1-1", 10, 1, &gap) == 0 && !gap);
    assert(sms_db_record_event(&db, "usb-1-1", 10, 2, &gap) == 0 && !gap);
    assert(sms_db_record_event(&db, "usb-1-1", 10, 4, &gap) == 0 && gap);
    assert(sms_db_record_event(&db, "usb-1-1", 11, 1, &gap) == 0 && gap);
    assert(!strcmp((const char *)sqlite3_db_filename(db.sql, "main"), path));
    assert(sms_db_import_segment(&db, &first, 2000, 1, &result) == 0);
    assert(result.safe_to_delete && !result.published);
    assert(scalar(db.sql, "SELECT count(*) FROM messages") == 0);
    sms_db_close(&db);

    assert(sms_db_open(&db, path) == 0);
    assert(sms_db_import_segment(&db, &first, 2001, 1, &result) == 0);
    assert(result.duplicate && result.safe_to_delete);
    assert(scalar(db.sql, "SELECT count(*) FROM source_messages") == 1);
    sms_db_set_multipart_windows(&db, 10, 3600);
    assert(sms_db_publish_expired(&db, "usb-1-1", 2011, 1, &published) == 0);
    assert(published == 1);
    assert(scalar(db.sql, "SELECT complete FROM messages") == 0);
    assert(scalar(db.sql, "SELECT revision FROM messages") == 1);
    assert(scalar(db.sql, "SELECT count(*) FROM deliveries") == 1);

    assert(sms_db_import_segment(&db, &second, 2400, 1, &result) == 0);
    assert(result.safe_to_delete && result.published && result.revision == 2);
    assert(scalar(db.sql, "SELECT complete FROM messages") == 1);
    assert(scalar(db.sql, "SELECT revision FROM messages") == 2);
    assert(scalar(db.sql, "SELECT count(*) FROM deliveries") == 2);
    {
        sqlite3_stmt *stmt = NULL;
        assert(sqlite3_prepare_v2(db.sql, "SELECT content FROM messages", -1,
                                  &stmt, NULL) == SQLITE_OK);
        assert(sqlite3_step(stmt) == SQLITE_ROW);
        assert(!strcmp((const char *)sqlite3_column_text(stmt, 0), "hello world"));
        sqlite3_finalize(stmt);
    }
	{
		int deleted = 0;
		assert(sms_db_delete_message(&db, "usb-1-1", result.message_id, &deleted) == 0);
		assert(deleted == 1);
		assert(sms_db_publish_expired(&db, "usb-1-1", 2500, 1, &published) == 0);
		assert(scalar(db.sql, "SELECT count(*) FROM messages") == 0);
	}

    second.source_index = 9;
    second.pdu = "PDU-C";
    second.part_number = 1;
    second.content = "new";
    assert(sms_db_import_segment(&db, &second, 7001, 0, &result) == 0);
    assert(scalar(db.sql, "SELECT count(*) FROM multipart_groups") == 1);
    {
        sms_segment_t single = {
            .modem_id = "usb-1-1", .storage = "ME", .source_index = 10,
            .pdu = "PDU-D", .sender = "+101", .timestamp = 7002,
            .content = "single", .total_parts = 1, .part_number = 1,
        };
        assert(sms_db_import_segment(&db, &single, 7002, 0, &result) == 0);
        assert(sms_db_finish_scan(&db, "usb-1-1", "ME", 7003) == 0);
        assert(scalar(db.sql,
            "SELECT count(*) FROM source_messages WHERE pdu='PDU-D'") == 0);
        assert(scalar(db.sql,
            "SELECT count(*) FROM source_messages WHERE pdu='PDU-C'") == 1);
    }
    assert(scalar(db.sql, "PRAGMA foreign_keys") == 1);
    sms_db_close(&db);
    unlink(path);
    {
        char wal[256];
        snprintf(wal, sizeof(wal), "%s-wal", path); unlink(wal);
        snprintf(wal, sizeof(wal), "%s-shm", path); unlink(wal);
    }
    puts("PASS durable idempotent multipart import, timeout, and late revision");
    return 0;
}
