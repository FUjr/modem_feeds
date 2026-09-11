#ifndef QMODEM_VOIP_SIP_ACTIVATION_H
#define QMODEM_VOIP_SIP_ACTIVATION_H

#include <sys/types.h>

enum qmodem_voip_sip_activation_result {
	QMODEM_VOIP_SIP_ACTIVATION_RELOADED,
	QMODEM_VOIP_SIP_ACTIVATION_SCHEDULED
};

struct qmodem_voip_sip_activation_ops {
	int (*enable)(void *opaque);
	int (*reload)(void *opaque);
	int (*schedule_start)(void *opaque);
};

int qmodem_voip_sip_activate(const struct qmodem_voip_sip_activation_ops *ops,
				     void *opaque, int registrar_live);
int qmodem_voip_sip_run_program(const char *path, char *const arguments[]);
int qmodem_voip_sip_process_start(pid_t process, unsigned long long *start);
int qmodem_voip_sip_pidfile_identity(const char *pidfile,
				     const char *expected_executable,
				     pid_t *process);

#endif
