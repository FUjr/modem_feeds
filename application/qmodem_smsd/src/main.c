#include "sms_db.h"
#include "legacy_migrate.h"

#include <errno.h>
#include <fcntl.h>
#include <json-c/json.h>
#include <libubox/blobmsg_json.h>
#include <libubox/uloop.h>
#include <libubus.h>
#include <qmodem-sms/pdu.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <uci.h>
#include <unistd.h>

#define DEFAULT_DB "/etc/qmodem/sms.sqlite3"
#define TOM_MODEM "/usr/bin/tom_modem"
#define QMODEM_SETTINGS "/usr/sbin/qmodem-settings"
#define SCHEDULER_TICK_MS 30000

struct modem_config {
    char section[64];
    char at_port[128];
    char mode[32];
    char storage[8];
    char legacy_dir[256];
    int use_ubus;
    int auto_delete;
    int forwarding;
    int poll_interval;
};

struct app_context {
    struct ubus_context *ubus;
    struct ubus_object object;
    struct ubus_event_handler urc_handler;
    sms_db_t db;
    char db_path[256];
    char last_sync_section[64];
    char last_sync_error[160];
    int64_t last_sync_at;
    int last_sync_imported;
    struct uloop_timeout scheduler;
};

static struct app_context app;
static int forwarder_enabled_for(const char *section_name);

enum {
    ARG_MODEM,
    ARG_MODE,
    ARG_POLL_INTERVAL,
    ARG_FORWARDING,
    ARG_AUTO_DELETE,
    ARG_LIMIT,
    ARG_OFFSET,
    ARG_ID,
    ARG_RECIPIENT,
    ARG_CONTENT,
    ARG_INDEX,
    ARG_MEM1,
    ARG_MEM2,
    ARG_MEM3,
    ARG_DELIVERY_ID,
    ARG_SUCCESS,
    ARG_ERROR,
    __ARG_MAX
};

static const struct blobmsg_policy policy[] = {
    [ARG_MODEM] = { .name = "modem_id", .type = BLOBMSG_TYPE_STRING },
    [ARG_MODE] = { .name = "mode", .type = BLOBMSG_TYPE_STRING },
    [ARG_POLL_INTERVAL] = { .name = "poll_interval", .type = BLOBMSG_TYPE_INT32 },
    [ARG_FORWARDING] = { .name = "forwarding", .type = BLOBMSG_TYPE_BOOL },
    [ARG_AUTO_DELETE] = { .name = "auto_delete", .type = BLOBMSG_TYPE_BOOL },
    [ARG_LIMIT] = { .name = "limit", .type = BLOBMSG_TYPE_INT32 },
    [ARG_OFFSET] = { .name = "offset", .type = BLOBMSG_TYPE_INT32 },
    [ARG_ID] = { .name = "id", .type = BLOBMSG_TYPE_INT64 },
    [ARG_RECIPIENT] = { .name = "recipient", .type = BLOBMSG_TYPE_STRING },
    [ARG_CONTENT] = { .name = "content", .type = BLOBMSG_TYPE_STRING },
    [ARG_INDEX] = { .name = "index", .type = BLOBMSG_TYPE_INT32 },
    [ARG_MEM1] = { .name = "mem1", .type = BLOBMSG_TYPE_STRING },
    [ARG_MEM2] = { .name = "mem2", .type = BLOBMSG_TYPE_STRING },
    [ARG_MEM3] = { .name = "mem3", .type = BLOBMSG_TYPE_STRING },
    [ARG_DELIVERY_ID] = { .name = "delivery_id", .type = BLOBMSG_TYPE_INT64 },
    [ARG_SUCCESS] = { .name = "success", .type = BLOBMSG_TYPE_BOOL },
    [ARG_ERROR] = { .name = "error", .type = BLOBMSG_TYPE_STRING },
};

static int valid_id(const char *value)
{
    if (!value || !*value || strlen(value) >= 64)
        return 0;
    for (; *value; value++)
        if (!((*value >= 'a' && *value <= 'z') || (*value >= 'A' && *value <= 'Z') ||
              (*value >= '0' && *value <= '9') || *value == '_' || *value == '-'))
            return 0;
    return 1;
}

static const char *option_string(struct uci_context *uci, struct uci_section *section,
                                 const char *name, const char *fallback)
{
    const char *value = uci_lookup_option_string(uci, section, name);
    return value && *value ? value : fallback;
}

static int load_config(const char *section_name, struct modem_config *cfg)
{
    struct uci_context *uci = uci_alloc_context();
    struct uci_package *package = NULL;
    struct uci_section *section;
    const char *value;

    if (!uci || !valid_id(section_name) ||
        uci_load(uci, "qmodem", &package) != UCI_OK)
        goto fail;
    section = uci_lookup_section(uci, package, section_name);
    if (!section)
        goto fail;
    memset(cfg, 0, sizeof(*cfg));
    snprintf(cfg->section, sizeof(cfg->section), "%s", section_name);
    snprintf(cfg->at_port, sizeof(cfg->at_port), "%s",
             option_string(uci, section, "override_at_port",
             option_string(uci, section, "sms_at_port",
             option_string(uci, section, "at_port", ""))));
    snprintf(cfg->mode, sizeof(cfg->mode), "%s",
             option_string(uci, section, "sms_mode", "database_poll"));
    snprintf(cfg->storage, sizeof(cfg->storage), "%s",
             option_string(uci, section, "sms_storage_mem1", "SM"));
    snprintf(cfg->legacy_dir, sizeof(cfg->legacy_dir), "%s",
             option_string(uci, section, "sms_db_path", "/etc/qmodem"));
    cfg->use_ubus = !strcmp(option_string(uci, section, "use_ubus", "0"), "1");
    cfg->auto_delete = strcmp(option_string(uci, section,
                              "sms_auto_delete_from_sim", "1"), "0") != 0;
    cfg->forwarding = !strcmp(option_string(uci, section, "sms_forwarding", "0"), "1");
    value = option_string(uci, section, "sms_poll_interval", "300");
    cfg->poll_interval = atoi(value);
    if (cfg->poll_interval < 60 || cfg->poll_interval > 3600)
        cfg->poll_interval = 300;
    uci_unload(uci, package);
    uci_free_context(uci);
    cfg->forwarding = cfg->forwarding || forwarder_enabled_for(section_name);
    return cfg->at_port[0] ? 0 : -1;
fail:
    if (package)
        uci_unload(uci, package);
    if (uci)
        uci_free_context(uci);
    return -1;
}

