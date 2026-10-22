#ifndef _FAL_CFG_H_
#define _FAL_CFG_H_

#define FAL_DEBUG 1
#define FAL_PART_HAS_TABLE_CFG
#define FAL_USING_SFUD_PORT
#define NOR_FLASH_DEV_NAME "norflash0"
extern const struct fal_flash_dev onchip_flash;
#define FAL_FLASH_DEV_TABLE { &onchip_flash }
#define FAL_PART_TABLE \
{ {FAL_PART_MAGIC_WORD, "FlashEnv", "flashdb_onchip", 0x1e0000, 112*1024, 0} }
#endif
