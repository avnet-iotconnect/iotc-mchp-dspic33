/**
 * @file nvm_flash.h
 * @brief Minimal on-chip flash word-program/page-erase driver.
 *
 * This is hand-written directly from the "Flash Program Memory" chapter
 * (section 6, Run-Time Self-Programming / NVMCON) of the dsPIC33AK512MPS512
 * Family Data Sheet (DS70005591) - it is NOT MCC Melody output, unlike
 * everything under mcc_generated_files/. MCC Melody does offer a Flash/NVM
 * driver component for this device family; if you'd rather have a
 * Microchip-generated and -validated driver, add it via MCC Melody's Device
 * Resources and swap it in here instead.
 *
 * IMPORTANT - please validate on real hardware before relying on this:
 *  - The unlock-free NVMCON/NVMADR/NVMDATAx word-program and page-erase
 *    sequence matches the data sheet's documented steps exactly.
 *  - The reserved config page address (NVM_FLASH_CONFIG_PAGE_ADDR) is the
 *    last 4KB page of the 512K flash range (0x800000-0x87FFFC). This
 *    project does not use the dual-partition/bootloader (BOOTSWP) feature,
 *    so that page is not claimed by anything else, and device Configuration
 *    Words live in a separate address space (~0x7F3010), not in this
 *    range - but this address was derived from the data sheet, not
 *    confirmed against a linker map file. Read back what you write during
 *    bring-up to confirm there's no collision on your build.
 */
#ifndef NVM_FLASH_H
#define NVM_FLASH_H

#include <stdbool.h>
#include <stdint.h>

// Last 4KB page of the 512K flash range (0x800000 - 0x87FFFC). See the
// header comment above - verify on hardware before trusting this in a
// project that also uses the dual-boot/bootloader feature.
#define NVM_FLASH_CONFIG_PAGE_ADDR 0x87F000UL
#define NVM_FLASH_PAGE_SIZE        4096U
#define NVM_FLASH_WORD_SIZE        16U // 128 bits per Flash word (RTSP word program granularity)

/**
 * @brief Erases the 4KB page at NVM_FLASH_CONFIG_PAGE_ADDR.
 * @return true on success (NVMCON.WRERR == 0 after the erase completes).
 */
bool NVM_FLASH_ErasePage(void);

/**
 * @brief Word-programs (16 bytes at a time) a buffer into the config page.
 * @param offset  Byte offset from NVM_FLASH_CONFIG_PAGE_ADDR, must be a
 *                multiple of NVM_FLASH_WORD_SIZE (16).
 * @param data    Buffer to write.
 * @param len     Length in bytes, must be a multiple of NVM_FLASH_WORD_SIZE.
 * @return true on success.
 * @note The target page must already be erased (all 1s) - call
 *       NVM_FLASH_ErasePage() first when overwriting existing data.
 */
bool NVM_FLASH_WriteBytes(uint32_t offset, const uint8_t *data, uint16_t len);

/**
 * @brief Reads directly from flash (plain memory-mapped reads - no NVMCON
 *        sequencing needed for reads).
 */
void NVM_FLASH_ReadBytes(uint32_t offset, uint8_t *data, uint16_t len);

#endif // NVM_FLASH_H
