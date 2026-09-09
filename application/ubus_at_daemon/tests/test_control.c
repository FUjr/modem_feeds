#include "ubus_at_daemon.h"

#include <assert.h>

at_daemon_ctx_t g_daemon_ctx;

int main(void)
{
    char token[96];
    uint64_t expires = 0;

    g_daemon_ctx.daemon_epoch = 1234;
    assert(pthread_mutex_init(&g_daemon_ctx.control_mutex, NULL) == 0);
    assert(at_lease_acquire("/dev/ttyTEST0", "qmodem.settings", 60,
                            token, sizeof(token), &expires) == 0);
    assert(expires > at_monotonic_ms());
    assert(at_lease_authorize("/dev/ttyTEST0", NULL) == -1);
    assert(at_lease_authorize("/dev/ttyTEST0", "qmodem.sms") == -1);
    assert(at_lease_authorize("/dev/ttyTEST0", "qmodem.settings") == 0);
    assert(at_lease_acquire("/dev/ttyTEST0", "other", 60,
                            token, sizeof(token), &expires) == -2);
    assert(at_lease_renew("/dev/ttyTEST0", "qmodem.settings", token, 60,
                          &expires) == 0);

    assert(at_urc_register("/dev/ttyTEST0", "qmodem.sms", "sms-new",
                           "+CMTI:") == 0);
    assert(at_urc_register("/dev/ttyTEST0", "qmodem.sms", "sms-new",
                           "+CMTI: ") == 0);
    assert(g_daemon_ctx.urcs != NULL && g_daemon_ctx.urcs->next == NULL);
    assert(!strcmp(g_daemon_ctx.urcs->prefix, "+CMTI: "));
    assert(at_urc_unregister("/dev/ttyTEST0", "other", "sms-new") == -1);
    assert(at_urc_unregister("/dev/ttyTEST0", "qmodem.sms", "sms-new") == 0);
    assert(at_urc_register("/dev/ttyTEST0", "qmodem.sms", "sms-new",
                           "+CMTI:") == 0);
    assert(at_urc_register("/dev/ttyTEST1", "qmodem.sms", "sms-new",
                           "+CMTI: ") == 0);
    assert(g_daemon_ctx.urcs != NULL && g_daemon_ctx.urcs->next == NULL);
    assert(!strcmp(g_daemon_ctx.urcs->port, "/dev/ttyTEST1"));
    assert(at_urc_unregister("/dev/ttyTEST1", "qmodem.sms", "sms-new") == 0);
    assert(at_lease_release("/dev/ttyTEST0", "qmodem.settings", token) == 0);
    assert(at_lease_authorize("/dev/ttyTEST0", NULL) == 0);

    at_control_cleanup();
    pthread_mutex_destroy(&g_daemon_ctx.control_mutex);
    puts("PASS exclusive lease authorization and idempotent URC registration");
    return 0;
}
