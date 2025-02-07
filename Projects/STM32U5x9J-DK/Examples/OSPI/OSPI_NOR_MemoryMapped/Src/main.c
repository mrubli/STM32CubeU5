/**
  ******************************************************************************
  * @file    OSPI/OSPI_NOR_MemoryMapped/Src/main.c
  * @author  MCD Application Team
  * @brief   This example describes how to configure and use OctoSPI through
  *          the STM32U5xx HAL API.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2021 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include <stdio.h>
#include <stdbool.h>
#include <string.h>

/** @addtogroup STM32U5xx_HAL_Examples
  * @{
  */

/** @addtogroup OSPI_NOR_MemoryMapped
  * @{
  */

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
XSPI_HandleTypeDef OSPIHandle;
static uint8_t TempBuffer[256 + 16];

/* Private function prototypes -----------------------------------------------*/
static void SystemClock_Config(void);
static void CACHE_Enable(void);
static void Error_Handler(void);
static void OSPI_WriteEnable(XSPI_HandleTypeDef *hospi);
static void OSPI_AutoPollingMemReady(XSPI_HandleTypeDef *hospi);
static void OSPI_OctalModeCfg(XSPI_HandleTypeDef *hospi);
HAL_StatusTypeDef OSPIClock_Config(void);

/* Private functions ---------------------------------------------------------*/

int __io_putchar(int ch)
{
  return ITM_SendChar(ch);
}

static void dump_flash_at_addr(const void *addr, size_t len)
{
  const int BytesPerLine = 16;
  printf("ext_flash@%p:\n", addr);
  for (int i = 0; i < len; i++)
  {
    printf("0x%02x ", ((const uint8_t *)(addr))[i]);
    if (i % BytesPerLine == BytesPerLine - 1)
      printf("\n");
  }
  if (len % BytesPerLine != 0)
    printf("\n");
}

