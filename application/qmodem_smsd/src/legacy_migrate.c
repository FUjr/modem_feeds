#include "legacy_migrate.h"

#include <json-c/json.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static int exec_sql(sqlite3 *sql, const char *statement)
{
    return sqlite3_exec(sql, statement, NULL, NULL, NULL) == SQLITE_OK ? 0 : -1;
}

static int bind_text(sqlite3_stmt *stmt, int index, const char *value)
{
    return sqlite3_bind_text(stmt, index, value ? value : "", -1, SQLITE_TRANSIENT);
}

static int set_error(sms_db_t *db, const char *modem_id, const char *error)
{
    sqlite3_stmt *stmt = NULL;
    int rc = -1;
    if (sqlite3_prepare_v2(db->sql,
            "INSERT INTO modems(id,migration_error) VALUES(?,?) "
            "ON CONFLICT(id) DO UPDATE SET migration_error=excluded.migration_error",
            -1, &stmt, NULL) != SQLITE_OK)
        return -1;
    bind_text(stmt, 1, modem_id);
    if (error)
        bind_text(stmt, 2, error);
    else
        sqlite3_bind_null(stmt, 2);
    if (sqlite3_step(stmt) == SQLITE_DONE)
        rc = 0;
    sqlite3_finalize(stmt);
    return rc;
}

static int already_imported(sqlite3 *sql, const char *path, const char *key)
{
    sqlite3_stmt *stmt = NULL;
    int found = -1;
    if (sqlite3_prepare_v2(sql,
            "SELECT 1 FROM legacy_imports WHERE path=? AND record_key=?", -1,
            &stmt, NULL) != SQLITE_OK)
        return -1;
    bind_text(stmt, 1, path);
    bind_text(stmt, 2, key);
    found = sqlite3_step(stmt) == SQLITE_ROW;
    sqlite3_finalize(stmt);
    return found;
}

static int remember(sqlite3 *sql, const char *path, const char *key)
{
    sqlite3_stmt *stmt = NULL;
    int rc = -1;
    if (sqlite3_prepare_v2(sql,
            "INSERT INTO legacy_imports(path,record_key,imported_at) "
            "VALUES(?,?,strftime('%s','now'))", -1, &stmt, NULL) != SQLITE_OK)
        return -1;
    bind_text(stmt, 1, path);
    bind_text(stmt, 2, key);
    if (sqlite3_step(stmt) == SQLITE_DONE)
        rc = 0;
    sqlite3_finalize(stmt);
    return rc;
}

static const char *json_string(json_object *entry, const char *name, const char *fallback)
{
    json_object *value = NULL;
    return json_object_object_get_ex(entry, name, &value) &&
           json_object_is_type(value, json_type_string) ? json_object_get_string(value) : fallback;
}

static int64_t json_integer(json_object *entry, const char *name, int64_t fallback)
{
    json_object *value = NULL;
    return json_object_object_get_ex(entry, name, &value) &&
           json_object_is_type(value, json_type_int) ? json_object_get_int64(value) : fallback;
}

static int migrate_file(sms_db_t *db, const char *path, const char *modem_id,
                        const char *array_name, const char *direction, int *imported)
{
    json_object *root = json_object_from_file(path), *array = NULL;
    sqlite3_stmt *stmt = NULL;
    int rc = -1;

    if (!root)
        return access(path, F_OK) == 0 ? -1 : -2;
    if (!json_object_object_get_ex(root, array_name, &array) ||
        !json_object_is_type(array, json_type_array))
        goto out;
    if (exec_sql(db->sql, "BEGIN IMMEDIATE") != 0)
        goto out;
    for (size_t i = 0; i < json_object_array_length(array); i++) {
        json_object *entry = json_object_array_get_idx(array, i);
        const char *party, *content;
        char key[96];
        int64_t old_id, timestamp;
        int seen;
        if (!entry || !json_object_is_type(entry, json_type_object))
            goto rollback;
        old_id = json_integer(entry, "id", -1);
        timestamp = json_integer(entry, "timestamp", -1);
        content = json_string(entry, "content", NULL);
        party = json_string(entry, !strcmp(direction, "received") ? "sender" : "recipient", NULL);
        if (old_id < 0 || timestamp < 0 || !content || !party)
            goto rollback;
        snprintf(key, sizeof(key), "%s:%lld", direction, (long long)old_id);
        seen = already_imported(db->sql, path, key);
        if (seen < 0)
            goto rollback;
        if (seen)
            continue;
        if (sqlite3_prepare_v2(db->sql,
                "INSERT INTO messages(modem_id,direction,sender,recipient,timestamp,content,complete,is_read,send_success,created_at,updated_at) "
                "VALUES(?,?,?,?,?,?,1,?,?,strftime('%s','now'),strftime('%s','now'))",
                -1, &stmt, NULL) != SQLITE_OK)
            goto rollback;
        bind_text(stmt, 1, modem_id);
        bind_text(stmt, 2, direction);
        if (!strcmp(direction, "received")) {
            bind_text(stmt, 3, party);
            sqlite3_bind_null(stmt, 4);
        } else {
            sqlite3_bind_null(stmt, 3);
            bind_text(stmt, 4, party);
        }
        sqlite3_bind_int64(stmt, 5, timestamp);
        bind_text(stmt, 6, content);
        sqlite3_bind_int(stmt, 7, json_integer(entry, "is_read", 0) != 0);
        if (!strcmp(direction, "sent"))
            sqlite3_bind_int(stmt, 8, json_integer(entry, "is_success", 0) != 0);
        else
            sqlite3_bind_null(stmt, 8);
        if (sqlite3_step(stmt) != SQLITE_DONE)
            goto rollback;
        sqlite3_finalize(stmt);
        stmt = NULL;
        if (remember(db->sql, path, key) != 0)
            goto rollback;
        (*imported)++;
    }
    if (exec_sql(db->sql, "COMMIT") != 0)
        goto out;
    rc = 0;
    goto out;
rollback:
    sqlite3_finalize(stmt);
    stmt = NULL;
    exec_sql(db->sql, "ROLLBACK");
out:
    sqlite3_finalize(stmt);
    json_object_put(root);
    return rc;
}

int sms_legacy_migrate(sms_db_t *db, const char *directory,
                       const char *modem_id, int *imported)
{
    char sent[PATH_MAX], received[PATH_MAX], error[256];
    int rc;
    *imported = 0;
    if (snprintf(sent, sizeof(sent), "%s/%s_sent.json", directory, modem_id) >= (int)sizeof(sent) ||
        snprintf(received, sizeof(received), "%s/%s_received.json", directory, modem_id) >= (int)sizeof(received))
        return -1;
    rc = migrate_file(db, sent, modem_id, "sent", "sent", imported);
    if (rc == -2)
        rc = 0;
    if (rc == 0) {
        rc = migrate_file(db, received, modem_id, "received", "received", imported);
        if (rc == -2)
            rc = 0;
    }
    if (rc != 0) {
        snprintf(error, sizeof(error), "legacy JSON migration failed in %s", directory);
        set_error(db, modem_id, error);
        return -1;
    }
    return set_error(db, modem_id, NULL);
}
