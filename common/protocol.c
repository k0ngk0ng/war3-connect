/*
 * protocol.c – Length-prefixed JSON framing implementation.
 *
 * Wire format:  [4-byte big-endian payload length][JSON payload]
 */

#include "protocol.h"

#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#ifdef _WIN32
#   include <winsock2.h>          /* htonl / ntohl */
#else
#   include <arpa/inet.h>         /* htonl / ntohl */
#endif

/* ------------------------------------------------------------------ */
/*  Protocol_Frame                                                    */
/* ------------------------------------------------------------------ */

uint8_t *Protocol_Frame(const char *json_str, uint32_t *out_len)
{
    if (json_str == NULL || out_len == NULL) {
        return NULL;
    }

    uint32_t payload_len = (uint32_t)strlen(json_str);
    uint32_t total_len   = FRAME_HEADER_SIZE + payload_len;

    uint8_t *buf = (uint8_t *)malloc(total_len);
    if (buf == NULL) {
        return NULL;
    }

    /* Write 4-byte big-endian length header. */
    uint32_t net_len = htonl(payload_len);
    memcpy(buf, &net_len, FRAME_HEADER_SIZE);

    /* Copy the JSON payload (no NUL terminator on the wire). */
    memcpy(buf + FRAME_HEADER_SIZE, json_str, payload_len);

    *out_len = total_len;
    return buf;
}

/* ------------------------------------------------------------------ */
/*  Protocol_Extract                                                  */
/* ------------------------------------------------------------------ */

uint32_t Protocol_Extract(const uint8_t *buf, uint32_t buf_len, char **out_json)
{
    if (buf == NULL || out_json == NULL) {
        return 0;
    }

    /* Need at least the 4-byte header. */
    if (buf_len < FRAME_HEADER_SIZE) {
        return 0;
    }

    /* Read the big-endian payload length. */
    uint32_t net_len;
    memcpy(&net_len, buf, FRAME_HEADER_SIZE);
    uint32_t payload_len = ntohl(net_len);

    /* Sanity check – reject absurdly large messages. */
    if (payload_len > MAX_MSG_SIZE) {
        return 0;
    }

    /* Check whether the full frame has arrived. */
    uint32_t frame_len = FRAME_HEADER_SIZE + payload_len;
    if (buf_len < frame_len) {
        return 0;
    }

    /* Allocate a NUL-terminated copy of the JSON payload. */
    char *json = (char *)malloc(payload_len + 1);
    if (json == NULL) {
        return 0;
    }

    memcpy(json, buf + FRAME_HEADER_SIZE, payload_len);
    json[payload_len] = '\0';

    *out_json = json;
    return frame_len;
}