static int forwarder_enabled_for(const char *section_name)
{
    struct uci_context *uci = uci_alloc_context();
    struct uci_package *package = NULL;
    struct uci_element *element;
    int enabled = 0;

    if (!uci || uci_load(uci, "sms_forwarder", &package) != UCI_OK)
        goto out;
    uci_foreach_element(&package->sections, element) {
        struct uci_section *section = uci_to_section(element);
        if (!strcmp(section->type, "sms_forward") &&
            !strcmp(option_string(uci, section, "enable", "0"), "1"))
            enabled = 1;
    }
    if (!enabled)
        goto out;
    enabled = 0;
    uci_foreach_element(&package->sections, element) {
        struct uci_section *section = uci_to_section(element);
        if (!strcmp(section->type, "sms_forward_instance") &&
            !strcmp(option_string(uci, section, "enable", "0"), "1") &&
            !strcmp(option_string(uci, section, "modem_cfg", ""), section_name)) {
            enabled = 1;
            break;
        }
    }
out:
    if (package)
        uci_unload(uci, package);
    if (uci)
        uci_free_context(uci);
    return enabled;
}

static int prepare_database_mode(const struct modem_config *cfg)
{
    char error[256];
    int imported;

    if (!strcmp(cfg->mode, "direct"))
        return 0;
    if (sms_legacy_migrate(&app.db, cfg->legacy_dir, cfg->section, &imported) != 0)
        return -1;
    return sms_db_migration_error(&app.db, cfg->section, error, sizeof(error)) > 0 ? -1 : 0;
}

static int set_uci_option(const char *section, const char *option, const char *value)
{
    struct uci_context *uci = uci_alloc_context();
    struct uci_ptr ptr = { 0 };
    struct uci_package *package = NULL;
    char expression[256];
    int result = -1;

    if (!uci || !valid_id(section) || !valid_id(option) || !value ||
        snprintf(expression, sizeof(expression), "qmodem.%s.%s=%s",
                 section, option, value) >= (int)sizeof(expression))
        goto out;
    if (uci_lookup_ptr(uci, &ptr, expression, true) != UCI_OK ||
        uci_set(uci, &ptr) != UCI_OK)
        goto out;
    package = ptr.p;
    if (uci_commit(uci, &package, false) != UCI_OK)
        goto out;
    result = 0;
out:
    if (uci)
        uci_free_context(uci);
    return result;
}

static int read_child_output(char *const argv[], char **output)
{
    int pipefd[2], status;
    pid_t child;
    char *buffer = NULL;
    size_t used = 0, capacity = 0;

    *output = NULL;
    if (pipe2(pipefd, O_CLOEXEC) != 0)
        return -1;
    child = fork();
    if (child == 0) {
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[0]); close(pipefd[1]);
        execv(argv[0], argv);
        _exit(127);
    }
    close(pipefd[1]);
    if (child < 0) {
        close(pipefd[0]);
        return -1;
    }
    for (;;) {
        ssize_t count;
        if (capacity - used < 4096) {
            char *grown;
            capacity = capacity ? capacity * 2 : 8192;
            grown = realloc(buffer, capacity);
            if (!grown) { free(buffer); close(pipefd[0]); kill(child, SIGKILL); waitpid(child, NULL, 0); return -1; }
            buffer = grown;
        }
        count = read(pipefd[0], buffer + used, capacity - used - 1);
        if (count > 0) { used += (size_t)count; continue; }
        if (count < 0 && errno == EINTR) continue;
        break;
    }
    close(pipefd[0]);
    waitpid(child, &status, 0);
    if (!buffer)
        buffer = strdup("");
    if (buffer)
        buffer[used] = '\0';
    *output = buffer;
    return WIFEXITED(status) && WEXITSTATUS(status) == 0 ? 0 : -1;
}

static int run_tom(const struct modem_config *cfg, const char *operation,
                   const char *pdu, int index, const char *at_command,
                   char **output)
{
    char index_string[24];
    char *argv[14];
    int n = 0;

    argv[n++] = TOM_MODEM;
    if (cfg->use_ubus || strcmp(cfg->mode, "direct"))
        argv[n++] = "-u";
    argv[n++] = "-d"; argv[n++] = (char *)cfg->at_port;
    argv[n++] = "-o"; argv[n++] = (char *)operation;
    if (pdu) { argv[n++] = "-p"; argv[n++] = (char *)pdu; }
    if (index >= 0) {
        snprintf(index_string, sizeof(index_string), "%d", index);
        argv[n++] = "-i"; argv[n++] = index_string;
    }
    if (at_command) { argv[n++] = "-c"; argv[n++] = (char *)at_command; }
    argv[n] = NULL;
    return read_child_output(argv, output);
}

static void reply_error(struct ubus_context *ctx, struct ubus_request_data *req,
                        const char *error)
{
    struct blob_buf b = {};
    blob_buf_init(&b, 0);
    blobmsg_add_string(&b, "status", "error");
    blobmsg_add_string(&b, "error", error);
    ubus_send_reply(ctx, req, b.head);
    blob_buf_free(&b);
}

static int apply_modem_settings(const char *section)
{
    char *argv[] = { QMODEM_SETTINGS, "apply", (char *)section, NULL };
    char *output = NULL;
    int result = read_child_output(argv, &output);
    free(output);
    return result;
}

