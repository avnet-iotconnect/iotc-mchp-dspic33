#include <xc.h>
#include <string.h>
#include "nvm_flash.h"

// NVMCON.NVMOP values (dsPIC33AK512MPS512 Family Data Sheet DS70005591,
// section 6, "Flash Programming Operations").
#define NVMOP_WORD_PROGRAM 0x1U
#define NVMOP_PAGE_ERASE    0x3U

static bool NVM_FLASH_WaitAndCheck(void)
{
    while (NVMCONbits.WR == 1)
    {
        // The core stalls automatically during the actual program/erase
        // pulse, but poll WR too since we get here right after setting it.
    }
    bool ok = (NVMCONbits.WRERR == 0);
    NVMCONbits.WREN = 0;
    return ok;
}

bool NVM_FLASH_ErasePage(void)
{
    NVMADR = NVM_FLASH_CONFIG_PAGE_ADDR;
    NVMCONbits.WRERR = 0;
    NVMCONbits.WREN = 1;
    NVMCONbits.NVMOP = NVMOP_PAGE_ERASE;
    NVMCONbits.WR = 1;
    return NVM_FLASH_WaitAndCheck();
}

bool NVM_FLASH_WriteBytes(uint32_t offset, const uint8_t *data, uint16_t len)
{
    if ((offset % NVM_FLASH_WORD_SIZE) != 0U || (len % NVM_FLASH_WORD_SIZE) != 0U)
    {
        return false;
    }

    for (uint16_t written = 0; written < len; written += NVM_FLASH_WORD_SIZE)
    {
        uint32_t word[4];
        memcpy(word, data + written, NVM_FLASH_WORD_SIZE);

        NVMDATA0 = word[0];
        NVMDATA1 = word[1];
        NVMDATA2 = word[2];
        NVMDATA3 = word[3];

        NVMADR = NVM_FLASH_CONFIG_PAGE_ADDR + offset + written;
        NVMCONbits.WRERR = 0;
        NVMCONbits.WREN = 1;
        NVMCONbits.NVMOP = NVMOP_WORD_PROGRAM;
        NVMCONbits.WR = 1;

        if (!NVM_FLASH_WaitAndCheck())
        {
            return false;
        }
    }
    return true;
}

void NVM_FLASH_ReadBytes(uint32_t offset, uint8_t *data, uint16_t len)
{
    // Program Flash is directly memory-mapped for reads on this core - no
    // table-read instructions or Program Space Visibility window needed
    // (data sheet section 6.3.1: "Program memory is read linearly, similar
    // to how data memory is read").
    const uint8_t *src = (const uint8_t *)(NVM_FLASH_CONFIG_PAGE_ADDR + offset);
    memcpy(data, src, len);
}
