/*******************************************************************************
 * Company    : Tecnomic Components
 * File Name  : spi_flash.c
 * Description: Config-Driven Top-Level SPI Flash Manager with W25Q Support.
 *******************************************************************************/

#include <stdio.h>
#include "spi_flash.h"
#include "board_config.h"  /* Pulled automatically from boards/$(BOARD)/board_config.h */

/* ========================================================================== */
/* JEDEC Manufacturer ID Definitions                                          */
/* ========================================================================== */
#define MFR_ID_MICRON       0x20    /* Micron MT25Q Series */
#define MFR_ID_SST_3V       0xBF    /* 3.3V SST25 / SST26 Series */
#define MFR_ID_SST_1V8      0x62    /* 1.8V SST25WF Series */
#define MFR_ID_SST_LEGACY   0x31    /* Legacy SST Series */
#define MFR_ID_WINBOND      0xEF    /* Winbond W25QXX Series */

/* ========================================================================== */
/* Driver Operation Table Selection Based on Configuration                    */
/* ========================================================================== */
#if defined(CONFIG_FLASH_FAMILY) && (CONFIG_FLASH_FAMILY == FLASH_FAMILY_MICRON)
    extern const struct spi_flash_ops micron1g_flash_ops;
    static const struct spi_flash_ops *active_driver = &micron1g_flash_ops;

#elif defined(CONFIG_FLASH_FAMILY) && (CONFIG_FLASH_FAMILY == FLASH_FAMILY_SST)
    extern const struct spi_flash_ops sst25_flash_ops;
    static const struct spi_flash_ops *active_driver = &sst25_flash_ops;

#elif defined(CONFIG_FLASH_FAMILY) && (CONFIG_FLASH_FAMILY == FLASH_FAMILY_WINBOND)
    extern const struct spi_flash_ops w25qxx_flash_ops;
    static const struct spi_flash_ops *active_driver = &w25qxx_flash_ops;

#else
    /* Fallback / Auto-Detect Mode: Declare all supported drivers */
    extern const struct spi_flash_ops micron1g_flash_ops;
    extern const struct spi_flash_ops sst25_flash_ops;
    extern const struct spi_flash_ops w25qxx_flash_ops;
    static const struct spi_flash_ops *active_driver = &micron1g_flash_ops;
#endif

/* Fallback capacity cached from board configuration */
#if defined(CONFIG_FLASH_SIZE_BYTES)
static uint32_t g_cached_flash_size_bytes = CONFIG_FLASH_SIZE_BYTES;
#else
static uint32_t g_cached_flash_size_bytes = 0;
#endif

/**
 * @brief Decodes 3-byte JEDEC ID into Flash memory capacity in bytes.
 */
static uint32_t decode_jedec_capacity(uint8_t mfr_id, uint8_t dev_id, uint8_t capacity_id)
{
    (void)dev_id;

    /* 1. Micron MT25Q Series (Mfr 0x20) */
    if (mfr_id == MFR_ID_MICRON) {
        switch (capacity_id) {
            case 0x16: return 4U * 1024U * 1024U;     /* 32 Mbit = 4 MB */
            case 0x17: return 8U * 1024U * 1024U;     /* 64 Mbit = 8 MB */
            case 0x18: return 16U * 1024U * 1024U;    /* 128 Mbit = 16 MB */
            case 0x19: return 32U * 1024U * 1024U;    /* 256 Mbit = 32 MB */
            case 0x20: return 64U * 1024U * 1024U;    /* 512 Mbit = 64 MB */
            case 0x21: return 128U * 1024U * 1024U;   /* 1 Gbit = 128 MB */
            case 0x22: return 256U * 1024U * 1024U;   /* 2 Gbit = 256 MB */
            default: break;
        }
    }

    /* 2. Winbond W25QXX Series (Mfr 0xEF) */
    if (mfr_id == MFR_ID_WINBOND) {
        switch (capacity_id) {
            case 0x14: return 1U * 1024U * 1024U;     /* W25Q80:  8 Mbit = 1 MB */
            case 0x15: return 2U * 1024U * 1024U;     /* W25Q16: 16 Mbit = 2 MB */
            case 0x16: return 4U * 1024U * 1024U;     /* W25Q32: 32 Mbit = 4 MB */
            case 0x17: return 8U * 1024U * 1024U;     /* W25Q64: 64 Mbit = 8 MB */
            case 0x18: return 16U * 1024U * 1024U;    /* W25Q128: 128 Mbit = 16 MB */
            case 0x19: return 32U * 1024U * 1024U;    /* W25Q256: 256 Mbit = 32 MB */
            case 0x20: return 64U * 1024U * 1024U;    /* W25Q512: 512 Mbit = 64 MB */
            case 0x21: return 128U * 1024U * 1024U;   /* W25M02:  1 Gbit = 128 MB */
            default: break;
        }
    }

    /* 3. SST 3.3V Series (Mfr 0xBF) */
    if (mfr_id == MFR_ID_SST_3V) {
        switch (capacity_id) {
            case 0x8C: return 256U * 1024U;      /* 2 Mbit = 256 KB */
            case 0x8D: return 512U * 1024U;      /* 4 Mbit = 512 KB */
            case 0x8E: return 1024U * 1024U;     /* 8 Mbit = 1 MB */
            case 0x41: return 2048U * 1024U;     /* 16 Mbit = 2 MB */
            case 0x4A: return 4096U * 1024U;     /* 32 Mbit = 4 MB */
            case 0x4B: return 8192U * 1024U;     /* 64 Mbit = 8 MB */
            default: break;
        }
    }

    /* 4. SST 1.8V Low-Voltage Series (Mfr 0x62) */
    if (mfr_id == MFR_ID_SST_1V8) {
        switch (capacity_id) {
            case 0x12: return 256U * 1024U;      /* 2 Mbit = 256 KB */
            case 0x13: return 512U * 1024U;      /* 4 Mbit = 512 KB */
            case 0x14: return 1024U * 1024U;     /* 8 Mbit = 1 MB */
            default: break;
        }
    }

    /* 5. Fallback Power-of-Two Calculation for sizes <= 512MB (capacity_id <= 29) */
    if (capacity_id >= 0x10 && capacity_id <= 0x1D) {
        return (1U << capacity_id);
    }

    return 0;
}

