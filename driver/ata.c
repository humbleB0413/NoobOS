#include "ata.h"
#include "port_io.h"
#include "string.h"
#include "linux/vga.h"
#include "linux/spinlock.h"

/* 디스크 응답을 기다리는 동안 다른 프로세스가 같은 포트를 건드리지 않도록 — 짧은 PIO 라 인터럽트 차단으로 충분 */
static spinlock_t ata_lock = SPINLOCK_INIT("ata");
static ata_drive_t drive;

/* 드라이브 선택 후 상태 레지스터가 안정되기까지 ~400ns 필요: 대체 상태 포트를 4번 읽는 관용구 */
static void ata_delay(void) {
  for (int i = 0; i < 4; i++) inb(ATA_PRIMARY_CTRL);
}

static int ata_wait_not_busy(void) {
  for (uint32_t spin = 0; spin < 1000000; spin++) {
    if (!(inb(ATA_PRIMARY_IO + ATA_REG_STATUS) & ATA_SR_BSY)) return 0;
  }
  return -1;
}

/* BSY 가 풀리고 데이터 요청(DRQ)이 설 때까지. 오류 비트면 -1 */
static int ata_wait_drq(void) {
  for (uint32_t spin = 0; spin < 1000000; spin++) {
    uint8_t status = inb(ATA_PRIMARY_IO + ATA_REG_STATUS);
    if (status & (ATA_SR_ERR | ATA_SR_DF)) return -1;
    if (!(status & ATA_SR_BSY) && (status & ATA_SR_DRQ)) return 0;
  }
  return -1;
}

int init_ata(void) {
  memset(&drive, 0, sizeof(drive));
  /* 컨트롤러 인터럽트(nIEN)를 꺼서 폴링만 사용 */
  outb(ATA_PRIMARY_CTRL, 0x02);

  /* 플로팅 버스(장치 없음)는 0xFF 를 돌려준다 */
  if (inb(ATA_PRIMARY_IO + ATA_REG_STATUS) == 0xFF) return 0;

  outb(ATA_PRIMARY_IO + ATA_REG_DRIVE, 0xA0);  /* master */
  ata_delay();
  outb(ATA_PRIMARY_IO + ATA_REG_SECCOUNT, 0);
  outb(ATA_PRIMARY_IO + ATA_REG_LBA_LO, 0);
  outb(ATA_PRIMARY_IO + ATA_REG_LBA_MID, 0);
  outb(ATA_PRIMARY_IO + ATA_REG_LBA_HI, 0);
  outb(ATA_PRIMARY_IO + ATA_REG_COMMAND, ATA_CMD_IDENTIFY);

  if (inb(ATA_PRIMARY_IO + ATA_REG_STATUS) == 0) return 0;  /* 장치 없음 */
  if (ata_wait_not_busy() < 0) return 0;
  /* LBA mid/hi 가 0 이 아니면 ATA 가 아닌 장치(ATAPI CD-ROM 등) */
  if (inb(ATA_PRIMARY_IO + ATA_REG_LBA_MID) || inb(ATA_PRIMARY_IO + ATA_REG_LBA_HI)) return 0;
  if (ata_wait_drq() < 0) return 0;

  uint16_t id[256];
  for (int i = 0; i < 256; i++) id[i] = inw(ATA_PRIMARY_IO + ATA_REG_DATA);

  /* 모델명(word 27~46)은 워드마다 바이트 순서가 뒤집혀 있다 */
  for (int i = 0; i < 20; i++) {
    drive.model[i * 2] = (char)(id[27 + i] >> 8);
    drive.model[i * 2 + 1] = (char)(id[27 + i] & 0xFF);
  }
  for (int i = 39; i >= 0 && drive.model[i] == ' '; i--) drive.model[i] = '\0';
  drive.sectors = (uint32_t)id[60] | ((uint32_t)id[61] << 16);
  drive.present = 1;

  kprintf("ata: primary master \"%s\", %u sectors (%u MB)\n", drive.model, drive.sectors,
          drive.sectors / 2048);
  return 1;
}

const ata_drive_t *ata_drive(void) {
  return &drive;
}

static int ata_setup(uint32_t lba, uint8_t count, uint8_t command) {
  if (!drive.present || count == 0 || lba + count > drive.sectors) return -1;
  if (ata_wait_not_busy() < 0) return -1;
  outb(ATA_PRIMARY_IO + ATA_REG_DRIVE, 0xE0 | ((lba >> 24) & 0x0F));  /* master + LBA 모드 */
  ata_delay();
  outb(ATA_PRIMARY_IO + ATA_REG_SECCOUNT, count);
  outb(ATA_PRIMARY_IO + ATA_REG_LBA_LO, (uint8_t)lba);
  outb(ATA_PRIMARY_IO + ATA_REG_LBA_MID, (uint8_t)(lba >> 8));
  outb(ATA_PRIMARY_IO + ATA_REG_LBA_HI, (uint8_t)(lba >> 16));
  outb(ATA_PRIMARY_IO + ATA_REG_COMMAND, command);
  return 0;
}

int ata_read(uint32_t lba, uint8_t count, void *buf) {
  uint16_t *dst = buf;
  int result = 0;
  spin_lock(&ata_lock);
  if (ata_setup(lba, count, ATA_CMD_READ_PIO) < 0) {
    result = -1;
  } else {
    for (uint8_t s = 0; s < count && result == 0; s++) {
      if (ata_wait_drq() < 0) {
        result = -1;
        break;
      }
      for (int i = 0; i < ATA_SECTOR_SIZE / 2; i++) *dst++ = inw(ATA_PRIMARY_IO + ATA_REG_DATA);
      ata_delay();
    }
  }
  spin_unlock(&ata_lock);
  return result;
}

int ata_write(uint32_t lba, uint8_t count, const void *buf) {
  const uint16_t *src = buf;
  int result = 0;
  spin_lock(&ata_lock);
  if (ata_setup(lba, count, ATA_CMD_WRITE_PIO) < 0) {
    result = -1;
  } else {
    for (uint8_t s = 0; s < count && result == 0; s++) {
      if (ata_wait_drq() < 0) {
        result = -1;
        break;
      }
      for (int i = 0; i < ATA_SECTOR_SIZE / 2; i++) outw(ATA_PRIMARY_IO + ATA_REG_DATA, *src++);
      ata_delay();
    }
    if (result == 0) {
      outb(ATA_PRIMARY_IO + ATA_REG_COMMAND, ATA_CMD_CACHE_FLUSH);
      if (ata_wait_not_busy() < 0) result = -1;
    }
  }
  spin_unlock(&ata_lock);
  return result;
}
