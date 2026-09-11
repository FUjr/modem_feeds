#ifndef QMODEM_VOIP_MEDIA_SOCKET_CLIENT_H
#define QMODEM_VOIP_MEDIA_SOCKET_CLIENT_H

#include "media_socket.h"

#include <libubox/uloop.h>
#include <stdint.h>

/*
 * Reusable consumer-side client for the daemon's unix SOCK_SEQPACKET media
 * socket. A consumer (SIP bridge, web gateway, diagnostic tool) owns one of
 * these, connects to the daemon path, attaches with the current call revision
 * and an authorized session id, and then receives modem PCM_CAPTURE frames
 * while submitting PCM_PLAYBACK frames. Keep it allocation-free so it can be
 * embedded in any consumer loop.
 */

struct qmodem_voip_media_socket_client {
	int fd;
	int attached;
	uint64_t call_revision;
	char session_id[QMODEM_VOIP_MEDIA_SOCKET_SESSION_ID_MAX];
	uint32_t outbound_sequence;
	uint32_t dropped;
};

/* Returns 0 on success; on failure leaves fd closed. */
int qmodem_voip_media_socket_client_attach(
	struct qmodem_voip_media_socket_client *client,
	const char *path, uint64_t call_revision, const char *session_id);

/* Non-blocking. Returns 1 frame read, 0 nothing ready, -1 error/closed. */
int qmodem_voip_media_socket_client_read(
	struct qmodem_voip_media_socket_client *client,
	struct qmodem_voip_media_socket_message *message);

/* Returns 0 on delivery, -1 on error (peer detached or header invalid). */
int qmodem_voip_media_socket_client_write(
	struct qmodem_voip_media_socket_client *client,
	enum qmodem_voip_media_socket_frame_type type,
	const void *payload, size_t payload_size);

void qmodem_voip_media_socket_client_detach(
	struct qmodem_voip_media_socket_client *client);

#endif
