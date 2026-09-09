#include "legacy_migrate.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
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

static void write_file(const char *path, const char *content)
{
    FILE *file = fopen(path, "w");
    assert(file);
    assert(fputs(content, file) >= 0);
    assert(fclose(file) == 0);
}

int main(void)
{
    char directory[] = "/tmp/qmodem-sms-migrate-XXXXXX";
    char db_path[256], sent_path[256], received_path[256];
    char error[512];
    sms_db_t db;
    int imported;

    assert(mkdtemp(directory));
    snprintf(db_path, sizeof(db_path), "%s/sms.sqlite3", directory);
    snprintf(sent_path, sizeof(sent_path), "%s/modem_1_sent.json", directory);
    snprintf(received_path, sizeof(received_path), "%s/modem_1_received.json", directory);
    write_file(sent_path,
        "{\"sent\":[{\"id\":1,\"recipient\":\"10086\",\"timestamp\":10,"
        "\"content\":\"sent\",\"is_success\":true}],\"next_id\":2}");
    write_file(received_path,
        "{\"received\":[{\"id\":7,\"sender\":\"10010\",\"timestamp\":20,"
        "\"content\":\"received\",\"is_read\":false,\"forwarded\":false}],\"next_id\":8}");
    assert(sms_db_open(&db, db_path) == 0);
    assert(sms_legacy_migrate(&db, directory, "modem_1", &imported) == 0);
    assert(imported == 2);
    assert(scalar(db.sql, "SELECT count(*) FROM messages") == 2);
    assert(scalar(db.sql, "SELECT count(*) FROM deliveries") == 0);
    assert(sms_legacy_migrate(&db, directory, "modem_1", &imported) == 0);
    assert(imported == 0);
    assert(scalar(db.sql, "SELECT count(*) FROM messages") == 2);

    snprintf(received_path, sizeof(received_path), "%s/modem_2_received.json", directory);
    write_file(received_path, "{broken");
    assert(sms_legacy_migrate(&db, directory, "modem_2", &imported) != 0);
    assert(sms_db_migration_error(&db, "modem_2", error, sizeof(error)) == 1);
    assert(*error);
    assert(scalar(db.sql, "SELECT count(*) FROM messages WHERE modem_id='modem_2'") == 0);
    sms_db_close(&db);
    unlink(sent_path); unlink(received_path); unlink(db_path);
    snprintf(error, sizeof(error), "%s-wal", db_path); unlink(error);
    snprintf(error, sizeof(error), "%s-shm", db_path); unlink(error);
    rmdir(directory);
    puts("PASS idempotent legacy migration and per-modem corruption state");
    return 0;
}
