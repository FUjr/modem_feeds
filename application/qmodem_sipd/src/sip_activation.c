#define _POSIX_C_SOURCE 200809L

#include "sip_activation.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

int qmodem_voip_sip_activate(const struct qmodem_voip_sip_activation_ops *ops,
				     void *opaque, int registrar_live)
{
	if (!ops || !ops->enable || !ops->reload || !ops->schedule_start ||
	    ops->enable(opaque) != 0)
		return -1;
	if (registrar_live)
		return ops->reload(opaque) == 0 ?
			QMODEM_VOIP_SIP_ACTIVATION_RELOADED : -1;
	return ops->schedule_start(opaque) == 0 ?
		QMODEM_VOIP_SIP_ACTIVATION_SCHEDULED : -1;
}

int qmodem_voip_sip_run_program(const char *path, char *const arguments[])
{
	int status;
	pid_t child;
	if (!path || !arguments)
		return -1;
	child = fork();
	if (child < 0)
		return -1;
	if (child == 0) {
		execv(path, arguments);
		_exit(127);
	}
	while (waitpid(child, &status, 0) < 0) {
		if (errno != EINTR)
			return -1;
	}
	return WIFEXITED(status) && WEXITSTATUS(status) == 0 ? 0 : -1;
}

int qmodem_voip_sip_process_start(pid_t process, unsigned long long *start)
{
	char path[64];
	char line[1024];
	char *cursor;
	char *end;
	unsigned field;
	FILE *file;
	if (process <= 1 || !start)
		return -1;
	(void)snprintf(path, sizeof(path), "/proc/%ld/stat", (long)process);
	file = fopen(path, "r");
	if (!file)
		return -1;
	if (!fgets(line, sizeof(line), file)) {
		(void)fclose(file);
		return -1;
	}
	if (fclose(file) != 0)
		return -1;
	cursor = strrchr(line, ')');
	if (!cursor || cursor[1] != ' ')
		return -1;
	cursor += 2;
	for (field = 3; field <= 22; field++) {
		char *token = cursor;
		cursor = strchr(cursor, ' ');
		if (cursor)
			*cursor++ = '\0';
		if (field == 22) {
			errno = 0;
			*start = strtoull(token, &end, 10);
			return errno == 0 && end != token && *end == '\0' ? 0 : -1;
		}
		if (!cursor)
			return -1;
	}
	return -1;
}

int qmodem_voip_sip_pidfile_identity(const char *pidfile,
				     const char *expected_executable,
				     pid_t *process)
{
	char executable[4096];
	char line[128];
	char trailing;
	unsigned long long expected_start;
	unsigned long long actual_start;
	long parsed;
	ssize_t length;
	FILE *file;
	if (!pidfile || !expected_executable || !process)
		return -1;
	file = fopen(pidfile, "r");
	if (!file)
		return -1;
	if (!fgets(line, sizeof(line), file)) {
		(void)fclose(file);
		return -1;
	}
	if (fclose(file) != 0 ||
	    sscanf(line, "%ld %llu %c", &parsed, &expected_start, &trailing) != 2 ||
	    parsed <= 1)
		return -1;
	(void)snprintf(line, sizeof(line), "/proc/%ld/exe", parsed);
	length = readlink(line, executable, sizeof(executable) - 1);
	if (length <= 0)
		return -1;
	executable[length] = '\0';
	if (strcmp(executable, expected_executable) != 0 ||
	    qmodem_voip_sip_process_start((pid_t)parsed, &actual_start) != 0 ||
	    actual_start != expected_start)
		return -1;
	*process = (pid_t)parsed;
	return 0;
}
