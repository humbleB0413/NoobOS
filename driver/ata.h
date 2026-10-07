#ifndef ATA_H
#define ATA_H

#include <stdint.h>

/* Primary ATA 버스 (레거시 ISA 포트) */
#define ATA_PRIMARY_IO      0x1F0
#define ATA_PRIMARY_CTRL    0x3F6

/* I/O 포트 오프셋 */
#define ATA_REG_DATA        0
#define ATA_REG_ERROR       1
#define ATA_REG_SECCOUNT    2
#define ATA_REG_LBA_LO      3
#define ATA_REG_LBA_MID     4
#define ATA_REG_LBA_HI      5
#define ATA_REG_DRIVE       6
#define ATA_REG_STATUS      7   /* 읽기 */
#define ATA_REG_COMMAND     7   /* 쓰기 */

#define ATA_SR_ERR  0x01
#define ATA_SR_DRQ  0x08
#define ATA_SR_DF   0x20
#define ATA_SR_BSY  0x80

#define ATA_CMD_READ_PIO   0x20
#define ATA_CMD_WRITE_PIO  0x30
#define ATA_CMD_CACHE_FLUSH 0xE7
#define ATA_CMD_IDENTIFY   0xEC

#define ATA_SECTOR_SIZE 512

typedef struct {
  int present;
  uint32_t sectors;     /* LBA28 주소 가능한 섹터 수 */
  char model[41];
} ata_drive_t;

/* primary master 를 IDENTIFY 로 찾는다. 없으면 0 */
int init_ata(void);
const ata_drive_t *ata_drive(void);
/* LBA28 PIO. 성공 0, 실패 -1 */
int ata_read(uint32_t lba, uint8_t count, void *buf);
int ata_write(uint32_t lba, uint8_t count, const void *buf);

#endif