uint32_t spi_flash_get_size_bytes(void)
{
    return g_cached_flash_size_bytes;
}

void spi_flash_get_formatted_size(char *buf, size_t buf_len)
{
    uint32_t bytes = g_cached_flash_size_bytes;

    if (bytes == 0) {
        snprintf(buf, buf_len, "Unknown Size");
    } else if (bytes >= (1024U * 1024U)) {
        snprintf(buf, buf_len, "%u MB (%u KB)",
                 (unsigned int)(bytes / (1024U * 1024U)),
                 (unsigned int)(bytes / 1024U));
    } else {
        snprintf(buf, buf_len, "%u KB (%u bytes)",
                 (unsigned int)(bytes / 1024U),
                 (unsigned int)bytes);
    }
}

int spi_flash_init(void)
{
    uint8_t mfr_id = 0;
    uint8_t dev_id = 0;
    uint8_t capacity_id = 0;
    char size_str[32];

    /* 1. Print Auto-Detection Status Log */
#if defined(FLASH_USE_MSS_SPI0)
    printf("[SPI FLASH] Auto-detecting Flash on MikroBus...\n");
#else
    printf("[SPI FLASH] Auto-detecting Flash memory...\n");
#endif

    if (!active_driver || !active_driver->init) {
        printf("[SPI FLASH] Error: No driver bound in configuration!\n");
        return -1;
    }

    /* 2. Initialize low-level SPI controller */
    if (active_driver->init() != 0) {
        printf("[SPI FLASH] Error: SPI hardware driver init failed!\n");
        return -1;
    }

    /* 3. Read 3-byte JEDEC ID opcode (0x9F) */
    if (active_driver->read_id) {
        active_driver->read_id(&mfr_id, &dev_id, &capacity_id);
    }

    if (mfr_id == 0x00 || mfr_id == 0xFF) {
        printf("[SPI FLASH] Error: No Flash memory detected on MikroBus! (Mfr ID: 0x%02X)\n", mfr_id);
        active_driver = NULL;
        return -1;
    }

    /* 4. Re-bind active driver based on detected Manufacturer ID */
#if !defined(SPI_FLASH_FAMILY) || (SPI_FLASH_FAMILY == FLASH_FAMILY_AUTO)
    switch (mfr_id) {
        case MFR_ID_WINBOND:   /* 0xEF */
            active_driver = &w25qxx_flash_ops;
            break;

        case MFR_ID_SST_3V:    /* 0xBF */
        case MFR_ID_SST_1V8:   /* 0x62 */
        case MFR_ID_SST_LEGACY:/* 0x31 */
            active_driver = &sst25_flash_ops;
            break;

        case MFR_ID_MICRON:    /* 0x20 */
            active_driver = &micron1g_flash_ops;
            break;

        default:
            printf("[SPI FLASH] Warning: Unknown Mfr ID 0x%02X, using fallback driver.\n", mfr_id);
            break;
    }

    /* Re-initialize target driver (e.g., set up 4-byte mode or global unprotect) */
    if (active_driver && active_driver->init) {
        active_driver->init();
    }
#endif

    /* 5. Calculate and cache total Flash size */
    uint32_t decoded_bytes = decode_jedec_capacity(mfr_id, dev_id, capacity_id);
    if (decoded_bytes > 0) {
        g_cached_flash_size_bytes = decoded_bytes;
    }

    spi_flash_get_formatted_size(size_str, sizeof(size_str));

    /* 6. Print detection summary log */
    printf("[SPI FLASH] Detected: %s (Mfr: 0x%02X, Dev: 0x%02X, Cap: 0x%02X) [%s]\n",
           active_driver->name, mfr_id, dev_id, capacity_id, size_str);

    return 0;
}


int spi_flash_read_id(uint8_t *mfr_id, uint8_t *dev_id, uint8_t *capacity_id)
{
    if (active_driver && active_driver->read_id) {
        return active_driver->read_id(mfr_id, dev_id, capacity_id);
    }
    return -1;
}

int spi_flash_read(uint32_t offset, uint8_t *buf, size_t len)
{
    if (active_driver && active_driver->read) {
        return active_driver->read(offset, buf, len);
    }
    return -1;
}

int spi_flash_write(uint32_t offset, const uint8_t *buf, size_t len)
{
    if (active_driver && active_driver->write) {
        return active_driver->write(offset, buf, len);
    }
    return -1;
}

int spi_flash_erase(uint32_t offset, size_t len)
{
    if (active_driver && active_driver->erase) {
        return active_driver->erase(offset, len);
    }
    return -1;
}

const char *spi_flash_get_driver_name(void)
{
    if (active_driver && active_driver->name) {
        return active_driver->name;
    }
    return "No Driver Loaded / Unbound";
}