static int sync_modem(const char *section, const char *trigger, int *imported,
                      int *deleted)
{
    struct modem_config cfg;
    struct json_object *root = NULL, *messages = NULL;
    char *output = NULL;
    int result = -1;
    int64_t started_at = time(NULL);

    *imported = *deleted = 0;
    snprintf(app.last_sync_section, sizeof(app.last_sync_section), "%s", section);
    app.last_sync_at = time(NULL);
    app.last_sync_error[0] = '\0';
    if (load_config(section, &cfg) != 0) {
        snprintf(app.last_sync_error, sizeof(app.last_sync_error), "modem configuration unavailable");
        return -1;
    }
    if (!strcmp(cfg.mode, "direct")) {
        snprintf(app.last_sync_error, sizeof(app.last_sync_error), "direct mode has no database sync");
        return -1;
    }
    if (!cfg.use_ubus) {
        snprintf(app.last_sync_error, sizeof(app.last_sync_error), "database mode requires use_ubus");
        return -1;
    }
    if (prepare_database_mode(&cfg) != 0) {
        snprintf(app.last_sync_error, sizeof(app.last_sync_error), "legacy SMS database migration failed");
        return -1;
    }
    (void)trigger;
    if (run_tom(&cfg, "r", NULL, -1, NULL, &output) != 0)
        goto out;
    root = json_tokener_parse(output);
    if (!root || (!json_object_object_get_ex(root, "msg", &messages) &&
                  !json_object_object_get_ex(root, "received", &messages)) ||
        !json_object_is_type(messages, json_type_array))
        goto out;
    for (size_t i = 0; i < json_object_array_length(messages); i++) {
        struct json_object *entry = json_object_array_get_idx(messages, i);
        struct json_object *field;
        sms_segment_t segment = { .modem_id = section, .storage = cfg.storage,
                                  .total_parts = 1, .part_number = 1 };
        sms_import_result_t import_result;
        char *delete_output = NULL;
        if (!json_object_object_get_ex(entry, "index", &field))
            continue;
        segment.source_index = json_object_get_int(field);
        if (!json_object_object_get_ex(entry, "pdu", &field))
            continue;
        segment.pdu = json_object_get_string(field);
        if (!json_object_object_get_ex(entry, "sender", &field))
            continue;
        segment.sender = json_object_get_string(field);
        if (!json_object_object_get_ex(entry, "timestamp", &field))
            continue;
        segment.timestamp = json_object_get_int64(field);
        if (!json_object_object_get_ex(entry, "content", &field))
            continue;
        segment.content = json_object_get_string(field);
        if (json_object_object_get_ex(entry, "reference", &field))
            segment.reference = json_object_get_int(field);
        if (json_object_object_get_ex(entry, "total", &field))
            segment.total_parts = json_object_get_int(field);
        if (json_object_object_get_ex(entry, "part", &field))
            segment.part_number = json_object_get_int(field);
        if (sms_db_import_segment(&app.db, &segment, time(NULL), cfg.forwarding,
                                  &import_result) != 0)
            goto out;
        if (!import_result.duplicate)
            (*imported)++;
        if (cfg.auto_delete && import_result.safe_to_delete &&
            run_tom(&cfg, "d", NULL, segment.source_index, NULL, &delete_output) == 0) {
            (*deleted)++;
            (void)sms_db_mark_source_deleted(&app.db, import_result.source_id, time(NULL));
        }
        free(delete_output);
    }
    {
        int expired;
        if (sms_db_publish_expired(&app.db, section, time(NULL), cfg.forwarding,
                                   &expired) != 0)
            goto out;
    }
    app.last_sync_imported = *imported;
    if (sms_db_prune(&app.db, section, 5000, 1000) != 0)
        goto out;
    result = 0;
out:
    if (result != 0 && !app.last_sync_error[0])
        snprintf(app.last_sync_error, sizeof(app.last_sync_error), "modem read or database import failed");
    if (root)
        json_object_put(root);
    free(output);
    (void)sms_db_record_sync(&app.db, section, trigger, started_at, *imported,
                             result == 0 ? NULL : app.last_sync_error);
    return result;
}

static int status_method(struct ubus_context *ctx, struct ubus_object *obj,
                         struct ubus_request_data *req, const char *method,
                         struct blob_attr *msg)
{
    struct blob_buf b = {};
    (void)obj; (void)method; (void)msg;
    blob_buf_init(&b, 0);
    blobmsg_add_string(&b, "status", "ready");
    blobmsg_add_string(&b, "database", app.db_path);
    blobmsg_add_string(&b, "default_mode", "database_poll");
    blobmsg_add_u32(&b, "default_poll_interval", 300);
    ubus_send_reply(ctx, req, b.head);
    blob_buf_free(&b);
    return UBUS_STATUS_OK;
}

static int sync_status_method(struct ubus_context *ctx, struct ubus_object *obj,
                              struct ubus_request_data *req, const char *method,
                              struct blob_attr *msg)
{
    struct blob_buf b = {};
    (void)obj; (void)method; (void)msg;
    blob_buf_init(&b, 0);
    blobmsg_add_string(&b, "modem_id", app.last_sync_section);
    blobmsg_add_u64(&b, "last_sync_at", app.last_sync_at);
    blobmsg_add_u32(&b, "imported", app.last_sync_imported);
    blobmsg_add_string(&b, "status", app.last_sync_error[0] ? "error" : "idle");
    if (app.last_sync_error[0])
        blobmsg_add_string(&b, "error", app.last_sync_error);
    ubus_send_reply(ctx, req, b.head);
    blob_buf_free(&b);
    return UBUS_STATUS_OK;
}

static int sync_method(struct ubus_context *ctx, struct ubus_object *obj,
                       struct ubus_request_data *req, const char *method,
                       struct blob_attr *msg)
{
    struct blob_attr *tb[__ARG_MAX];
    struct blob_buf b = {};
    int imported, deleted;
    (void)obj; (void)method;
    blobmsg_parse(policy, __ARG_MAX, tb, blob_data(msg), blob_len(msg));
    if (!tb[ARG_MODEM])
        return UBUS_STATUS_INVALID_ARGUMENT;
    if (sync_modem(blobmsg_get_string(tb[ARG_MODEM]), "manual", &imported, &deleted) != 0) {
        reply_error(ctx, req, app.last_sync_error);
        return UBUS_STATUS_OK;
    }
    blob_buf_init(&b, 0);
    blobmsg_add_string(&b, "status", "success");
    blobmsg_add_u32(&b, "imported", imported);
    blobmsg_add_u32(&b, "deleted_from_modem", deleted);
    ubus_send_reply(ctx, req, b.head);
    blob_buf_free(&b);
    return UBUS_STATUS_OK;
}

static int configure_method(struct ubus_context *ctx, struct ubus_object *obj,
                            struct ubus_request_data *req, const char *method,
                            struct blob_attr *msg)
{
    struct blob_attr *tb[__ARG_MAX];
    struct modem_config cfg;
    struct blob_buf b = {};
    const char *section, *mode_name;
    char number[24];
    int pending = 0;
    (void)obj; (void)method;
    blobmsg_parse(policy, __ARG_MAX, tb, blob_data(msg), blob_len(msg));
    if (!tb[ARG_MODEM] || !valid_id(section = blobmsg_get_string(tb[ARG_MODEM])))
        return UBUS_STATUS_INVALID_ARGUMENT;
    if (tb[ARG_MODE]) {
        mode_name = blobmsg_get_string(tb[ARG_MODE]);
        if (strcmp(mode_name, "direct") && strcmp(mode_name, "database_poll") &&
            strcmp(mode_name, "database_urc"))
            return UBUS_STATUS_INVALID_ARGUMENT;
        if (set_uci_option(section, "sms_mode", mode_name) != 0)
            goto error;
        if (!strcmp(mode_name, "database_urc") &&
            set_uci_option(section, "use_ubus", "1") != 0)
            goto error;
    }
    if (tb[ARG_POLL_INTERVAL]) {
        int interval = blobmsg_get_u32(tb[ARG_POLL_INTERVAL]);
        if (interval < 60 || interval > 3600)
            return UBUS_STATUS_INVALID_ARGUMENT;
        snprintf(number, sizeof(number), "%d", interval);
        if (set_uci_option(section, "sms_poll_interval", number) != 0)
            goto error;
    }
    if (tb[ARG_FORWARDING] && set_uci_option(section, "sms_forwarding",
            blobmsg_get_bool(tb[ARG_FORWARDING]) ? "1" : "0") != 0)
        goto error;
    if (tb[ARG_AUTO_DELETE] && set_uci_option(section, "sms_auto_delete_from_sim",
            blobmsg_get_bool(tb[ARG_AUTO_DELETE]) ? "1" : "0") != 0)
        goto error;
    if (load_config(section, &cfg) != 0)
        goto error;
    if (apply_modem_settings(section) != 0)
        pending = 1;
    blob_buf_init(&b, 0);
    blobmsg_add_string(&b, "status", pending ? "pending" : "success");
    blobmsg_add_string(&b, "mode", cfg.mode);
    blobmsg_add_u32(&b, "use_ubus", cfg.use_ubus);
    blobmsg_add_u32(&b, "poll_interval", cfg.poll_interval);
    ubus_send_reply(ctx, req, b.head);
    blob_buf_free(&b);
    return UBUS_STATUS_OK;
error:
    reply_error(ctx, req, "failed to persist SMS configuration");
    return UBUS_STATUS_OK;
}