static HAL_StatusTypeDef XSPI_NOR_Read(XSPI_HandleTypeDef *hxspi, uint32_t addr,
  uint8_t *pData, uint32_t size)
{
  XSPI_RegularCmdTypeDef sCommand = {0};

  sCommand.OperationType      = HAL_XSPI_OPTYPE_COMMON_CFG;
  sCommand.IOSelect           = HAL_XSPI_SELECT_IO_7_0;				// default
  sCommand.Instruction        = OCTAL_IO_READ_CMD;
  sCommand.InstructionMode    = HAL_XSPI_INSTRUCTION_8_LINES;		// common
  sCommand.InstructionWidth   = HAL_XSPI_INSTRUCTION_16_BITS;		// common
  sCommand.InstructionDTRMode = HAL_XSPI_INSTRUCTION_DTR_DISABLE;	// common
  sCommand.Address            = addr;
  sCommand.AddressMode        = HAL_XSPI_ADDRESS_8_LINES;
  sCommand.AddressWidth       = HAL_XSPI_ADDRESS_32_BITS;			// common
  sCommand.AddressDTRMode     = HAL_XSPI_ADDRESS_DTR_DISABLE;		// common
  sCommand.AlternateBytes     = 0;									// default
  sCommand.AlternateBytesMode = HAL_XSPI_ALT_BYTES_NONE;			// default
  sCommand.AlternateBytesWidth = HAL_XSPI_ALT_BYTES_8_BITS;			// default
  sCommand.AlternateBytesDTRMode = HAL_XSPI_ALT_BYTES_DTR_DISABLE;	// default
  sCommand.DataMode           = HAL_XSPI_DATA_8_LINES;
  sCommand.DataLength         = size;
  sCommand.DataDTRMode        = HAL_XSPI_DATA_DTR_DISABLE;			// common
  sCommand.DummyCycles        = DUMMY_CLOCK_CYCLES_READ;
  sCommand.DQSMode            = HAL_XSPI_DQS_DISABLE;				// common
  sCommand.SIOOMode           = HAL_XSPI_SIOO_INST_EVERY_CMD;		// common

  if (HAL_XSPI_Command(hxspi, &sCommand, HAL_XSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
  {
    Error_Handler();
    return HAL_ERROR;
  }

  if (HAL_XSPI_Receive(hxspi, pData, HAL_XSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
  {
    Error_Handler();
    return HAL_ERROR;
  }

  return HAL_OK;
}

static HAL_StatusTypeDef XSPI_NOR_Erase_Block(XSPI_HandleTypeDef *hxspi, uint32_t BlockAddress)
{
  XSPI_RegularCmdTypeDef sCommand = {0};

  sCommand.InstructionMode    = HAL_XSPI_INSTRUCTION_8_LINES;
  sCommand.InstructionWidth   = HAL_XSPI_INSTRUCTION_16_BITS;
  sCommand.InstructionDTRMode = HAL_XSPI_INSTRUCTION_DTR_DISABLE;
  sCommand.AddressWidth       = HAL_XSPI_ADDRESS_32_BITS;
  sCommand.AddressDTRMode     = HAL_XSPI_ADDRESS_DTR_DISABLE;
  sCommand.AlternateBytesMode = HAL_XSPI_ALT_BYTES_NONE;
  sCommand.DataDTRMode        = HAL_XSPI_DATA_DTR_DISABLE;
  sCommand.DQSMode            = HAL_XSPI_DQS_DISABLE;
  sCommand.SIOOMode           = HAL_XSPI_SIOO_INST_EVERY_CMD;

  /* Enable write operations ------------------------------------------ */
  OSPI_WriteEnable(hxspi);

  /* Erasing Sequence ------------------------------------------------- */
  sCommand.OperationType = HAL_XSPI_OPTYPE_COMMON_CFG;
  sCommand.Instruction   = OCTAL_SECTOR_ERASE_CMD;
  sCommand.AddressMode   = HAL_XSPI_ADDRESS_8_LINES;
  sCommand.Address       = BlockAddress;
  sCommand.DataMode      = HAL_XSPI_DATA_NONE;
  sCommand.DummyCycles   = 0;

  if (HAL_XSPI_Command(hxspi, &sCommand, HAL_MAX_DELAY) != HAL_OK)
  {
    Error_Handler();
    return HAL_ERROR;
  }

  /* Configure automatic polling mode to wait for end of erase ------ */
  OSPI_AutoPollingMemReady(&OSPIHandle);

  return HAL_OK;
}

static void XSPI_NOR_EnableMemoryMapped(XSPI_HandleTypeDef *hxspi)
{
  XSPI_RegularCmdTypeDef sCommand = {0};

  sCommand.AddressDTRMode     = HAL_XSPI_ADDRESS_DTR_DISABLE;
  sCommand.AddressMode        = HAL_XSPI_ADDRESS_8_LINES;
  sCommand.AddressWidth       = HAL_XSPI_ADDRESS_32_BITS;
  sCommand.AlternateBytesMode = HAL_XSPI_ALT_BYTES_NONE;
  sCommand.DataDTRMode        = HAL_XSPI_DATA_DTR_DISABLE;
  sCommand.DataMode      = HAL_XSPI_DATA_8_LINES;
  sCommand.DataLength    = 1;
  sCommand.DQSMode            = HAL_XSPI_DQS_DISABLE;
  sCommand.InstructionDTRMode = HAL_XSPI_INSTRUCTION_DTR_DISABLE;
  sCommand.InstructionMode    = HAL_XSPI_INSTRUCTION_8_LINES;
  sCommand.InstructionWidth   = HAL_XSPI_INSTRUCTION_16_BITS;
  sCommand.SIOOMode           = HAL_XSPI_SIOO_INST_EVERY_CMD;

  /* Memory-mapped mode configuration ------------------------------- */
  sCommand.DQSMode       = HAL_XSPI_DQS_DISABLE;
  sCommand.DummyCycles   = DUMMY_CLOCK_CYCLES_READ;
  sCommand.Instruction   = OCTAL_IO_READ_CMD;
  sCommand.OperationType = HAL_XSPI_OPTYPE_READ_CFG;

  if (HAL_XSPI_Command(hxspi, &sCommand, HAL_XSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
  {
    Error_Handler();
  }

  /* Memory-mapped mode configuration ------------------------------- */
  sCommand.DQSMode       = HAL_XSPI_DQS_ENABLE;
  sCommand.DummyCycles        = 0;
  sCommand.Instruction   = OCTAL_PAGE_PROG_CMD;
  sCommand.OperationType = HAL_XSPI_OPTYPE_WRITE_CFG;

  if (HAL_XSPI_Command(hxspi, &sCommand, HAL_XSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
  {
    Error_Handler();
  }

  XSPI_MemoryMappedTypeDef sMemMappedCfg = {0};
  sMemMappedCfg.TimeOutActivation     = HAL_XSPI_TIMEOUT_COUNTER_ENABLE;
  sMemMappedCfg.TimeoutPeriodClock    = 0x40;
  if (HAL_XSPI_MemoryMapped(hxspi, &sMemMappedCfg) != HAL_OK)
  {
    Error_Handler();
  }
}

static void XSPI_NOR_DisableMemoryMapped(XSPI_HandleTypeDef *hxspi)
{
  if (HAL_XSPI_Abort(hxspi) != HAL_OK)
  {
    Error_Handler();
  }
}

static void dump_flash_at_offset(size_t offset, size_t len)
{
  const int BytesPerLine = 16;
  static uint8_t buf[32];

  printf("ext_flash@+%x:\n", offset);
  size_t dumped_len = 0;
  while (dumped_len < len)
  {
    size_t read_len = len - dumped_len;
    if (read_len > sizeof(buf))
      read_len = sizeof(buf);

    if (XSPI_NOR_Read(&OSPIHandle, offset + dumped_len, buf, read_len) != HAL_OK)
    {
      printf("Read failed\n");
      return;
    }

    for (int i = 0; i < read_len; i++)
    {
      printf("0x%02x ", buf[i]);
      if (i % BytesPerLine == BytesPerLine - 1)
        printf("\n");
    }
    dumped_len += read_len;
  }
  if (len % BytesPerLine != 0)
    printf("\n");
}

#define DBG_PRINT(fmt, ...) printf(fmt "\n" __VA_OPT__(, ) __VA_ARGS__);

uint32_t * const ext_flash_start = (uint32_t *)OCTOSPI1_BASE;
//uint32_t * const ext_flash_end   = (uint32_t *)&_ext_flash_end;
//const uint32_t   ext_flash_size  = (ext_flash_end - ext_flash_start) * sizeof(*ext_flash_start);

void ext_flash_erase_block_addr(uint32_t blockAddr)
{
  DBG_PRINT("erasing block 0x%08lx ...", blockAddr);
  const HAL_StatusTypeDef status = XSPI_NOR_Erase_Block(&OSPIHandle, blockAddr);
  if (status == HAL_OK)
  {
    DBG_PRINT("erased block 0x%08lx", blockAddr);
  }
  else
  {
    DBG_PRINT("ERROR: failed to erase block 0x%08lx: %d", blockAddr, status);
    Error_Handler();
  }
}

void ext_flash_test_memory_mapped(bool read, bool write, bool verify)
{
  DBG_PRINT("enabling memory-mapping ...");
  XSPI_NOR_EnableMemoryMapped(&OSPIHandle);
  DBG_PRINT("memory-mapping enabled");

  if (read)
  {
    DBG_PRINT("reading memory-mapped ...");
    dump_flash_at_addr(ext_flash_start, 32);
    //dump_flash_at_addr(ext_flash_end - 32, 32);
    DBG_PRINT("done reading memory-mapped");
  }

  if (write)
  {
    DBG_PRINT("writing memory-mapped ...");
    uint8_t *mem = (uint8_t *)(ext_flash_start);
    for (int i = 0; i < 256; i++)
      *mem++ = (uint8_t)i;

    HAL_Delay(MEMORY_PAGE_PROG_DELAY);
    DBG_PRINT("done writing memory-mapped");
  }

  if (verify)
  {
    DBG_PRINT("verifying memory-mapped ...");
    uint8_t *mem = (uint8_t *)(ext_flash_start);
    int bad = 0;
    for (int i = 0; i < 256; i++)
    {
      if (*mem++ != (uint8_t)i)
      {
        bad++;
      }
    }
    if (bad == 0)
    {
      DBG_PRINT("verification successful");
    }
    else
    {
      DBG_PRINT("verification FAILED (%d bad bytes)", bad);
    }
  }

  DBG_PRINT("disabling memory-mapping ...");
  XSPI_NOR_DisableMemoryMapped(&OSPIHandle);
  DBG_PRINT("memory-mapping disabled");
}

void ext_flash_test_indirect(bool read, bool write, bool verify)
{
  static uint8_t buf[256];

  memset(buf, 0, sizeof(buf));

  if (read)
  {
    DBG_PRINT("reading indirect ...");
    dump_flash_at_offset(0, 32);
    //dump_flash_at_offset(ext_flash_size - 32, 32);
    DBG_PRINT("done reading indirect");
  }

  if (write)
  {
#if 1
    printf("ERROR: indirect writing not implemented");
#else
    for (int i = 0; i < sizeof(buf); i++)
      buf[i] = i;

    DBG_PRINT("writing indirect ...");
    const auto status = XSPI_NOR_Write(&OSPIHandle, (uint8_t *)buf, 0, sizeof(buf));
    if (status == 0)
    {
      DBG_PRINT("done writing indirect");
    }
    else
    {
      DBG_PRINT("ERROR: indirect write failed: %d", status);
      Error_Handler();
    }
#endif
  }

  if (verify)
  {
    DBG_PRINT("verifying indirect ...");

    const HAL_StatusTypeDef status = XSPI_NOR_Read(&OSPIHandle, 0, (uint8_t *)buf, sizeof(buf));
    if (status == HAL_OK)
    {
      int bad = 0;
      for (int i = 0; i < 256; i++)
      {
        if (buf[i] != (uint8_t)i)
        {
          bad++;
        }
      }
      if (bad == 0)
      {
        DBG_PRINT("verification successful");
      }
      else
      {
        DBG_PRINT("verification FAILED (%d bad bytes)", bad);
      }
    }
    else
    {
      DBG_PRINT("ERROR: indirect verification read failed: %d", status);
      Error_Handler();
    }
  }
}

/**
  * @brief  Main program
  * @param  None
  * @retval None
  */
int main(void)
{
//  XSPI_RegularCmdTypeDef sCommand = {0};

  /* STM32U5xx HAL library initialization:
  - Configure the Flash prefetch
  - Configure the Systick to generate an interrupt each 1 msec
  - Set NVIC Group Priority to 3
  - Low Level Initialization
  */
  HAL_Init();

  /* Enable the Instruction and Data Cache */
  CACHE_Enable();

  /* Configure the System clock to have a frequency of 160 MHz */
  SystemClock_Config();

  /* Configure the system clock */
  if(OSPIClock_Config() != HAL_OK)
  {
    while(1);
  }

  BSP_LED_Init(LED_GREEN);
  BSP_LED_Init(LED_RED);

  /* Initialization of the OSPI ------------------------------------------ */
  OSPIHandle.Instance = OCTOSPI1;
  HAL_XSPI_DeInit(&OSPIHandle);

  OSPIHandle.Init.FifoThresholdByte         = 4;
  OSPIHandle.Init.MemoryType                = HAL_XSPI_MEMTYPE_MICRON;
  OSPIHandle.Init.MemorySize                = HAL_XSPI_SIZE_512MB;
  OSPIHandle.Init.ChipSelectHighTimeCycle   = 2;
  OSPIHandle.Init.FreeRunningClock          = HAL_XSPI_FREERUNCLK_DISABLE;
  OSPIHandle.Init.ClockMode                 = HAL_XSPI_CLOCK_MODE_0;
  OSPIHandle.Init.WrapSize                  = HAL_XSPI_WRAP_NOT_SUPPORTED;
  OSPIHandle.Init.ClockPrescaler            = 1;
  OSPIHandle.Init.SampleShifting            = HAL_XSPI_SAMPLE_SHIFT_NONE;
  OSPIHandle.Init.DelayHoldQuarterCycle     = HAL_XSPI_DHQC_ENABLE;
  OSPIHandle.Init.ChipSelectBoundary        = HAL_XSPI_BONDARYOF_NONE;

  if (HAL_XSPI_Init(&OSPIHandle) != HAL_OK)
  {
    Error_Handler();
  }

  /* Configure the memory in octal mode ------------------------------------- */
  OSPI_OctalModeCfg(&OSPIHandle);

//  sCommand.InstructionMode    = HAL_XSPI_INSTRUCTION_8_LINES;
//  sCommand.InstructionWidth   = HAL_XSPI_INSTRUCTION_16_BITS;
//  sCommand.InstructionDTRMode = HAL_XSPI_INSTRUCTION_DTR_DISABLE;
//  sCommand.AddressWidth       = HAL_XSPI_ADDRESS_32_BITS;
//  sCommand.AddressDTRMode     = HAL_XSPI_ADDRESS_DTR_DISABLE;
//  sCommand.AlternateBytesMode = HAL_XSPI_ALT_BYTES_NONE;
//  sCommand.DataDTRMode        = HAL_XSPI_DATA_DTR_DISABLE;
//  sCommand.DQSMode            = HAL_XSPI_DQS_DISABLE;
//  sCommand.SIOOMode           = HAL_XSPI_SIOO_INST_EVERY_CMD;

  BSP_LED_Off(LED_GREEN);
  BSP_LED_Off(LED_RED);

#if 1
  const bool UseMemoryMapping = true;
  const bool Erase  = true;
  const bool Read   = !true;
  const bool Write  = true;
  const bool Verify = true;

  HAL_Delay(1000);

  DBG_PRINT("\n===\nTesting external flash ...");

  if (Erase)
  {
    ext_flash_erase_block_addr(0);
  }

  if (UseMemoryMapping)
  {
    ext_flash_test_memory_mapped(Read, Write, Verify);
  }
  else
  {
    ext_flash_test_indirect(Read, Write, Verify);
  }
  DBG_PRINT("===\n");

  while (1) {}
#endif

  // Enable for dumping and/or inspection using an external flash programmer
#if 0
  dump_flash_at_offset(0, 256 + 16);
  while (1)
    HAL_Delay(1000);
#endif

  enum State {
    ERASE = 0,
    READ,
    WRITE,
    VERIFY,
    DONE,
  };
  uint8_t state = READ;
  //uint8_t state = ERASE;

  while (1)
  {
    switch(state)
    {
      case ERASE:
      {
        printf("ERASE\n");
        ext_flash_erase_block_addr(0);

        state = WRITE;
        break;
      }
      case READ:
      case VERIFY:
      {
        printf(state == READ ? "READ\n" : "VERIFY\n");

        XSPI_NOR_EnableMemoryMapped(&OSPIHandle);

        dump_flash_at_addr((void *)OCTOSPI1_BASE, 256 + 16);

        // Verify using memory-mapped access
        int good1 = 1;
        uint8_t *ext_flash = (uint8_t *)OCTOSPI1_BASE;
        for (int i = 0; i < 256; i++)
        {
          if (ext_flash[i] != i)
          {
            good1 = 0;
            break;
          }
        }
        printf("Memory-mapped %s %s\n",
          state == VERIFY ? "verification" : "reading",
          good1 ? "succeeded" : "failed");

        XSPI_NOR_DisableMemoryMapped(&OSPIHandle);

        dump_flash_at_offset(0, 256 + 16);

        // Verify using indirect access
        int good2 = 1;
        if (XSPI_NOR_Read(&OSPIHandle, 0, TempBuffer, sizeof(TempBuffer)) != HAL_OK)
        {
          good2 = 0;
        }
        else
        {
          for (int i = 0; i < 256; i++)
          {
            if (TempBuffer[i] != i)
            {
              good2 = 0;
              break;
            }
          }
        }
        printf("Indirect %s %s\n",
          state == VERIFY ? "verification" : "reading",
          good2 ? "succeeded" : "failed");

        if (state == VERIFY)
        {
          state = DONE;
        }
        else
        {
          //state = (good1 && good2) ? DONE : ERASE;    // ERASE → WRITE on failure
          state = DONE;                   // don't ERASE/WRITE on failure
        }
        break;
      }
      case WRITE:
      {
        printf("WRITE\n");

        /* Enable write operations ---------------------------------------- */
        OSPI_WriteEnable(&OSPIHandle);

        XSPI_NOR_EnableMemoryMapped(&OSPIHandle);

        /* Writing Sequence ----------------------------------------------- */
        uint8_t *ext_flash = (uint8_t *)OCTOSPI1_BASE;
        for (int i = 0; i < 256; i++)
        {
          *ext_flash = (uint8_t)i;
          ext_flash++;
        }

        /* In memory-mapped mode, not possible to check if the memory is ready
        after the programming. So a delay corresponding to max page programming
        time is added */
        HAL_Delay(MEMORY_PAGE_PROG_DELAY);

        dump_flash_at_addr((void *)OCTOSPI1_BASE, 256 + 16);

        XSPI_NOR_DisableMemoryMapped(&OSPIHandle);

        // TODO unsure if this does anything. OSPI_AutoPollingMemReady() crashes before HAL_XSPI_Abort()
        const uint32_t before = HAL_GetTick();
        OSPI_AutoPollingMemReady(&OSPIHandle);
        const uint32_t after = HAL_GetTick();
        printf("OSPI_AutoPollingMemReady() took %lu ms\n", after - before);

        state = VERIFY;
        break;
      }
      case DONE:
      {
        printf("DONE\n");
#if 0 // Run the test in a reboot loop
        HAL_Delay(10000);
        HAL_NVIC_SystemReset();
#endif
        while(1)
          HAL_Delay(1000);
        break;
      }
      default:
        Error_Handler();
    }
  }
}

/**
  * @brief  Command completed callback.
  * @param  hospi: OSPI handle
  * @retval None
  */
void HAL_XSPI_CmdCpltCallback(XSPI_HandleTypeDef *hospi)
{
  Error_Handler();
}

/**
  * @brief  Transfer Error callback.
  * @param  hospi: OSPI handle
  * @retval None
  */
void HAL_XSPI_ErrorCallback(XSPI_HandleTypeDef *hospi)
{
  Error_Handler();
}

/**
  * @brief  System Clock Configuration
  *         The system Clock is configured as follows :
  *            System Clock source            = PLL (MSI)
  *            SYSCLK(Hz)                     = 160000000
  *            HCLK(Hz)                       = 160000000
  *            AHB Prescaler                  = 1
  *            APB1 Prescaler                 = 1
  *            APB2 Prescaler                 = 1
  *            APB3 Prescaler                 = 1
  *            MSI Frequency(Hz)              = 4000000
  *            PLL_MBOOST                     = 1
  *            PLL_M                          = 1
  *            PLL_N                          = 80
  *            PLL_Q                          = 2
  *            PLL_R                          = 2
  *            PLL_P                          = 2
  *            Flash Latency(WS)              = 4
  * @param  None
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};

  /* Enable voltage range 1 for frequency above 100 Mhz */
  __HAL_RCC_PWR_CLK_ENABLE();
  HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1);

  /* Switch to SMPS regulator instead of LDO */
  HAL_PWREx_ConfigSupply(PWR_SMPS_SUPPLY);

  __HAL_RCC_PWR_CLK_DISABLE();

  /* MSI Oscillator enabled at reset (4Mhz), activate PLL with MSI as source */
  RCC_OscInitStruct.OscillatorType      = RCC_OSCILLATORTYPE_MSI | RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.MSIState            = RCC_MSI_ON;
  RCC_OscInitStruct.HSEState            = RCC_HSE_ON;
  RCC_OscInitStruct.MSIClockRange       = RCC_MSIRANGE_4;
  RCC_OscInitStruct.MSICalibrationValue = RCC_MSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState        = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource       = RCC_PLLSOURCE_MSI;
  RCC_OscInitStruct.PLL.PLLMBOOST       = RCC_PLLMBOOST_DIV1;
  RCC_OscInitStruct.PLL.PLLM            = 1;
  RCC_OscInitStruct.PLL.PLLN            = 80;
  RCC_OscInitStruct.PLL.PLLR            = 2;
  RCC_OscInitStruct.PLL.PLLP            = 2;
  RCC_OscInitStruct.PLL.PLLQ            = 2;
  RCC_OscInitStruct.PLL.PLLFRACN        = 0;
  RCC_OscInitStruct.PLL.PLLRGE          = RCC_PLLVCIRANGE_0;

  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    /* Initialization Error */
    Error_Handler();
  }

  /* Select PLL as system clock source and configure bus clocks dividers */
  RCC_ClkInitStruct.ClockType = (RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_PCLK1 | \
                                 RCC_CLOCKTYPE_PCLK2  | RCC_CLOCKTYPE_PCLK3);
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB3CLKDivider = RCC_HCLK_DIV1;
  if(HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
  {
    /* Initialization Error */
    Error_Handler();
  }
}

/**
  * @brief  Periph Clock Configuration
  *         The periph Clock is configured as follows :
  *         PLL2_VCO Input = HSE_VALUE/PLL2M = 4 Mhz
  *         PLL2_VCO Output = PLL2_VCO Input * PLL2N = 180 Mhz
  *         PLLOSPICLK = PLL2_VCO Output/PLL2R = 180 Mhz
  * ---->   OSPI presacler = 1 so OSPI frequency = 90 Mhz
  * @param  None
  * @retval None
  */
HAL_StatusTypeDef OSPIClock_Config(void)
{
  RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};
  RCC_PLL2InitTypeDef RCC_PLL2InitTypeDef = {0};

  RCC_PLL2InitTypeDef.PLL2ClockOut = RCC_PLL2_DIVQ;
  RCC_PLL2InitTypeDef.PLL2Source = RCC_PLLSOURCE_HSE;

  RCC_PLL2InitTypeDef.PLL2M = 4;
  RCC_PLL2InitTypeDef.PLL2N = 45;
  RCC_PLL2InitTypeDef.PLL2P = 1;
  RCC_PLL2InitTypeDef.PLL2Q = 1;
  RCC_PLL2InitTypeDef.PLL2R = 1;
  RCC_PLL2InitTypeDef.PLL2RGE = RCC_PLLVCIRANGE_0;
  RCC_PLL2InitTypeDef.PLL2FRACN = 0;
  PeriphClkInitStruct.PLL2= RCC_PLL2InitTypeDef;

  /* Activate PLL with HSE as source */
  PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_OSPI;
  PeriphClkInitStruct.OspiClockSelection = RCC_OSPICLKSOURCE_PLL2;

  return HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct);

}

/**
  * @brief  This function send a Write Enable and wait it is effective.
  * @param  hospi: OSPI handle
  * @retval None
  */
static void OSPI_WriteEnable(XSPI_HandleTypeDef *hospi)
{
  XSPI_RegularCmdTypeDef  sCommand = {0};
  XSPI_AutoPollingTypeDef sConfig = {0};

  /* Enable write operations ------------------------------------------ */
  sCommand.OperationType      = HAL_XSPI_OPTYPE_COMMON_CFG;
  sCommand.Instruction        = OCTAL_WRITE_ENABLE_CMD;
  sCommand.InstructionMode    = HAL_XSPI_INSTRUCTION_8_LINES;
  sCommand.InstructionWidth   = HAL_XSPI_INSTRUCTION_16_BITS;
  sCommand.InstructionDTRMode = HAL_XSPI_INSTRUCTION_DTR_DISABLE;
  sCommand.AddressMode        = HAL_XSPI_ADDRESS_NONE;
  sCommand.AlternateBytesMode = HAL_XSPI_BONDARYOF_NONE;
  sCommand.DataMode           = HAL_XSPI_DATA_NONE;
  sCommand.DummyCycles        = 0;
  sCommand.DQSMode            = HAL_XSPI_DQS_DISABLE;
  sCommand.SIOOMode           = HAL_XSPI_SIOO_INST_EVERY_CMD;

  if (HAL_XSPI_Command(hospi, &sCommand, HAL_XSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
  {
    Error_Handler();
  }

  /* Configure automatic polling mode to wait for write enabling ---- */
  sCommand.Instruction    = OCTAL_READ_STATUS_REG_CMD;
  sCommand.Address        = 0x0;
  sCommand.AddressMode    = HAL_XSPI_ADDRESS_8_LINES;
  sCommand.AddressWidth   = HAL_XSPI_ADDRESS_32_BITS;
  sCommand.AddressDTRMode = HAL_XSPI_ADDRESS_DTR_DISABLE;
  sCommand.DataMode       = HAL_XSPI_DATA_8_LINES;
  sCommand.DataDTRMode    = HAL_XSPI_DATA_DTR_DISABLE;
  sCommand.DataLength     = 1;
  sCommand.DummyCycles    = DUMMY_CLOCK_CYCLES_READ_REG;

    if (HAL_XSPI_Command(hospi, &sCommand, HAL_XSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    {
      Error_Handler();
    }

  sConfig.MatchValue      = WRITE_ENABLE_MATCH_VALUE;
  sConfig.MatchMask       = WRITE_ENABLE_MASK_VALUE;
  sConfig.MatchMode       = HAL_XSPI_MATCH_MODE_AND;
  sConfig.IntervalTime    = AUTO_POLLING_INTERVAL;
  sConfig.AutomaticStop   = HAL_XSPI_AUTOMATIC_STOP_ENABLE;

  if (HAL_XSPI_AutoPolling(hospi, &sConfig, HAL_XSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    {
      Error_Handler();
    }
}

/**
  * @brief  This function read the SR of the memory and wait the EOP.
  * @param  hospi: OSPI handle
  * @retval None
  */
static void OSPI_AutoPollingMemReady(XSPI_HandleTypeDef *hospi)
{
  XSPI_RegularCmdTypeDef  sCommand = {0};
  XSPI_AutoPollingTypeDef sConfig = {0};

  /* Configure automatic polling mode to wait for memory ready ------ */
  sCommand.OperationType      = HAL_XSPI_OPTYPE_COMMON_CFG;
  sCommand.Instruction        = OCTAL_READ_STATUS_REG_CMD;
  sCommand.InstructionMode    = HAL_XSPI_INSTRUCTION_8_LINES;
  sCommand.InstructionWidth   = HAL_XSPI_INSTRUCTION_16_BITS;
  sCommand.InstructionDTRMode = HAL_XSPI_INSTRUCTION_DTR_DISABLE;
  sCommand.Address            = 0x0;
  sCommand.AddressMode        = HAL_XSPI_ADDRESS_8_LINES;
  sCommand.AddressWidth       = HAL_XSPI_ADDRESS_32_BITS;
  sCommand.AddressDTRMode     = HAL_XSPI_ADDRESS_DTR_DISABLE;
  sCommand.AlternateBytesMode = HAL_XSPI_ALT_BYTES_NONE;
  sCommand.DataMode           = HAL_XSPI_DATA_8_LINES;
  sCommand.DataDTRMode        = HAL_XSPI_DATA_DTR_DISABLE;
  sCommand.DataLength         = 1;
  sCommand.DummyCycles        = DUMMY_CLOCK_CYCLES_READ_REG;
  sCommand.DQSMode            = HAL_XSPI_DQS_DISABLE;
  sCommand.SIOOMode           = HAL_XSPI_SIOO_INST_EVERY_CMD;

  if (HAL_XSPI_Command(hospi, &sCommand, HAL_XSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
  {
    Error_Handler();
  }

  sConfig.MatchValue      = MEMORY_READY_MATCH_VALUE;
  sConfig.MatchMask       = MEMORY_READY_MASK_VALUE;
  sConfig.MatchMode       = HAL_XSPI_MATCH_MODE_AND;
  sConfig.IntervalTime    = AUTO_POLLING_INTERVAL;
  sConfig.AutomaticStop   = HAL_XSPI_AUTOMATIC_STOP_ENABLE;

  if (HAL_XSPI_AutoPolling(hospi, &sConfig, HAL_XSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief  This function configure the memory in Octal mode.
  * @param  hospi: OSPI handle
  * @retval None
  */
static void OSPI_OctalModeCfg(XSPI_HandleTypeDef *hospi)
{
  XSPI_RegularCmdTypeDef  sCommand = {0};
  XSPI_AutoPollingTypeDef sConfig = {0};
  uint8_t reg;

  /* Enable write operations ---------------------------------------- */
  sCommand.OperationType      = HAL_XSPI_OPTYPE_COMMON_CFG;
  sCommand.Instruction        = WRITE_ENABLE_CMD;
  sCommand.InstructionMode    = HAL_XSPI_INSTRUCTION_1_LINE;
  sCommand.InstructionWidth   = HAL_XSPI_INSTRUCTION_8_BITS;
  sCommand.InstructionDTRMode = HAL_XSPI_INSTRUCTION_DTR_DISABLE;
  sCommand.AddressMode        = HAL_XSPI_ADDRESS_NONE;
  sCommand.AlternateBytesMode = HAL_XSPI_ALT_BYTES_NONE;
  sCommand.DataMode           = HAL_XSPI_DATA_NONE;
  sCommand.DummyCycles        = 0;
  sCommand.DQSMode            = HAL_XSPI_DQS_DISABLE;
  sCommand.SIOOMode           = HAL_XSPI_SIOO_INST_EVERY_CMD;

  if (HAL_XSPI_Command(hospi, &sCommand, HAL_XSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
  {
    Error_Handler();
  }

  /* Configure automatic polling mode to wait for write enabling ---- */
  sCommand.Instruction    = READ_STATUS_REG_CMD;
  sCommand.DataMode       = HAL_XSPI_DATA_1_LINE;
  sCommand.DataDTRMode    = HAL_XSPI_DATA_DTR_DISABLE;
  sCommand.DataLength     = 1;

  if (HAL_XSPI_Command(hospi, &sCommand, HAL_XSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
  {
    Error_Handler();
  }

  sConfig.MatchValue      = WRITE_ENABLE_MATCH_VALUE;
  sConfig.MatchMask       = WRITE_ENABLE_MASK_VALUE;
  sConfig.MatchMode       = HAL_XSPI_MATCH_MODE_AND;
  sConfig.IntervalTime    = AUTO_POLLING_INTERVAL;
  sConfig.AutomaticStop   = HAL_XSPI_AUTOMATIC_STOP_ENABLE;

  if (HAL_XSPI_AutoPolling(hospi, &sConfig, HAL_XSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
  {
    Error_Handler();
  }

  /* Write Configuration register 2 (with new dummy cycles) --------- */
  sCommand.Instruction    = WRITE_CFG_REG_2_CMD;
  sCommand.Address        = CONFIG_REG2_ADDR3;
  sCommand.AddressMode    = HAL_XSPI_ADDRESS_1_LINE;
  sCommand.AddressWidth   = HAL_XSPI_ADDRESS_32_BITS;
  sCommand.AddressDTRMode = HAL_XSPI_ADDRESS_DTR_DISABLE;

  if (HAL_XSPI_Command(hospi, &sCommand, HAL_XSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
  {
    Error_Handler();
  }

  reg = CR2_DUMMY_CYCLES_66MHZ;

  if (HAL_XSPI_Transmit(hospi, &reg, HAL_XSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
  {
    Error_Handler();
  }

  /* Wait that the memory is ready ---------------------------------- */
  sCommand.Instruction = READ_STATUS_REG_CMD;
  sCommand.AddressMode = HAL_XSPI_ADDRESS_NONE;

  if (HAL_XSPI_Command(hospi, &sCommand, HAL_XSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
  {
    Error_Handler();
  }

  sConfig.MatchValue = MEMORY_READY_MATCH_VALUE;
  sConfig.MatchMask  = MEMORY_READY_MASK_VALUE;

  if (HAL_XSPI_AutoPolling(hospi, &sConfig, HAL_XSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
  {
    Error_Handler();
  }

  /* Enable write operations ---------------------------------------- */
  sCommand.Instruction = WRITE_ENABLE_CMD;
  sCommand.DataMode    = HAL_XSPI_DATA_NONE;

  if (HAL_XSPI_Command(hospi, &sCommand, HAL_XSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
  {
    Error_Handler();
  }

  /* Configure automatic polling mode to wait for write enabling ---- */
  sCommand.Instruction = READ_STATUS_REG_CMD;
  sCommand.DataMode    = HAL_XSPI_DATA_1_LINE;

  if (HAL_XSPI_Command(hospi, &sCommand, HAL_XSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
  {
    Error_Handler();
  }

  sConfig.MatchValue = WRITE_ENABLE_MATCH_VALUE;
  sConfig.MatchMask  = WRITE_ENABLE_MASK_VALUE;

  if (HAL_XSPI_AutoPolling(hospi, &sConfig, HAL_XSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
  {
    Error_Handler();
  }

  /* Write Configuration register 2 (with octal mode) --------------- */
  sCommand.Instruction = WRITE_CFG_REG_2_CMD;
  sCommand.Address     = CONFIG_REG2_ADDR1;
  sCommand.AddressMode = HAL_XSPI_ADDRESS_1_LINE;

  if (HAL_XSPI_Command(hospi, &sCommand, HAL_XSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
  {
    Error_Handler();
  }

  reg = CR2_STR_OPI_ENABLE;

  if (HAL_XSPI_Transmit(hospi, &reg, HAL_XSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
  {
    Error_Handler();
  }

  /* Wait that the configuration is effective and check that memory is ready */
  HAL_Delay(MEMORY_REG_WRITE_DELAY);

  /* Wait that the memory is ready ---------------------------------- */
  OSPI_AutoPollingMemReady(hospi);
}

/**
  * @brief  This function is executed in case of error occurrence.
  * @param  None
  * @retval None
  */
static void Error_Handler(void)
{
  BSP_LED_Off(LED_GREEN);
  while(1)
  {
    HAL_Delay(100);
    BSP_LED_Toggle(LED_RED);
  }
}
/**
  * @brief  Enable ICACHE with 1-way set-associative configuration.
  * @param  None
  * @retval None
  */
static void CACHE_Enable(void)
{
  /* Configure ICACHE associativity mode */
  HAL_ICACHE_ConfigAssociativityMode(ICACHE_1WAY);

  /* Enable ICACHE */
  HAL_ICACHE_Enable();
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */

  /* Infinite loop */
  while (1)
  {
  }
}
#endif

/**
  * @}
  */

/**
  * @}
  */
