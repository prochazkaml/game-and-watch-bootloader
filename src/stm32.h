#include "main.h"

void SystemClock_Config();
void MX_GPIO_Init();
void MX_DMA_Init();
void MX_LTDC_Init();
void MX_SPI2_Init();
void MX_OCTOSPI1_Init();
void MX_SAI1_Init();
void MX_NVIC_Init();

extern LTDC_HandleTypeDef hltdc;
extern OSPI_HandleTypeDef hospi1;
extern SAI_HandleTypeDef hsai_BlockA1;
extern DMA_HandleTypeDef hdma_sai1_a;
extern SPI_HandleTypeDef hspi2;