static int config_get_method(struct ubus_context *ctx, struct ubus_object *obj,
                             struct ubus_request_data *req, const char *method,
                             struct blob_attr *msg)
{
    struct blob_attr *tb[__ARG_MAX];
    struct modem_config cfg;
    struct blob_buf b = {};
    (void)obj; (void)method;
    blobmsg_parse(policy, __ARG_MAX, tb, blob_data(msg), blob_len(msg));
    if (!tb[ARG_MODEM] ||
        load_config(blobmsg_get_string(tb[ARG_MODEM]), &cfg) != 0)
        return UBUS_STATUS_INVALID_ARGUMENT;
    blob_buf_init(&b, 0);
    blobmsg_add_string(&b, "status", "success");
    blobmsg_add_string(&b, "mode", cfg.mode);
    blobmsg_add_u8(&b, "use_ubus", cfg.use_ubus);
    blobmsg_add_u8(&b, "auto_delete", cfg.auto_delete);
    blobmsg_add_u8(&b, "forwarding", cfg.forwarding);
    blobmsg_add_u32(&b, "poll_interval", cfg.poll_interval);
    ubus_send_reply(ctx, req, b.head);
    blob_buf_free(&b);
    return UBUS_STATUS_OK;
}

static int list_method(struct ubus_context *ctx, struct ubus_object *obj,
                       struct ubus_request_data *req, const char *method,
                       struct blob_attr *msg)
{
    struct blob_attr *tb[__ARG_MAX];
    struct modem_config cfg;
    struct blob_buf b = {};
    sqlite3_stmt *stmt = NULL;
    const char *section;
    int limit = 100, offset = 0;
    void *array;
    (void)obj; (void)method;
    blobmsg_parse(policy, __ARG_MAX, tb, blob_data(msg), blob_len(msg));
    if (!tb[ARG_MODEM])
        return UBUS_STATUS_INVALID_ARGUMENT;
    section = blobmsg_get_string(tb[ARG_MODEM]);
    if (load_config(section, &cfg) != 0) {
        reply_error(ctx, req, "modem configuration unavailable");
        return UBUS_STATUS_OK;
    }
    if (tb[ARG_LIMIT]) limit = blobmsg_get_u32(tb[ARG_LIMIT]);
    if (tb[ARG_OFFSET]) offset = blobmsg_get_u32(tb[ARG_OFFSET]);
    if (limit < 1 || limit > 500) limit = 100;
    blob_buf_init(&b, 0);
    blobmsg_add_string(&b, "mode", cfg.mode);
    blobmsg_add_u8(&b, "use_ubus", cfg.use_ubus);
    blobmsg_add_u8(&b, "auto_delete", cfg.auto_delete);
    blobmsg_add_u8(&b, "forwarding", cfg.forwarding);
    blobmsg_add_u32(&b, "poll_interval", cfg.poll_interval);
    if (!strcmp(cfg.mode, "direct")) {
        char *output = NULL;
        if (run_tom(&cfg, "r", NULL, -1, NULL, &output) != 0 ||
            !blobmsg_add_json_from_string(&b, output ? output : "{}")) {
            free(output); blob_buf_free(&b);
            reply_error(ctx, req, "direct modem read failed");
            return UBUS_STATUS_OK;
        }
        free(output);
    } else {
        const char *query =
            "SELECT id,direction,sender,recipient,timestamp,content,complete,revision,is_read,send_success "
            "FROM messages WHERE modem_id=? ORDER BY timestamp DESC,id DESC LIMIT ? OFFSET ?";
        if (prepare_database_mode(&cfg) != 0) {
            blob_buf_free(&b);
            reply_error(ctx, req, "legacy SMS database migration failed");
            return UBUS_STATUS_OK;
        }
        if (sqlite3_prepare_v2(app.db.sql, query, -1, &stmt, NULL) != SQLITE_OK)
            goto db_error;
        sqlite3_bind_text(stmt, 1, section, -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt, 2, limit);
        sqlite3_bind_int(stmt, 3, offset);
        array = blobmsg_open_array(&b, "messages");
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            void *entry = blobmsg_open_table(&b, NULL);
            blobmsg_add_u64(&b, "id", sqlite3_column_int64(stmt, 0));
            blobmsg_add_string(&b, "type", (const char *)sqlite3_column_text(stmt, 1));
            if (sqlite3_column_type(stmt, 2) != SQLITE_NULL)
                blobmsg_add_string(&b, "sender", (const char *)sqlite3_column_text(stmt, 2));
            if (sqlite3_column_type(stmt, 3) != SQLITE_NULL)
                blobmsg_add_string(&b, "recipient", (const char *)sqlite3_column_text(stmt, 3));
            blobmsg_add_u64(&b, "timestamp", sqlite3_column_int64(stmt, 4));
            blobmsg_add_string(&b, "content", (const char *)sqlite3_column_text(stmt, 5));
            blobmsg_add_u8(&b, "complete", sqlite3_column_int(stmt, 6));
            blobmsg_add_u32(&b, "revision", sqlite3_column_int(stmt, 7));
            blobmsg_add_u8(&b, "is_read", sqlite3_column_int(stmt, 8));
            if (sqlite3_column_type(stmt, 9) != SQLITE_NULL)
                blobmsg_add_u8(&b, "success", sqlite3_column_int(stmt, 9));
            blobmsg_close_table(&b, entry);
        }
        blobmsg_close_array(&b, array);
        sqlite3_finalize(stmt);
    }
    ubus_send_reply(ctx, req, b.head);
    blob_buf_free(&b);
    return UBUS_STATUS_OK;
db_error:
    sqlite3_finalize(stmt);
    blob_buf_free(&b);
    reply_error(ctx, req, sms_db_error(&app.db));
    return UBUS_STATUS_OK;
}

static int get_method(struct ubus_context *ctx, struct ubus_object *obj,
                      struct ubus_request_data *req, const char *method,
                      struct blob_attr *msg)
{
    struct blob_attr *tb[__ARG_MAX];
    sqlite3_stmt *stmt = NULL;
    struct blob_buf b = {};
    const char *section;
    (void)obj; (void)method;

