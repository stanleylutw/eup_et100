/* Interop harness: decode SMP serial frames from stdin and re-encode each decoded
 * packet to stdout with this firmware's encoder. Used by test_smp_interop.py. */
#include "smp_serial.h"
#include <stdio.h>

static void out(const char *data, size_t length, void *context)
{
    (void)context;
    fwrite(data, 1, length, stdout);
}

int main(void)
{
    static struct smp_serial s;
    smp_serial_init(&s);
    int c;
    while ((c = getchar()) != EOF) {
        const uint8_t *packet;
        size_t length;
        if (smp_serial_feed(&s, (uint8_t)c, &packet, &length) == 2)
            smp_serial_encode(packet, length, out, NULL);
    }
    fprintf(stderr, "frames=%u crc_errors=%u overflows=%u\n",
            (unsigned)s.frames, (unsigned)s.crc_errors, (unsigned)s.overflows);
    return 0;
}
