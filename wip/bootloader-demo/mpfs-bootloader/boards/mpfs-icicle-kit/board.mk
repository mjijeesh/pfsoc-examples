# Fallback board defaults if board_config.mk is missing

DIE                 ?= MPFS250T
PACKAGE             ?= FCVG484

MSS_UART            ?= MSS_UART0
BAUD_RATE           ?= 115200

# SPI Hardware Engine ( MSS_SPI0, MSS_SPI1 or  CORE_SPI
SPI_FLASH_ENGINE    ?= CORE_SPI
FLASH_FAMILY        ?= MICRON

SPI_FLASH_BOOT_ADDR ?= 0x00100000
PAYLOAD_RAM_ADDR    ?= 0x80000000

ENABLE_SPI_FLASH    ?= 1
ENABLE_SDCARD       ?= 0
ENABLE_FATFS        ?= 0