    blobmsg_parse(policy, __ARG_MAX, tb, blob_data(msg), blob_len(msg));
    if (!tb[ARG_MODEM] || !tb[ARG_ID])
        return UBUS_STATUS_INVALID_ARGUMENT;
    section = blobmsg_get_string(tb[ARG_MODEM]);
    if (sqlite3_prepare_v2(app.db.sql,
            "SELECT id,direction,sender,recipient,timestamp,content,complete,revision,is_read,send_success "
            "FROM messages WHERE id=? AND modem_id=?", -1, &stmt, NULL) != SQLITE_OK)
        goto error;
    sqlite3_bind_int64(stmt, 1, blobmsg_get_u64(tb[ARG_ID]));
    sqlite3_bind_text(stmt, 2, section, -1, SQLITE_TRANSIENT);
    if (sqlite3_step(stmt) != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        return UBUS_STATUS_NOT_FOUND;
    }
    blob_buf_init(&b, 0);
    blobmsg_add_u64(&b, "id", sqlite3_column_int64(stmt, 0));
    blobmsg_add_string(&b, "type", (const char *)sqlite3_column_text(stmt, 1));
    if (sqlite3_column_type(stmt, 2) != SQLITE_NULL)
        blobmsg_add_string(&b, "sender", (const char *)sqlite3_column_text(stmt, 2));
    if (sqlite3_column_type(stmt, 3) != SQLITE_NULL)
        blobmsg_add_string(&b, "recipient", (const char *)sqlite3_column_text(stmt, 3));
    blobmsg_add_u64(&b, "timestamp", sqlite3_column_int64(stmt, 4));
    blobmsg_add_string(&b, "content", (const char *)sqlite3_column_text(stmt, 5));
    blobmsg_add_u8(&b, "complete", sqlite3_column_int(stmt, 6));
    blobmsg_add_u32(&b, "revision", sqlite3_column_int(stmt, 7));
    blobmsg_add_u8(&b, "is_read", sqlite3_column_int(stmt, 8));
    if (sqlite3_column_type(stmt, 9) != SQLITE_NULL)
        blobmsg_add_u8(&b, "success", sqlite3_column_int(stmt, 9));
    sqlite3_finalize(stmt);
    ubus_send_reply(ctx, req, b.head);
    blob_buf_free(&b);
    return UBUS_STATUS_OK;
error:
    sqlite3_finalize(stmt);
    reply_error(ctx, req, sms_db_error(&app.db));
    return UBUS_STATUS_OK;
}

static int send_method(struct ubus_context *ctx, struct ubus_object *obj,
                       struct ubus_request_data *req, const char *method,
                       struct blob_attr *msg)
{
    struct blob_attr *tb[__ARG_MAX];
    struct modem_config cfg;
    unsigned char pdus[SMS_MAX_PARTS][SMS_MAX_PDU_LENGTH];
    int lengths[SMS_MAX_PARTS];
    char hex[SMS_MAX_PDU_LENGTH * 2 + 1];
    const char *recipient, *content, *number;
    int parts, success = 1;
    char *output = NULL;
    sqlite3_stmt *stmt = NULL;
    struct blob_buf b = {};
    (void)obj; (void)method;
    blobmsg_parse(policy, __ARG_MAX, tb, blob_data(msg), blob_len(msg));
    if (!tb[ARG_MODEM] || !tb[ARG_RECIPIENT] || !tb[ARG_CONTENT])
        return UBUS_STATUS_INVALID_ARGUMENT;
    if (load_config(blobmsg_get_string(tb[ARG_MODEM]), &cfg) != 0)
        goto error;
    if (prepare_database_mode(&cfg) != 0)
        goto error;
    recipient = blobmsg_get_string(tb[ARG_RECIPIENT]);
    content = blobmsg_get_string(tb[ARG_CONTENT]);
    number = recipient[0] == '+' ? recipient + 1 : recipient;
    if (!*number || strlen(number) > 20)
        return UBUS_STATUS_INVALID_ARGUMENT;
    for (const char *p = number; *p; p++)
        if (*p < '0' || *p > '9')
            return UBUS_STATUS_INVALID_ARGUMENT;
    parts = pdu_encode_ucs2_parts(number, content, pdus, lengths, SMS_MAX_PARTS,
                                  (unsigned int)time(NULL));
    if (parts < 1)
        goto error;
    for (int part = 0; part < parts; part++) {
        free(output); output = NULL;
        for (int i = 0; i < lengths[part]; i++)
            snprintf(hex + i * 2, 3, "%02X", pdus[part][i]);
        hex[lengths[part] * 2] = '\0';
        if (run_tom(&cfg, "s", hex, -1, NULL, &output) != 0) {
            success = 0;
            break;
        }
    }
    if (strcmp(cfg.mode, "direct")) {
        if (sqlite3_prepare_v2(app.db.sql,
                "INSERT INTO messages(modem_id,direction,recipient,timestamp,content,complete,send_success,created_at,updated_at) "
                "VALUES(?,'sent',?,strftime('%s','now'),?,1,?,strftime('%s','now'),strftime('%s','now'))",
                -1, &stmt, NULL) != SQLITE_OK)
            goto error;
        sqlite3_bind_text(stmt, 1, cfg.section, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, recipient, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, content, -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt, 4, success);
        if (sqlite3_step(stmt) != SQLITE_DONE)
            goto error;
        sqlite3_finalize(stmt); stmt = NULL;
    }
    blob_buf_init(&b, 0);
    blobmsg_add_string(&b, "status", success ? "success" : "error");
    blobmsg_add_u32(&b, "parts", parts);
    if (output && *output)
        blobmsg_add_string(&b, "modem_response", output);
    ubus_send_reply(ctx, req, b.head);
    blob_buf_free(&b);
    free(output);
    return UBUS_STATUS_OK;
error:
    sqlite3_finalize(stmt);
    free(output);
    reply_error(ctx, req, "SMS encoding, transport, or history write failed");
    return UBUS_STATUS_OK;
}

static int delete_method(struct ubus_context *ctx, struct ubus_object *obj,
                         struct ubus_request_data *req, const char *method,
                         struct blob_attr *msg)
{
    struct blob_attr *tb[__ARG_MAX];
    struct modem_config cfg;
    struct blob_buf b = {};
    int changed = 0;
    char *output = NULL;
    (void)obj; (void)method;
    blobmsg_parse(policy, __ARG_MAX, tb, blob_data(msg), blob_len(msg));
    if (!tb[ARG_MODEM] || load_config(blobmsg_get_string(tb[ARG_MODEM]), &cfg) != 0)
        return UBUS_STATUS_INVALID_ARGUMENT;
    if (!strcmp(cfg.mode, "direct")) {
        if (!tb[ARG_INDEX] || run_tom(&cfg, "d", NULL, blobmsg_get_u32(tb[ARG_INDEX]),
                                     NULL, &output) != 0)
            goto error;
        changed = 1;
    } else {
        if (!tb[ARG_ID] || sms_db_delete_message(&app.db, cfg.section,
                blobmsg_get_u64(tb[ARG_ID]), &changed) != 0)
            goto error;
    }
    blob_buf_init(&b, 0);
    blobmsg_add_string(&b, "status", "success");
    blobmsg_add_u32(&b, "deleted", changed);
    ubus_send_reply(ctx, req, b.head);
    blob_buf_free(&b);
    free(output);
    return UBUS_STATUS_OK;
error:
    free(output);
    reply_error(ctx, req, "delete failed");
    return UBUS_STATUS_OK;
}

