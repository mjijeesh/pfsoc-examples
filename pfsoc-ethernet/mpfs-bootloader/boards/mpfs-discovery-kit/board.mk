# Fallback board defaults if board_config.mk is missing

DIE                 ?= MPFS095T
PACKAGE             ?= FCSG325

MSS_UART            ?= MSS_UART0
BAUD_RATE           ?= 115200

# SPI Hardware Engine ( MSS_SPI0, MSS_SPI1 or  CORE_SPI
SPI_FLASH_ENGINE    ?= MSS_SPI0
FLASH_FAMILY        ?= SST25

SPI_FLASH_BOOT_ADDR ?= 0x00002000
PAYLOAD_RAM_ADDR    ?= 0x80000000

ENABLE_SPI_FLASH    ?= 1
ENABLE_SDCARD       ?= 0
ENABLE_FATFS        ?= 0
