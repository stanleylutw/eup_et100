#ifndef ET100_SMP_SERIAL_H
#define ET100_SMP_SERIAL_H
/* MCUmgr/SMP serial transport framing (Zephyr mcumgr_serial, smpclient compatible):
 * line = start(06 09) or continue(04 14) + base64 + '\n'. The base64 stream decodes
 * to: be16 length(packet+2) | packet | be16 CRC16/XMODEM(packet). Each line holds a
 * whole number of base64 quads, so lines decode independently. */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define SMP_SERIAL_MAX_PACKET 1024u   /* decoded SMP packet (header + CBOR) */
#define SMP_SERIAL_MAX_LINE 256u      /* base64 chars per line accepted */
#define SMP_SERIAL_TX_LINE 124u       /* base64 chars per line emitted (127-byte frame) */

enum smp_serial_state { SMP_IDLE, SMP_LEAD, SMP_LINE };
struct smp_serial {
    enum smp_serial_state state;
    uint8_t lead;
    bool in_packet, line_bad;
    size_t line_len, raw_len;
    char line[SMP_SERIAL_MAX_LINE];
    uint8_t raw[SMP_SERIAL_MAX_PACKET + 4];
    uint32_t frames, crc_errors, overflows, timeouts;
    uint32_t now_ms, last_ms;
};
void smp_serial_init(struct smp_serial *s);
/* Feed one byte. Returns:
 *  0 = byte not part of an SMP frame (caller may give it to another parser),
 *  1 = consumed, no packet yet, 2 = consumed and a complete packet is ready
 *  (pointer and length returned through packet/length, valid until the next feed). */
int smp_serial_feed(struct smp_serial *s, uint8_t byte, const uint8_t **packet, size_t *length);
/* Call with the current time before feeding bytes (and periodically). A line
 * idle for SMP_SERIAL_LINE_TIMEOUT_MS returns the decoder to IDLE so ASCII
 * commands are not swallowed by a dead client's half line; an unfinished
 * multi-line packet is dropped after SMP_SERIAL_PACKET_TIMEOUT_MS. */
#define SMP_SERIAL_LINE_TIMEOUT_MS 200u
#define SMP_SERIAL_PACKET_TIMEOUT_MS 2000u
void smp_serial_expire(struct smp_serial *s, uint32_t now_ms);
/* True while inside an SMP line (caller should not route bytes elsewhere). */
bool smp_serial_busy(const struct smp_serial *s);

typedef void (*smp_serial_sink)(const char *data, size_t length, void *context);
/* Encode one packet into framed lines and pass each line to sink. Returns 0 or -1. */
int smp_serial_encode(const uint8_t *packet, size_t length, smp_serial_sink sink, void *context);
uint16_t smp_crc16_xmodem(const uint8_t *data, size_t length);
#endif