static int mark_read_method(struct ubus_context *ctx, struct ubus_object *obj,
                            struct ubus_request_data *req, const char *method,
                            struct blob_attr *msg)
{
    struct blob_attr *tb[__ARG_MAX];
    sqlite3_stmt *stmt = NULL;
    struct blob_buf b = {};
    (void)obj; (void)method;
    blobmsg_parse(policy, __ARG_MAX, tb, blob_data(msg), blob_len(msg));
    if (!tb[ARG_MODEM] || !tb[ARG_ID])
        return UBUS_STATUS_INVALID_ARGUMENT;
    if (sqlite3_prepare_v2(app.db.sql,
            "UPDATE messages SET is_read=1,updated_at=strftime('%s','now') WHERE id=? AND modem_id=? AND direction='received'",
            -1, &stmt, NULL) != SQLITE_OK)
        goto error;
    sqlite3_bind_int64(stmt, 1, blobmsg_get_u64(tb[ARG_ID]));
    sqlite3_bind_text(stmt, 2, blobmsg_get_string(tb[ARG_MODEM]), -1, SQLITE_TRANSIENT);
    if (sqlite3_step(stmt) != SQLITE_DONE)
        goto error;
    sqlite3_finalize(stmt);
    blob_buf_init(&b, 0);
    blobmsg_add_string(&b, "status", "success");
    blobmsg_add_u32(&b, "marked", sqlite3_changes(app.db.sql));
    ubus_send_reply(ctx, req, b.head);
    blob_buf_free(&b);
    return UBUS_STATUS_OK;
error:
    sqlite3_finalize(stmt);
    reply_error(ctx, req, "mark read failed");
    return UBUS_STATUS_OK;
}

static int storage_get_method(struct ubus_context *ctx, struct ubus_object *obj,
                              struct ubus_request_data *req, const char *method,
                              struct blob_attr *msg)
{
    struct blob_attr *tb[__ARG_MAX];
    struct modem_config cfg;
    struct blob_buf b = {};
    char *output = NULL;
    (void)obj; (void)method;
    blobmsg_parse(policy, __ARG_MAX, tb, blob_data(msg), blob_len(msg));
    if (!tb[ARG_MODEM] || load_config(blobmsg_get_string(tb[ARG_MODEM]), &cfg) != 0)
        return UBUS_STATUS_INVALID_ARGUMENT;
    blob_buf_init(&b, 0);
    blobmsg_add_string(&b, "configured", cfg.storage);
    if (run_tom(&cfg, "a", NULL, -1, "AT+CPMS?", &output) == 0) {
        blobmsg_add_string(&b, "status", "success");
        blobmsg_add_string(&b, "response", output);
    } else {
        blobmsg_add_string(&b, "status", "pending_use_ubus");
    }
    ubus_send_reply(ctx, req, b.head);
    blob_buf_free(&b);
    free(output);
    return UBUS_STATUS_OK;
}

static int storage_set_method(struct ubus_context *ctx, struct ubus_object *obj,
                              struct ubus_request_data *req, const char *method,
                              struct blob_attr *msg)
{
    struct blob_attr *tb[__ARG_MAX];
    struct modem_config cfg;
    struct blob_buf b = {};
    const char *section, *mem1, *mem2, *mem3;
    (void)obj; (void)method;
    blobmsg_parse(policy, __ARG_MAX, tb, blob_data(msg), blob_len(msg));
    if (!tb[ARG_MODEM] || !tb[ARG_MEM1] || !tb[ARG_MEM2])
        return UBUS_STATUS_INVALID_ARGUMENT;
    section = blobmsg_get_string(tb[ARG_MODEM]);
    mem1 = blobmsg_get_string(tb[ARG_MEM1]); mem2 = blobmsg_get_string(tb[ARG_MEM2]);
    mem3 = tb[ARG_MEM3] ? blobmsg_get_string(tb[ARG_MEM3]) : mem2;
    if ((strcmp(mem1, "SM") && strcmp(mem1, "ME")) ||
        (strcmp(mem2, "SM") && strcmp(mem2, "ME")) ||
        (strcmp(mem3, "SM") && strcmp(mem3, "ME")))
        return UBUS_STATUS_INVALID_ARGUMENT;
    if (set_uci_option(section, "sms_storage_mem1", mem1) != 0 ||
        set_uci_option(section, "sms_storage_mem2", mem2) != 0 ||
        set_uci_option(section, "sms_storage_mem3", mem3) != 0 ||
        load_config(section, &cfg) != 0) {
        reply_error(ctx, req, "failed to save storage configuration");
        return UBUS_STATUS_OK;
    }
    blob_buf_init(&b, 0);
    if (!cfg.use_ubus)
        blobmsg_add_string(&b, "status", "pending_use_ubus");
    else if (apply_modem_settings(section) == 0)
        blobmsg_add_string(&b, "status", "success");
    else
        blobmsg_add_string(&b, "status", "ready_degraded");
    ubus_send_reply(ctx, req, b.head);
    blob_buf_free(&b);
    return UBUS_STATUS_OK;
}

static int modem_list_method(struct ubus_context *ctx, struct ubus_object *obj,
                             struct ubus_request_data *req, const char *method,
                             struct blob_attr *msg)
{
    struct uci_context *uci = uci_alloc_context();
    struct uci_package *package = NULL;
    struct uci_element *element;
    struct blob_buf b = {};
    void *array;
    (void)obj; (void)method; (void)msg;
    if (!uci || uci_load(uci, "qmodem", &package) != UCI_OK) {
        if (uci) uci_free_context(uci);
        reply_error(ctx, req, "qmodem UCI unavailable");
        return UBUS_STATUS_OK;
    }
    blob_buf_init(&b, 0);
    array = blobmsg_open_array(&b, "modems");
    uci_foreach_element(&package->sections, element) {
        struct uci_section *section = uci_to_section(element);
        if (strcmp(section->type, "modem-device"))
            continue;
        void *entry = blobmsg_open_table(&b, NULL);
        char state_path[256];
        struct json_object *state = NULL, *state_value = NULL;
        blobmsg_add_string(&b, "modem_id", section->e.name);
        blobmsg_add_string(&b, "name", option_string(uci, section, "name", section->e.name));
        blobmsg_add_string(&b, "mode", option_string(uci, section, "sms_mode", "database_poll"));
        blobmsg_add_u8(&b, "use_ubus", !strcmp(option_string(uci, section, "use_ubus", "0"), "1"));
        blobmsg_add_u8(&b, "enabled", strcmp(option_string(uci, section, "enabled", "1"), "0") != 0);
        snprintf(state_path, sizeof(state_path), "/var/run/qmodem/settings/%s.json", section->e.name);
        state = json_object_from_file(state_path);
        if (state && json_object_object_get_ex(state, "state", &state_value))
            blobmsg_add_string(&b, "settings_state", json_object_get_string(state_value));
        else
            blobmsg_add_string(&b, "settings_state", "unknown");
        if (state)
            json_object_put(state);
        blobmsg_close_table(&b, entry);
    }
    blobmsg_close_array(&b, array);
    ubus_send_reply(ctx, req, b.head);
    blob_buf_free(&b);
    uci_unload(uci, package); uci_free_context(uci);
    return UBUS_STATUS_OK;
}

