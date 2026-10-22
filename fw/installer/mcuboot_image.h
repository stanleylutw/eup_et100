#ifndef ET100_MCUBOOT_IMAGE_H
#define ET100_MCUBOOT_IMAGE_H
#include <stddef.h>
#include <stdint.h>

#define ET100_APP_SLOT 0x08020000u
#define ET100_APP_SLOT_SIZE 0x1c0000u
#define ET100_APP_VECTOR 0x08020200u
enum image_result { IMAGE_OK, IMAGE_EFORMAT=-1, IMAGE_EHASH=-2, IMAGE_EVECTOR=-3,
                    IMAGE_ETARGET=-4, IMAGE_EMETADATA=-5, IMAGE_EIO=-6 };
struct image_info { uint32_t size, payload_size, msp, reset; uint8_t sha256[32]; };
/* Prototype only: unsigned, no encryption/compression/protected TLVs. */
int mcuboot_image_verify(const uint8_t *image, size_t available, struct image_info *info);
typedef int (*image_read_fn)(void *context, uint32_t offset, void *data, size_t length);
int image_hash(image_read_fn read, void *context, uint32_t length, uint8_t digest[32]);
/* Candidate requires unique, compatible metadata. Internal E0 images predate metadata. */
int image_candidate_verify(image_read_fn read, void *context, uint32_t length,
                           const uint8_t expected[32], struct image_info *info);
#endif
