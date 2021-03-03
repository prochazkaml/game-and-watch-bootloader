#include <string.h>

#include "flash.h"
#include "stm32.h"

static quad_mode_t g_quad_mode = SPI_MODE;
static spi_chip_vendor_t g_vendor = VENDOR_MX;

/**
  * @brief  Set the command lines based on the chip used.
  * @param  cmd: Command handle.
  * @param  quad_mode: SPI_MODE or QUAD_MODE. Depends on the connection.
  * @param  vendor: VENDOR_MX or VENDOR_ISSI. Depends on the chip used.
  * @param  has_address: Set to true if the chip has address lines.
  * @param  has_data: Set to true if the chip has data lines.
  * @return Nothing.
  */
void set_cmd_lines(OSPI_RegularCmdTypeDef *cmd, quad_mode_t quad_mode, spi_chip_vendor_t vendor, uint8_t has_address, uint8_t has_data)
{
  if (quad_mode == SPI_MODE) {
    cmd->InstructionMode     = HAL_OSPI_INSTRUCTION_1_LINE;
    cmd->AddressMode         = has_address ? HAL_OSPI_ADDRESS_1_LINE : HAL_OSPI_ADDRESS_NONE;
    cmd->DataMode            = has_data ? HAL_OSPI_DATA_1_LINE : HAL_OSPI_DATA_NONE;
  } else {
    // QUAD_MODE
    if (vendor == VENDOR_MX) {
      cmd->InstructionMode   = HAL_OSPI_INSTRUCTION_1_LINE;
      cmd->AddressMode       = has_address ? HAL_OSPI_ADDRESS_4_LINES : HAL_OSPI_ADDRESS_NONE;
      cmd->DataMode          = has_data ? HAL_OSPI_DATA_4_LINES : HAL_OSPI_DATA_NONE;
    } else {
      // VENDOR_ISSI
      cmd->InstructionMode   = HAL_OSPI_INSTRUCTION_4_LINES;
      cmd->AddressMode       = has_address ? HAL_OSPI_ADDRESS_4_LINES : HAL_OSPI_ADDRESS_NONE;
      cmd->DataMode          = has_data ? HAL_OSPI_DATA_4_LINES : HAL_OSPI_DATA_NONE;
    }
  }
}

/**
  * @brief  Read raw data from the flash memory.
  * @param  hospi: OSPI handle.
  * @param  instruction: Flash instrucion. Refer to the flash datasheet.
  * @param  data: Pointer to a data buffer.
  * @param  len: Number of bytes to read.
  * @return Nothing.
  */