static int modem_delete_method(struct ubus_context *ctx, struct ubus_object *obj,
                               struct ubus_request_data *req, const char *method,
                               struct blob_attr *msg)
{
    struct blob_attr *tb[__ARG_MAX];
    struct blob_buf b = {};
    int deleted;
    (void)obj; (void)method;
    blobmsg_parse(policy, __ARG_MAX, tb, blob_data(msg), blob_len(msg));
    if (!tb[ARG_MODEM]) return UBUS_STATUS_INVALID_ARGUMENT;
    if (sms_db_delete_modem_messages(&app.db, blobmsg_get_string(tb[ARG_MODEM]),
                                     &deleted) != 0) goto error;
    blob_buf_init(&b, 0); blobmsg_add_string(&b, "status", "success");
    blobmsg_add_u32(&b, "deleted", deleted);
    ubus_send_reply(ctx, req, b.head); blob_buf_free(&b); return UBUS_STATUS_OK;
error:
    reply_error(ctx, req, "modem database delete failed"); return UBUS_STATUS_OK;
}

static int delivery_claim_method(struct ubus_context *ctx, struct ubus_object *obj,
                                 struct ubus_request_data *req, const char *method,
                                 struct blob_attr *msg)
{
    sqlite3_stmt *stmt = NULL, *update = NULL;
    struct blob_buf b = {};
    int64_t id = 0;
    (void)obj; (void)method; (void)msg;
    if (sqlite3_exec(app.db.sql, "BEGIN IMMEDIATE", NULL, NULL, NULL) != SQLITE_OK) goto error;
    if (sqlite3_exec(app.db.sql,
            "UPDATE deliveries SET state='pending',claimed_at=NULL,"
            "available_at=strftime('%s','now') WHERE state='claimed' "
            "AND claimed_at<strftime('%s','now')-300", NULL, NULL, NULL) != SQLITE_OK)
        goto rollback;
    if (sqlite3_prepare_v2(app.db.sql,
            "SELECT d.id,m.id,m.modem_id,m.sender,m.timestamp,m.content,m.complete,m.revision "
            "FROM deliveries d JOIN messages m ON m.id=d.message_id "
            "WHERE d.state='pending' AND d.available_at<=strftime('%s','now') ORDER BY d.id LIMIT 1",
            -1, &stmt, NULL) != SQLITE_OK) goto rollback;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        id = sqlite3_column_int64(stmt, 0);
        blob_buf_init(&b, 0);
        blobmsg_add_string(&b, "status", "claimed");
        blobmsg_add_u64(&b, "delivery_id", id);
        blobmsg_add_u64(&b, "message_id", sqlite3_column_int64(stmt, 1));
        blobmsg_add_string(&b, "modem_id", (const char *)sqlite3_column_text(stmt, 2));
        blobmsg_add_string(&b, "sender", (const char *)sqlite3_column_text(stmt, 3));
        blobmsg_add_u64(&b, "timestamp", sqlite3_column_int64(stmt, 4));
        blobmsg_add_string(&b, "content", (const char *)sqlite3_column_text(stmt, 5));
        blobmsg_add_u8(&b, "complete", sqlite3_column_int(stmt, 6));
        blobmsg_add_u32(&b, "revision", sqlite3_column_int(stmt, 7));
    } else {
        blob_buf_init(&b, 0); blobmsg_add_string(&b, "status", "empty");
    }
    sqlite3_finalize(stmt); stmt = NULL;
    if (id) {
        if (sqlite3_prepare_v2(app.db.sql,
                "UPDATE deliveries SET state='claimed',attempts=attempts+1,claimed_at=strftime('%s','now') WHERE id=?",
                -1, &update, NULL) != SQLITE_OK) goto rollback;
        sqlite3_bind_int64(update, 1, id);
        if (sqlite3_step(update) != SQLITE_DONE) goto rollback;
        sqlite3_finalize(update); update = NULL;
    }
    if (sqlite3_exec(app.db.sql, "COMMIT", NULL, NULL, NULL) != SQLITE_OK) goto error;
    ubus_send_reply(ctx, req, b.head); blob_buf_free(&b); return UBUS_STATUS_OK;
rollback:
    sqlite3_finalize(stmt); sqlite3_finalize(update);
    sqlite3_exec(app.db.sql, "ROLLBACK", NULL, NULL, NULL);
error:
    if (b.head) blob_buf_free(&b);
    reply_error(ctx, req, "delivery claim failed"); return UBUS_STATUS_OK;
}

static int delivery_complete_method(struct ubus_context *ctx, struct ubus_object *obj,
                                    struct ubus_request_data *req, const char *method,
                                    struct blob_attr *msg)
{
    struct blob_attr *tb[__ARG_MAX]; sqlite3_stmt *stmt = NULL; struct blob_buf b = {};
    const char *state, *error = NULL;
    (void)obj; (void)method;
    blobmsg_parse(policy, __ARG_MAX, tb, blob_data(msg), blob_len(msg));
    if (!tb[ARG_DELIVERY_ID] || !tb[ARG_SUCCESS]) return UBUS_STATUS_INVALID_ARGUMENT;
    state = blobmsg_get_bool(tb[ARG_SUCCESS]) ? "done" : "pending";
    if (tb[ARG_ERROR]) error = blobmsg_get_string(tb[ARG_ERROR]);
    if (sqlite3_prepare_v2(app.db.sql,
            "UPDATE deliveries SET state=?,completed_at=CASE WHEN ?='done' THEN strftime('%s','now') END,"
            "available_at=CASE WHEN ?='pending' THEN strftime('%s','now')+MIN(3600,30*(1<<MIN(attempts,7))) ELSE available_at END,last_error=? "
            "WHERE id=? AND state='claimed'", -1, &stmt, NULL) != SQLITE_OK) goto error;
    sqlite3_bind_text(stmt, 1, state, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, state, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, state, -1, SQLITE_TRANSIENT);
    if (error) sqlite3_bind_text(stmt, 4, error, -1, SQLITE_TRANSIENT); else sqlite3_bind_null(stmt, 4);
    sqlite3_bind_int64(stmt, 5, blobmsg_get_u64(tb[ARG_DELIVERY_ID]));
    if (sqlite3_step(stmt) != SQLITE_DONE) goto error;
    sqlite3_finalize(stmt); blob_buf_init(&b, 0); blobmsg_add_string(&b, "status", "success");
    ubus_send_reply(ctx, req, b.head); blob_buf_free(&b); return UBUS_STATUS_OK;
error:
    sqlite3_finalize(stmt); reply_error(ctx, req, "delivery completion failed"); return UBUS_STATUS_OK;
}

