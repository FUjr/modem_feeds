#define _POSIX_C_SOURCE 200809L

#include "sip_gateway.h"

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/random.h>
#include <unistd.h>

static uint32_t md5_left_rotate(uint32_t value, unsigned count)
{
	return (value << count) | (value >> (32U - count));
}

static void md5(const unsigned char *input, size_t length, unsigned char output[16])
{
	static const uint32_t shift[] = {
		7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22,
		5, 9, 14, 20, 5, 9, 14, 20, 5, 9, 14, 20, 5, 9, 14, 20,
		4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23,
		6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21
	};
	static const uint32_t table[] = {
		0xd76aa478U,0xe8c7b756U,0x242070dbU,0xc1bdceeeU,0xf57c0fafU,0x4787c62aU,0xa8304613U,0xfd469501U,
		0x698098d8U,0x8b44f7afU,0xffff5bb1U,0x895cd7beU,0x6b901122U,0xfd987193U,0xa679438eU,0x49b40821U,
		0xf61e2562U,0xc040b340U,0x265e5a51U,0xe9b6c7aaU,0xd62f105dU,0x02441453U,0xd8a1e681U,0xe7d3fbc8U,
		0x21e1cde6U,0xc33707d6U,0xf4d50d87U,0x455a14edU,0xa9e3e905U,0xfcefa3f8U,0x676f02d9U,0x8d2a4c8aU,
		0xfffa3942U,0x8771f681U,0x6d9d6122U,0xfde5380cU,0xa4beea44U,0x4bdecfa9U,0xf6bb4b60U,0xbebfbc70U,
		0x289b7ec6U,0xeaa127faU,0xd4ef3085U,0x04881d05U,0xd9d4d039U,0xe6db99e5U,0x1fa27cf8U,0xc4ac5665U,
		0xf4292244U,0x432aff97U,0xab9423a7U,0xfc93a039U,0x655b59c3U,0x8f0ccc92U,0xffeff47dU,0x85845dd1U,
		0x6fa87e4fU,0xfe2ce6e0U,0xa3014314U,0x4e0811a1U,0xf7537e82U,0xbd3af235U,0x2ad7d2bbU,0xeb86d391U
	};
	uint32_t state[4] = { 0x67452301U, 0xefcdab89U, 0x98badcfeU, 0x10325476U };
	unsigned char block[64];
	uint64_t bit_length = (uint64_t)length * 8U;
	size_t offset;

	for (offset = 0; offset <= length; offset += 64) {
		uint32_t words[16];
		uint32_t a = state[0], b = state[1], c = state[2], d = state[3];
		size_t remaining = length - offset;
		unsigned i;
		if (remaining >= 64) {
			memcpy(block, input + offset, sizeof(block));
		} else {
			memset(block, 0, sizeof(block));
			if (remaining)
				memcpy(block, input + offset, remaining);
			block[remaining] = 0x80;
			if (remaining >= 56) {
				memset(words, 0, sizeof(words));
				for (i = 0; i < 64; i++) {
					words[i / 4] |= (uint32_t)block[i] << ((i % 4) * 8U);
				}
				for (i = 0; i < 64; i++) {
					uint32_t f; unsigned g;
					if (i < 16) { f = (b & c) | (~b & d); g = i; }
					else if (i < 32) { f = (d & b) | (~d & c); g = (5U * i + 1U) % 16U; }
					else if (i < 48) { f = b ^ c ^ d; g = (3U * i + 5U) % 16U; }
					else { f = c ^ (b | ~d); g = (7U * i) % 16U; }
					f += a + table[i] + words[g]; a = d; d = c; c = b; b += md5_left_rotate(f, shift[i]);
				}
				state[0] += a; state[1] += b; state[2] += c; state[3] += d;
				memset(block, 0, sizeof(block));
			}
			for (i = 0; i < 8; i++)
				block[56 + i] = (unsigned char)(bit_length >> (8U * i));
		}
		memset(words, 0, sizeof(words));
		for (i = 0; i < 64; i++)
			words[i / 4] |= (uint32_t)block[i] << ((i % 4) * 8U);
		for (i = 0; i < 64; i++) {
			uint32_t f; unsigned g;
			if (i < 16) { f = (b & c) | (~b & d); g = i; }
			else if (i < 32) { f = (d & b) | (~d & c); g = (5U * i + 1U) % 16U; }
			else if (i < 48) { f = b ^ c ^ d; g = (3U * i + 5U) % 16U; }
			else { f = c ^ (b | ~d); g = (7U * i) % 16U; }
			f += a + table[i] + words[g]; a = d; d = c; c = b; b += md5_left_rotate(f, shift[i]);
		}
		state[0] += a; state[1] += b; state[2] += c; state[3] += d;
		if (remaining < 56)
			break;
	}
	for (offset = 0; offset < 16; offset++)
		output[offset] = (unsigned char)(state[offset / 4] >> ((offset % 4) * 8U));
}

