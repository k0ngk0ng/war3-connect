/*
 * protocol.h – Length-prefixed JSON framing for War3 Connect.
 *
 * Wire format:  [4-byte big-endian payload length][JSON payload]
 *
 * The payload is a UTF-8 JSON string (no NUL terminator on the wire).
 */

#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>

#define FRAME_HEADER_SIZE   4
#define MAX_MSG_SIZE        65536
#define DEFAULT_SERVER_PORT 12000

/*
 * Frame a JSON string into [4-byte len][payload].
 *
 * Returns a malloc'd buffer containing the complete frame and sets
 * *out_len to the total number of bytes (header + payload).
 * The caller is responsible for freeing the returned buffer.
 * Returns NULL on allocation failure.
 */
uint8_t *Protocol_Frame(const char *json_str, uint32_t *out_len);

/*
 * Try to extract one complete frame from a receive buffer.
 *
 *   buf      – pointer to the receive buffer
 *   buf_len  – number of valid bytes currently in the buffer
 *   out_json – on success, set to a malloc'd NUL-terminated JSON string
 *              (caller frees); unchanged on failure
 *
 * Returns the number of bytes consumed from buf (header + payload).
 * Returns 0 if the buffer does not yet contain a complete frame.
 */
uint32_t Protocol_Extract(const uint8_t *buf, uint32_t buf_len, char **out_json);

#endif /* PROTOCOL_H */