static void urc_event(struct ubus_context *ctx, struct ubus_event_handler *handler,
                      const char *type, struct blob_attr *msg)
{
    enum { URC_PORT, URC_OWNER, URC_ID, __URC_MAX };
    static const struct blobmsg_policy urc_policy[] = {
        [URC_PORT] = { .name = "port", .type = BLOBMSG_TYPE_STRING },
        [URC_OWNER] = { .name = "owner", .type = BLOBMSG_TYPE_STRING },
        [URC_ID] = { .name = "urc_id", .type = BLOBMSG_TYPE_STRING },
    };
    struct blob_attr *tb[__URC_MAX];
    struct uci_context *uci; struct uci_package *package = NULL; struct uci_element *element;
    (void)ctx; (void)handler; (void)type;
    blobmsg_parse(urc_policy, __URC_MAX, tb, blob_data(msg), blob_len(msg));
    if (!tb[URC_PORT] || !tb[URC_OWNER] || !tb[URC_ID] ||
        strcmp(blobmsg_get_string(tb[URC_OWNER]), "qmodem.sms") ||
        strcmp(blobmsg_get_string(tb[URC_ID]), "sms-new")) return;
    uci = uci_alloc_context();
    if (!uci || uci_load(uci, "qmodem", &package) != UCI_OK) goto out;
    uci_foreach_element(&package->sections, element) {
        struct uci_section *section = uci_to_section(element);
        int imported, deleted;
        if (!strcmp(option_string(uci, section, "at_port", ""), blobmsg_get_string(tb[URC_PORT])) &&
            !strcmp(option_string(uci, section, "sms_mode", "database_poll"), "database_urc")) {
            sync_modem(section->e.name, "urc", &imported, &deleted);
            break;
        }
    }
out:
    if (package) uci_unload(uci, package);
    if (uci) uci_free_context(uci);
}

static void scheduler_cb(struct uloop_timeout *timeout)
{
    struct uci_context *uci = uci_alloc_context();
    struct uci_package *package = NULL;
    struct uci_element *element;
    int64_t now = time(NULL);
    (void)timeout;

    if (!uci || uci_load(uci, "qmodem", &package) != UCI_OK)
        goto out;
    uci_foreach_element(&package->sections, element) {
        struct uci_section *section = uci_to_section(element);
        struct modem_config cfg;
        sqlite3_stmt *stmt = NULL;
        int64_t last_sync = 0;
        int imported, deleted;

        if (strcmp(section->type, "modem-device") ||
            load_config(section->e.name, &cfg) != 0)
            continue;
        if (!strcmp(cfg.mode, "database_urc")) {
            continue;
        }
        if (strcmp(cfg.mode, "database_poll") || !cfg.forwarding || !cfg.use_ubus)
            continue;
        if (sqlite3_prepare_v2(app.db.sql,
                "SELECT COALESCE(last_sync_at,0) FROM modems WHERE id=?", -1,
                &stmt, NULL) == SQLITE_OK) {
            sqlite3_bind_text(stmt, 1, section->e.name, -1, SQLITE_TRANSIENT);
            if (sqlite3_step(stmt) == SQLITE_ROW)
                last_sync = sqlite3_column_int64(stmt, 0);
        }
        sqlite3_finalize(stmt);
        if (last_sync + cfg.poll_interval <= now)
            (void)sync_modem(section->e.name, "poll", &imported, &deleted);
    }
out:
    if (package)
        uci_unload(uci, package);
    if (uci)
        uci_free_context(uci);
    uloop_timeout_set(&app.scheduler, SCHEDULER_TICK_MS);
}

static const struct ubus_method methods[] = {
    UBUS_METHOD_NOARG("status", status_method),
    UBUS_METHOD("config_get", config_get_method, policy),
    UBUS_METHOD("configure", configure_method, policy),
    UBUS_METHOD("sync", sync_method, policy),
    UBUS_METHOD_NOARG("sync_status", sync_status_method),
    UBUS_METHOD("list", list_method, policy),
    UBUS_METHOD("get", get_method, policy),
    UBUS_METHOD("send", send_method, policy),
    UBUS_METHOD("delete", delete_method, policy),
    UBUS_METHOD("mark_read", mark_read_method, policy),
    UBUS_METHOD_NOARG("modem_list", modem_list_method),
    UBUS_METHOD("modem_delete", modem_delete_method, policy),
    UBUS_METHOD("storage_get", storage_get_method, policy),
    UBUS_METHOD("storage_set", storage_set_method, policy),
    UBUS_METHOD_NOARG("delivery_claim", delivery_claim_method),
    UBUS_METHOD("delivery_complete", delivery_complete_method, policy),
};

static struct ubus_object_type object_type = UBUS_OBJECT_TYPE("qmodem.sms", methods);

int main(int argc, char **argv)
{
    const char *db_path = argc > 1 ? argv[1] : DEFAULT_DB;
    memset(&app, 0, sizeof(app));
    snprintf(app.db_path, sizeof(app.db_path), "%s", db_path);
    umask(0077);
    uloop_init();
    if (sms_db_open(&app.db, db_path) != 0) {
        fprintf(stderr, "qmodem-smsd: database open failed: %s\n", sms_db_error(&app.db));
        return 1;
    }
    app.ubus = ubus_connect(NULL);
    if (!app.ubus) { sms_db_close(&app.db); return 1; }
    ubus_add_uloop(app.ubus);
    app.object.name = "qmodem.sms"; app.object.type = &object_type;
    app.object.methods = methods; app.object.n_methods = ARRAY_SIZE(methods);
    if (ubus_add_object(app.ubus, &app.object) != UBUS_STATUS_OK) {
        ubus_free(app.ubus); sms_db_close(&app.db); return 1;
    }
    app.urc_handler.cb = urc_event;
    ubus_register_event_handler(app.ubus, &app.urc_handler, "qmodem.at.urc");
    app.scheduler.cb = scheduler_cb;
    uloop_timeout_set(&app.scheduler, 1000);
    uloop_run();
    ubus_free(app.ubus); sms_db_close(&app.db); uloop_done();
    return 0;
}