static int valid_username(const char *value)
{
	size_t i, length;
	if (!value || !(length = strlen(value)) || length >= QMODEM_VOIP_SIP_USERNAME_SIZE)
		return 0;
	for (i = 0; i < length; i++)
		if (!(isalnum((unsigned char)value[i]) || value[i] == '.' || value[i] == '_' || value[i] == '-'))
			return 0;
	return 1;
}

int qmodem_voip_sip_validate_credentials(const char *username, const char *password)
{
	size_t i, length;
	if (!valid_username(username) || !password || (length = strlen(password)) < 8 || length > 127)
		return -1;
	for (i = 0; i < length; i++)
		if ((unsigned char)password[i] < 0x21 || (unsigned char)password[i] > 0x7e)
			return -1;
	return 0;
}

static void make_ha1(const char *username, const char *password, char output[QMODEM_VOIP_SIP_HA1_SIZE])
{
	static const char hex[] = "0123456789abcdef";
	unsigned char digest[16];
	char source[QMODEM_VOIP_SIP_USERNAME_SIZE + 1 + sizeof(QMODEM_VOIP_SIP_REALM) + 1 + 128];
	unsigned i;
	(void)snprintf(source, sizeof(source), "%s:%s:%s", username, QMODEM_VOIP_SIP_REALM, password);
	md5((const unsigned char *)source, strlen(source), digest);
	for (i = 0; i < sizeof(digest); i++) {
		output[i * 2] = hex[digest[i] >> 4];
		output[i * 2 + 1] = hex[digest[i] & 0x0fU];
	}
	output[32] = '\0';
	memset(source, 0, sizeof(source));
	memset(digest, 0, sizeof(digest));
}

int qmodem_voip_sip_write_credentials(const char *path, const char *username, const char *password)
{
	char temporary[256];
	char ha1[QMODEM_VOIP_SIP_HA1_SIZE];
	int descriptor;
	int result = -1;
	if (!path || qmodem_voip_sip_validate_credentials(username, password) != 0 || strlen(path) + 12 >= sizeof(temporary))
		return -1;
	make_ha1(username, password, ha1);
	(void)snprintf(temporary, sizeof(temporary), "%s.XXXXXX", path);
	descriptor = mkstemp(temporary);
	if (descriptor < 0)
		goto out;
	if (fchmod(descriptor, S_IRUSR | S_IWUSR) != 0 ||
	    dprintf(descriptor, "username=%s\nha1=%s\n", username, ha1) < 0 ||
	    fsync(descriptor) != 0 || close(descriptor) != 0) {
		(void)unlink(temporary);
		goto out;
	}
	if (rename(temporary, path) == 0)
		result = 0;
	else
		(void)unlink(temporary);
out:
	memset(ha1, 0, sizeof(ha1));
	return result;
}