void OSPI_ReadBytes(OSPI_HandleTypeDef *hospi, uint8_t instruction, uint8_t *data, size_t len)
{
  OSPI_RegularCmdTypeDef  sCommand;
  memset(&sCommand, 0x0, sizeof(sCommand));
  sCommand.OperationType         = HAL_OSPI_OPTYPE_COMMON_CFG;
  sCommand.FlashId               = 0;
  sCommand.Instruction           = instruction;
  sCommand.InstructionSize       = HAL_OSPI_INSTRUCTION_8_BITS;
  
  sCommand.AlternateBytesMode    = HAL_OSPI_ALTERNATE_BYTES_NONE;
  sCommand.NbData                = len;
  sCommand.DummyCycles           = 0;
  sCommand.DQSMode               = HAL_OSPI_DQS_DISABLE;
  sCommand.SIOOMode              = HAL_OSPI_SIOO_INST_EVERY_CMD;
  sCommand.InstructionDtrMode    = HAL_OSPI_INSTRUCTION_DTR_DISABLE;

  if (g_vendor == VENDOR_MX) {
    set_cmd_lines(&sCommand, SPI_MODE, g_vendor, 0, 1);
  } else if (g_vendor == VENDOR_ISSI) {
    set_cmd_lines(&sCommand, g_quad_mode, g_vendor, 0, 1);
  }

  if (HAL_OSPI_Command(hospi, &sCommand, HAL_OSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
  {
    Error_Handler();
  }

  if(HAL_OSPI_Receive(hospi, data, HAL_OSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK) {
    Error_Handler();
  }
}

/**
  * @brief  Write raw data to the flash memory.
  * @param  hospi: OSPI handle.
  * @param  instruction: Flash instrucion. Refer to the flash datasheet.
  * @param  dummy_cycles: Number of cycles to wait.
  * @param  data: Pointer to a data buffer.
  * @param  len: Number of bytes to write.
  * @param  quad_mode: SPI_MODE or QUAD_MODE. Depends on the connection.
  * @return Nothing.
  */
void OSPI_WriteBytes(OSPI_HandleTypeDef *hospi, uint8_t instruction, uint8_t dummy_cycles, uint8_t *data, size_t len, quad_mode_t quad_mode)
{
  OSPI_RegularCmdTypeDef  sCommand;
  memset(&sCommand, 0x0, sizeof(sCommand));
  sCommand.OperationType         = HAL_OSPI_OPTYPE_COMMON_CFG;
  sCommand.FlashId               = 0;
  sCommand.Instruction           = instruction;
  sCommand.InstructionSize       = HAL_OSPI_INSTRUCTION_8_BITS;
  
  sCommand.AlternateBytesMode    = HAL_OSPI_ALTERNATE_BYTES_NONE;
  sCommand.NbData                = len;
  sCommand.DummyCycles           = dummy_cycles;
  sCommand.DQSMode               = HAL_OSPI_DQS_DISABLE;
  sCommand.SIOOMode              = HAL_OSPI_SIOO_INST_EVERY_CMD;
  sCommand.InstructionDtrMode    = HAL_OSPI_INSTRUCTION_DTR_DISABLE;

  set_cmd_lines(&sCommand, quad_mode, g_vendor, 0, len > 0);

  if (HAL_OSPI_Command(hospi, &sCommand, HAL_OSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
  {
    Error_Handler();
  }

  if (len > 0) {
    if(HAL_OSPI_Transmit(hospi, data, HAL_OSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK) {
      Error_Handler();
    }
  }
}

/**
  * @brief  Initialize OSPI.
  * @param  hospi: OSPI handle.
  * @param  quad_mode: SPI_MODE or QUAD_MODE. Depends on the connection.
  * @param  vendor: VENDOR_MX or VENDOR_ISSI. Depends on the chip used.
  * @return Nothing.
  */
void OSPI_Init(OSPI_HandleTypeDef *hospi, quad_mode_t quad_mode, spi_chip_vendor_t vendor)
{
  if (vendor == VENDOR_ISSI) {
    // Disable quad mode (will do nothing in SPI mode)
    OSPI_WriteBytes(hospi, 0xf5, 0, NULL, 0, QUAD_MODE);
    HAL_Delay(2);
  }

  // Enable Reset
  OSPI_WriteBytes(hospi, 0x66, 0, NULL, 0, SPI_MODE);
  HAL_Delay(2);

  // Reset
  OSPI_WriteBytes(hospi, 0x99, 0, NULL, 0, SPI_MODE);
  HAL_Delay(20);

  g_vendor = vendor;
  g_quad_mode = quad_mode;

  if (quad_mode == QUAD_MODE) {
    if (vendor == VENDOR_MX) {
      // WRSR - Write Status Register
      // Set Quad Enable bit (6) in status register. Other bits = 0.
      uint8_t wr_status = 1<<6;
      uint8_t rd_status = 0xff;

      // Enable write to be allowed to change the status register
      OSPI_NOR_WriteEnable(hospi);

      // Loop until rd_status is updated
      while ((rd_status & wr_status) != wr_status) {
        OSPI_WriteBytes(hospi, 0x01, 0, &wr_status, 1, SPI_MODE);
        OSPI_ReadBytes(hospi, 0x05, &rd_status, 1);
      }
    } else if (vendor == VENDOR_ISSI) {
      // Enable QPI mode
      OSPI_WriteBytes(hospi, 0x35, 0, NULL, 0, SPI_MODE);
    }
  }
}

/**
  * @brief  Erase the entire flash.
  * @param  hospi: OSPI handle.
  * @return Nothing.
  */
void OSPI_ChipErase(OSPI_HandleTypeDef *hospi)
{
  uint8_t status;

  // Send Chip Erase command
  OSPI_WriteBytes(hospi, 0x60, 0, NULL, 0, g_quad_mode);

  // Wait for Write In Progress Bit to be zero
  do {
    OSPI_ReadBytes(hospi, 0x05, &status, 1);
    HAL_Delay(100);
  } while((status & 0x01) == 0x01);
}

/**
  * @brief  Erase a 64 kB block.
  * @param  hospi: OSPI handle.
  * @param  address: Block address.
  * @return Nothing.
  */
void OSPI_BlockErase(OSPI_HandleTypeDef *hospi, uint32_t address)
{
  uint8_t status;
  OSPI_RegularCmdTypeDef  sCommand;

  memset(&sCommand, 0x0, sizeof(sCommand));
  sCommand.OperationType         = HAL_OSPI_OPTYPE_COMMON_CFG;
  sCommand.FlashId               = 0;
  sCommand.Instruction           = 0xD8; // BE Block Erase
  sCommand.InstructionSize       = HAL_OSPI_INSTRUCTION_8_BITS;
  sCommand.Address               = address;
  sCommand.AddressSize           = HAL_OSPI_ADDRESS_24_BITS;
  sCommand.AlternateBytesMode    = HAL_OSPI_ALTERNATE_BYTES_NONE;
  sCommand.NbData                = 0;
  sCommand.DummyCycles           = 0;
  sCommand.DQSMode               = HAL_OSPI_DQS_DISABLE;
  sCommand.SIOOMode              = HAL_OSPI_SIOO_INST_ONLY_FIRST_CMD;
  sCommand.InstructionDtrMode    = HAL_OSPI_INSTRUCTION_DTR_DISABLE;

  set_cmd_lines(&sCommand, g_quad_mode, g_vendor, 1, 0);

  if (HAL_OSPI_Command(hospi, &sCommand, HAL_OSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
  {
    Error_Handler();
  }

  // Wait for Write In Progress Bit to be zero
  do {
    OSPI_ReadBytes(hospi, 0x05, &status, 1);
  } while((status & 0x01) == 0x01);
}

/**
  * @brief  Erase a 4 kB sector.
  * @param  hospi: OSPI handle.
  * @param  address: Sector address.
  * @return Nothing.
  */
void OSPI_SectorErase(OSPI_HandleTypeDef *hospi, uint32_t address)
{
  uint8_t status;
  OSPI_RegularCmdTypeDef  sCommand;

  memset(&sCommand, 0x0, sizeof(sCommand));
  sCommand.OperationType         = HAL_OSPI_OPTYPE_COMMON_CFG;
  sCommand.FlashId               = 0;
  sCommand.Instruction           = 0x20; // Sector Erase (4kB)
  sCommand.InstructionSize       = HAL_OSPI_INSTRUCTION_8_BITS;
  sCommand.Address               = address;
  sCommand.AddressSize           = HAL_OSPI_ADDRESS_24_BITS;
  sCommand.AlternateBytesMode    = HAL_OSPI_ALTERNATE_BYTES_NONE;
  sCommand.NbData                = 0;
  sCommand.DummyCycles           = 0;
  sCommand.DQSMode               = HAL_OSPI_DQS_DISABLE;
  sCommand.SIOOMode              = HAL_OSPI_SIOO_INST_ONLY_FIRST_CMD;
  sCommand.InstructionDtrMode    = HAL_OSPI_INSTRUCTION_DTR_DISABLE;

  set_cmd_lines(&sCommand, g_quad_mode, g_vendor, 1, 0);

  if (HAL_OSPI_Command(hospi, &sCommand, HAL_OSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
  {
    Error_Handler();
  }

  // Wait for Write In Progress Bit to be zero
  do {
    OSPI_ReadBytes(hospi, 0x05, &status, 1);
  } while((status & 0x01) == 0x01);
}

void  _OSPI_Program(OSPI_HandleTypeDef *hospi, uint32_t address, uint8_t *buffer, size_t buffer_size)
{
  uint8_t status;
  OSPI_RegularCmdTypeDef  sCommand;

  memset(&sCommand, 0x0, sizeof(sCommand));
  sCommand.OperationType         = HAL_OSPI_OPTYPE_COMMON_CFG;
  sCommand.FlashId               = 0;
  sCommand.Instruction           = 0x02; // PP
  sCommand.InstructionSize       = HAL_OSPI_INSTRUCTION_8_BITS;
  sCommand.Address               = address;
  sCommand.AddressSize           = HAL_OSPI_ADDRESS_24_BITS;
  sCommand.AlternateBytesMode    = HAL_OSPI_ALTERNATE_BYTES_NONE;
  sCommand.NbData = buffer_size;
  sCommand.DummyCycles           = 0;
  sCommand.DQSMode               = HAL_OSPI_DQS_DISABLE;
  sCommand.SIOOMode              = HAL_OSPI_SIOO_INST_ONLY_FIRST_CMD;
  sCommand.InstructionDtrMode    = HAL_OSPI_INSTRUCTION_DTR_DISABLE;

  // For MX vendor in quad mode, use the 4PP command
  if (g_quad_mode == QUAD_MODE && g_vendor == VENDOR_MX) {
    sCommand.Instruction         = 0x38; // 4PP
  }

  set_cmd_lines(&sCommand, g_quad_mode, g_vendor, 1, 1);

  if(buffer_size > 256) {
    Error_Handler();
  }

  if (HAL_OSPI_Command(hospi, &sCommand, HAL_OSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
  {
    Error_Handler();
  }

  if(HAL_OSPI_Transmit(hospi, buffer, HAL_OSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK) {
    Error_Handler();
  }

  // Wait for Write In Progress Bit to be zero
  do {
    OSPI_ReadBytes(hospi, 0x05, &status, 1);
  } while((status & 0x01) == 0x01);
}

/**
  * @brief  Write data to the flash.
  * @param  hospi: OSPI handle.
  * @param  address: Destination address.
  * @param  buffer: Pointer to a data buffer.
  * @param  buffer_size: Number of bytes to write.
  * @return Nothing.
  */
void OSPI_Program(OSPI_HandleTypeDef *hospi, uint32_t address, uint8_t *buffer, int32_t buffer_size)
{
  unsigned dest_page = address / 256;
  int i = 0;

  while (buffer_size > 0) {
    OSPI_NOR_WriteEnable(hospi);
    _OSPI_Program(hospi, (i + dest_page) * 256, buffer + (i * 256), buffer_size > 256 ? 256 : buffer_size);
    i++;
    buffer_size -= 256;
  }
}


void _OSPI_Read(OSPI_HandleTypeDef *hospi, uint32_t address, uint8_t *buffer, size_t buffer_size)
{
  OSPI_RegularCmdTypeDef  sCommand;

  memset(&sCommand, 0x0, sizeof(sCommand));
  sCommand.OperationType         = HAL_OSPI_OPTYPE_COMMON_CFG;
  sCommand.FlashId               = 0;
  sCommand.Instruction           = 0x0B; // FAST_READ
  sCommand.InstructionSize       = HAL_OSPI_INSTRUCTION_8_BITS;
  sCommand.Address               = address;
  sCommand.AddressSize           = HAL_OSPI_ADDRESS_24_BITS;
  sCommand.AlternateBytesMode    = HAL_OSPI_ALTERNATE_BYTES_NONE;
  sCommand.NbData = buffer_size;
  sCommand.DummyCycles           = 8;
  sCommand.DQSMode               = HAL_OSPI_DQS_DISABLE;
  sCommand.SIOOMode              = HAL_OSPI_SIOO_INST_ONLY_FIRST_CMD;
  sCommand.InstructionDtrMode    = HAL_OSPI_INSTRUCTION_DTR_DISABLE;

  set_cmd_lines(&sCommand, g_quad_mode, g_vendor, 1, 1);

  if(buffer_size > 256) {
    Error_Handler();
  }

  if (HAL_OSPI_Command(hospi, &sCommand, HAL_OSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
  {
    Error_Handler();
  }

  if(HAL_OSPI_Receive(hospi, buffer, HAL_OSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK) {
    Error_Handler();
  }
}

/**
  * @brief  Read data from the flash.
  * @param  hospi: OSPI handle.
  * @param  address: Source address.
  * @param  buffer: Pointer to a data buffer.
  * @param  buffer_size: Number of bytes to read.
  * @return Nothing.
  */
void OSPI_Read(OSPI_HandleTypeDef *hospi, uint32_t address, uint8_t *buffer, int32_t buffer_size)
{
  unsigned iterations = buffer_size / 256;
  unsigned dest_page = address / 256;

  for(int i = 0; i < iterations; i++) {
    _OSPI_Read(hospi, (i + dest_page) * 256, buffer + (i * 256), buffer_size > 256 ? 256 : buffer_size);
    buffer_size -= 256;
  }
}

/**
  * @brief  Unlock flash write protection.
  * @param  hospi: OSPI handle.
  * @return Nothing.
  */
void OSPI_NOR_WriteEnable(OSPI_HandleTypeDef *hospi)
{
  OSPI_WriteBytes(hospi, 0x06, 0, NULL, 0, g_quad_mode);
}
