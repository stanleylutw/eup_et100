#ifndef ET100_LOADER_FRAME_H
#define ET100_LOADER_FRAME_H
#include <stdint.h>
#include <stddef.h>
#define LOADER_HEADER 20
#define LOADER_MAX 4096
enum loader_op { LOAD_BEGIN=1,LOAD_CHUNK=2,LOAD_FINISH=3,LOAD_ABORT=4 };
/* Wire: LDR1, op:u8, reserved:u8, size:u16, index:u32, payload_crc:u32,
 * header_crc:u32 (IEEE CRC over first 16 bytes); all integers little endian. */
int loader_header(const uint8_t header[20],uint8_t *op,uint32_t *index,size_t *length);
int loader_payload(const uint8_t header[20],const uint8_t *payload);
#endif