int qmodem_voip_sip_read_credentials(const char *path, struct qmodem_voip_sip_credentials *credentials)
{
	char line[128];
	FILE *file;
	struct stat status;
	int username = 0, ha1 = 0;
	if (!path || !credentials || stat(path, &status) != 0 || !S_ISREG(status.st_mode) ||
	    (status.st_mode & (S_IRWXG | S_IRWXO)) != 0)
		return -1;
#ifndef QMODEM_VOIP_HOST_TEST
	if (status.st_uid != 0)
		return -1;
#endif
	file = fopen(path, "r");
	if (!file)
		return -1;
	memset(credentials, 0, sizeof(*credentials));
	while (fgets(line, sizeof(line), file)) {
		char *value = strchr(line, '=');
		if (!value)
			goto out;
		*value++ = '\0';
		value[strcspn(value, "\r\n")] = '\0';
		if (strcmp(line, "username") == 0 && !username && valid_username(value)) {
			strcpy(credentials->username, value); username = 1;
		} else if (strcmp(line, "ha1") == 0 && !ha1 && strlen(value) == 32 &&
			strspn(value, "0123456789abcdef") == 32) {
			strcpy(credentials->ha1, value); ha1 = 1;
		} else {
			goto out;
		}
	}
	if (ferror(file) || !username || !ha1)
		goto out;
	fclose(file);
	return 0;
out:
	fclose(file);
	memset(credentials, 0, sizeof(*credentials));
	return -1;
}

int qmodem_voip_sip_valid_lan_address(const char *address)
{
	unsigned a, b, c, d;
	char trailing;
	return address && sscanf(address, "%u.%u.%u.%u%c", &a, &b, &c, &d, &trailing) == 4 &&
		a <= 255 && b <= 255 && c <= 255 && d <= 255 && (a || b || c || d);
}

int qmodem_voip_sip_rate_allow(struct qmodem_voip_sip_rate *rates, size_t rate_count,
				       uint32_t address, uint64_t now, unsigned limit)
{
	struct qmodem_voip_sip_rate *slot = NULL;
	size_t i;
	for (i = 0; i < rate_count; i++) {
		if (rates[i].address == address) { slot = &rates[i]; break; }
		if (!slot || rates[i].window_start < slot->window_start)
			slot = &rates[i];
	}
	if (!slot)
		return 0;
	if (slot->address != address || now - slot->window_start >= 60) {
		slot->address = address; slot->window_start = now; slot->count = 0;
	}
	if (slot->count >= limit)
		return 0;
	slot->count++;
	return 1;
}

int qmodem_voip_sip_validate_sdp(const char *body, size_t length, unsigned *rtp_port)
{
	char copy[2049];
	char *line;
	char *line_save = NULL;
	int pcma = 0, pcmu = 0;
	unsigned port = 0;
	if (!body || !length || length >= sizeof(copy))
		return -1;
	memcpy(copy, body, length);
	copy[length] = '\0';
	for (line = strtok_r(copy, "\r\n", &line_save); line;
	     line = strtok_r(NULL, "\r\n", &line_save)) {
		if (strncmp(line, "m=audio ", 8) == 0) {
			if (sscanf(line + 8, "%u", &port) != 1 || port == 0 || port > 65535)
				return -1;
			if (!strstr(line, " 0") && !strstr(line, " 8"))
				return -1;
			if (strstr(line, " 0"))
				pcmu = 1;
			if (strstr(line, " 8"))
				pcma = 1;
		} else if (strncmp(line, "a=rtpmap:8 PCMA/8000", 20) == 0) {
			pcma = 1;
		} else if (strncmp(line, "a=rtpmap:0 PCMU/8000", 20) == 0) {
			pcmu = 1;
		}
	}
	if (!port || (!pcma && !pcmu))
		return -1;
	if (rtp_port)
		*rtp_port = port;
	return 0;
}

