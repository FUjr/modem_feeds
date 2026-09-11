#ifndef QMODEM_VOIP_SIP_GATEWAY_H
#define QMODEM_VOIP_SIP_GATEWAY_H

#include <stddef.h>
#include <stdint.h>

#define QMODEM_VOIP_SIP_REALM "qmodem-voip"
#define QMODEM_VOIP_SIP_USERNAME_SIZE 64
#define QMODEM_VOIP_SIP_HA1_SIZE 33
#ifndef QMODEM_VOIP_SIP_CONFIG
#define QMODEM_VOIP_SIP_CONFIG "/var/run/qmodem_voip/sip.conf"
#endif
#define QMODEM_VOIP_SIP_NONCE_SIZE 33
#define QMODEM_VOIP_SIP_NONCE_SLOTS 32
#define QMODEM_VOIP_SIP_CALL_ID_SIZE 128
#define QMODEM_VOIP_SIP_TAG_SIZE 96
#define QMODEM_VOIP_SIP_BRANCH_SIZE 128
#define QMODEM_VOIP_SIP_MEDIA_ADDRESS_SIZE 16

struct qmodem_voip_sip_credentials {
	char username[QMODEM_VOIP_SIP_USERNAME_SIZE];
	char ha1[QMODEM_VOIP_SIP_HA1_SIZE];
};

struct qmodem_voip_sip_rate {
	uint32_t address;
	uint64_t window_start;
	unsigned count;
};

struct qmodem_voip_sip_nonce {
	uint32_t address;
	uint64_t issued;
	unsigned long last_nc;
	char value[QMODEM_VOIP_SIP_NONCE_SIZE];
};

struct qmodem_voip_sip_nonce_store {
	struct qmodem_voip_sip_nonce slots[QMODEM_VOIP_SIP_NONCE_SLOTS];
};

enum qmodem_voip_sip_nonce_result {
	QMODEM_VOIP_SIP_NONCE_OK,
	QMODEM_VOIP_SIP_NONCE_UNKNOWN,
	QMODEM_VOIP_SIP_NONCE_EXPIRED,
	QMODEM_VOIP_SIP_NONCE_REPLAYED
};

enum qmodem_voip_sip_call_origin {
	QMODEM_VOIP_SIP_CALL_LAN,
	QMODEM_VOIP_SIP_CALL_CELLULAR
};

struct qmodem_voip_sip_call {
	int active;
	int established;
	enum qmodem_voip_sip_call_origin origin;
	unsigned cseq;
	char call_id[QMODEM_VOIP_SIP_CALL_ID_SIZE];
	char remote_tag[QMODEM_VOIP_SIP_TAG_SIZE];
	char branch[QMODEM_VOIP_SIP_BRANCH_SIZE];
};

struct qmodem_voip_sip_media {
	char address[QMODEM_VOIP_SIP_MEDIA_ADDRESS_SIZE];
	unsigned port;
	unsigned payload_type;
};

typedef int (*qmodem_voip_sip_action_fn)(void *opaque);

int qmodem_voip_sip_validate_credentials(const char *username,
					 const char *password);
int qmodem_voip_sip_write_credentials(const char *path, const char *username,
					  const char *password);
int qmodem_voip_sip_read_credentials(const char *path,
					 struct qmodem_voip_sip_credentials *credentials);
int qmodem_voip_sip_valid_lan_address(const char *address);
int qmodem_voip_sip_rate_allow(struct qmodem_voip_sip_rate *rates,
				       size_t rate_count, uint32_t address,
				       uint64_t now, unsigned limit);
int qmodem_voip_sip_validate_sdp(const char *body, size_t length,
				  unsigned *rtp_port);
int qmodem_voip_sip_parse_media(const char *body, size_t length,
				struct qmodem_voip_sip_media *media);
int qmodem_voip_sip_invite_body_status(const char *body, size_t length,
					unsigned *rtp_port);
int qmodem_voip_sip_nonce_issue(struct qmodem_voip_sip_nonce_store *store,
				uint32_t address, uint64_t now,
				char output[QMODEM_VOIP_SIP_NONCE_SIZE]);
enum qmodem_voip_sip_nonce_result qmodem_voip_sip_nonce_check(
	struct qmodem_voip_sip_nonce_store *store, uint32_t address,
	const char *nonce, uint64_t now, unsigned long nc,
	struct qmodem_voip_sip_nonce **accepted);
void qmodem_voip_sip_nonce_commit(struct qmodem_voip_sip_nonce *nonce,
					  unsigned long nc);
int qmodem_voip_sip_call_begin(struct qmodem_voip_sip_call *call,
			       enum qmodem_voip_sip_call_origin origin,
			       const char *call_id, const char *remote_tag,
			       const char *branch, unsigned cseq);
int qmodem_voip_sip_call_cancel(struct qmodem_voip_sip_call *call,
				int authenticated, const char *call_id,
				const char *remote_tag, const char *branch,
				unsigned cseq,
				qmodem_voip_sip_action_fn action, void *opaque);
int qmodem_voip_sip_call_bye(struct qmodem_voip_sip_call *call,
			     int authenticated, const char *call_id,
			     const char *remote_tag,
			     qmodem_voip_sip_action_fn action, void *opaque);
int qmodem_voip_sip_call_establish(struct qmodem_voip_sip_call *call,
				   const char *remote_tag);
void qmodem_voip_sip_call_release(struct qmodem_voip_sip_call *call);
void qmodem_voip_sip_call_clear(struct qmodem_voip_sip_call *call);

#endif