int qmodem_voip_sip_parse_media(const char *body, size_t length,
				struct qmodem_voip_sip_media *media)
{
	char copy[2049];
	char *line;
	char *line_save = NULL;
	unsigned port = 0;
	int pcma = 0;
	int pcmu = 0;
	char address[QMODEM_VOIP_SIP_MEDIA_ADDRESS_SIZE] = { 0 };
	if (!body || !length || length >= sizeof(copy) || !media)
		return -1;
	memcpy(copy, body, length);
	copy[length] = '\0';
	for (line = strtok_r(copy, "\r\n", &line_save); line;
	     line = strtok_r(NULL, "\r\n", &line_save)) {
		if (strncmp(line, "c=IN IP4 ", 9) == 0) {
			if (!qmodem_voip_sip_valid_lan_address(line + 9) || address[0])
				return -1;
			(void)snprintf(address, sizeof(address), "%s", line + 9);
		} else if (strncmp(line, "m=audio ", 8) == 0) {
			char *formats = strstr(line + 8, " RTP/AVP ");
			char *token;
			char *format_save = NULL;
			if (!formats || port || sscanf(line + 8, "%u", &port) != 1 ||
			    !port || port > 65535)
				return -1;
			formats += 9;
			for (token = strtok_r(formats, " ", &format_save); token;
			     token = strtok_r(NULL, " ", &format_save)) {
				char *end;
				unsigned long payload = strtoul(token, &end, 10);
				if (*token && !*end && payload == 8)
					pcma = 1;
				else if (*token && !*end && payload == 0)
					pcmu = 1;
			}
		}
	}
	if (!address[0] || !port || (!pcma && !pcmu))
		return -1;
	memset(media, 0, sizeof(*media));
	(void)snprintf(media->address, sizeof(media->address), "%s", address);
	media->port = port;
	media->payload_type = pcma ? 8U : 0U;
	return 0;
}

int qmodem_voip_sip_invite_body_status(const char *body, size_t length,
					unsigned *rtp_port)
{
	return qmodem_voip_sip_validate_sdp(body, length, rtp_port) == 0 ? 0 : 488;
}

static int random_bytes(unsigned char *output, size_t length)
{
	size_t offset = 0;
	while (offset < length) {
		ssize_t result = getrandom(output + offset, length - offset, 0);
		if (result > 0) {
			offset += (size_t)result;
			continue;
		}
		if (result < 0 && errno == EINTR)
			continue;
		return -1;
	}
	return 0;
}

int qmodem_voip_sip_nonce_issue(struct qmodem_voip_sip_nonce_store *store,
				uint32_t address, uint64_t now,
				char output[QMODEM_VOIP_SIP_NONCE_SIZE])
{
	static const char hex[] = "0123456789abcdef";
	struct qmodem_voip_sip_nonce *slot = NULL;
	unsigned char random[16];
	size_t i;
	if (!store || !output || random_bytes(random, sizeof(random)) != 0)
		return -1;
	for (i = 0; i < QMODEM_VOIP_SIP_NONCE_SLOTS; i++) {
		if (store->slots[i].value[0] == '\0') {
			slot = &store->slots[i];
			break;
		}
		if (!slot || store->slots[i].issued < slot->issued)
			slot = &store->slots[i];
	}
	if (!slot)
		return -1;
	memset(slot, 0, sizeof(*slot));
	slot->address = address;
	slot->issued = now;
	for (i = 0; i < sizeof(random); i++) {
		slot->value[i * 2] = hex[random[i] >> 4];
		slot->value[i * 2 + 1] = hex[random[i] & 0x0fU];
	}
	slot->value[32] = '\0';
	memcpy(output, slot->value, sizeof(slot->value));
	memset(random, 0, sizeof(random));
	return 0;
}

enum qmodem_voip_sip_nonce_result qmodem_voip_sip_nonce_check(
	struct qmodem_voip_sip_nonce_store *store, uint32_t address,
	const char *nonce, uint64_t now, unsigned long nc,
	struct qmodem_voip_sip_nonce **accepted)
{
	size_t i;
	if (accepted)
		*accepted = NULL;
	if (!store || !nonce)
		return QMODEM_VOIP_SIP_NONCE_UNKNOWN;
	for (i = 0; i < QMODEM_VOIP_SIP_NONCE_SLOTS; i++) {
		struct qmodem_voip_sip_nonce *slot = &store->slots[i];
		if (slot->address != address || strcmp(slot->value, nonce) != 0)
			continue;
		if (now < slot->issued || now - slot->issued >= 60)
			return QMODEM_VOIP_SIP_NONCE_EXPIRED;
		if (nc == 0 || nc <= slot->last_nc)
			return QMODEM_VOIP_SIP_NONCE_REPLAYED;
		if (accepted)
			*accepted = slot;
		return QMODEM_VOIP_SIP_NONCE_OK;
	}
	return QMODEM_VOIP_SIP_NONCE_UNKNOWN;
}

void qmodem_voip_sip_nonce_commit(struct qmodem_voip_sip_nonce *nonce,
					  unsigned long nc)
{
	if (nonce && nc > nonce->last_nc)
		nonce->last_nc = nc;
}

int qmodem_voip_sip_call_begin(struct qmodem_voip_sip_call *call,
			       enum qmodem_voip_sip_call_origin origin,
			       const char *call_id, const char *remote_tag,
			       const char *branch, unsigned cseq)
{
	if (!call || !call_id || !remote_tag || !branch || !*call_id ||
	    !*remote_tag || !*branch ||
	    strlen(call_id) >= sizeof(call->call_id) ||
	    strlen(remote_tag) >= sizeof(call->remote_tag) ||
	    strlen(branch) >= sizeof(call->branch))
		return -1;
	memset(call, 0, sizeof(*call));
	call->active = 1;
	call->origin = origin;
	call->cseq = cseq;
	strcpy(call->call_id, call_id);
	strcpy(call->remote_tag, remote_tag);
	strcpy(call->branch, branch);
	return 0;
}

static int end_call(struct qmodem_voip_sip_call *call, int authenticated,
		    const char *call_id, const char *remote_tag, unsigned cseq,
		    int cancel, qmodem_voip_sip_action_fn action, void *opaque)
{
	if (!authenticated)
		return 401;
	if (!call || !call->active || !call_id || !remote_tag ||
	    strcmp(call->call_id, call_id) != 0 ||
	    strcmp(call->remote_tag, remote_tag) != 0 ||
	    /* A LAN caller can send CANCEL while it is still waiting for the
	     * delayed 200 OK.  The consumer marks the call established when that
	     * response is handed to PJSIP, so rejecting CANCEL solely because the
	     * local state is established leaves the cellular call stuck when the
	     * response was lost or not accepted by the client. */
	    (cancel && (call->origin != QMODEM_VOIP_SIP_CALL_LAN ||
		call->cseq != cseq)) ||
	    /* Linphone may send BYE when it has locally abandoned an INVITE
	     * whose delayed 200 OK was not accepted.  It is still a valid
	     * teardown request for the matched LAN call; requiring established
	     * here strands the cellular leg until the modem-side timeout. */
	    (!cancel && !call->established &&
		call->origin != QMODEM_VOIP_SIP_CALL_LAN))
		return 481;
	if (!action || action(opaque) != 0)
		return 503;
	qmodem_voip_sip_call_clear(call);
	return 200;
}

int qmodem_voip_sip_call_cancel(struct qmodem_voip_sip_call *call,
				int authenticated, const char *call_id,
				const char *remote_tag, const char *branch,
				unsigned cseq,
				qmodem_voip_sip_action_fn action, void *opaque)
{
	if (!call || !branch || strcmp(call->branch, branch) != 0)
		return authenticated ? 481 : 401;
	return end_call(call, authenticated, call_id, remote_tag, cseq, 1,
			action, opaque);
}

int qmodem_voip_sip_call_bye(struct qmodem_voip_sip_call *call,
			     int authenticated, const char *call_id,
			     const char *remote_tag,
			     qmodem_voip_sip_action_fn action, void *opaque)
{
	return end_call(call, authenticated, call_id, remote_tag, 0, 0,
			action, opaque);
}

int qmodem_voip_sip_call_establish(struct qmodem_voip_sip_call *call,
				   const char *remote_tag)
{
	if (!call || !call->active || !remote_tag || !*remote_tag ||
	    strlen(remote_tag) >= sizeof(call->remote_tag))
		return -1;
	strcpy(call->remote_tag, remote_tag);
	call->established = 1;
	return 0;
}

void qmodem_voip_sip_call_clear(struct qmodem_voip_sip_call *call)
{
	if (call)
		memset(call, 0, sizeof(*call));
}

void qmodem_voip_sip_call_release(struct qmodem_voip_sip_call *call)
{
	qmodem_voip_sip_call_clear(call);
}
