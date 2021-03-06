#include "stm32h7xx_hal.h"

typedef struct {
	uint16_t Magic;
	uint32_t Brightness : 3;
} SystemConfig;

extern SystemConfig *syscfg;

#define CFG_REG 0

#define CFG_BACKLIGHT 0x01

extern uint8_t data_buffer[512 * 1024] __attribute__((section (".databuf")));

extern LTDC_HandleTypeDef hltdc;
extern OSPI_HandleTypeDef hospi1;
extern SAI_HandleTypeDef hsai_BlockA1;
extern DMA_HandleTypeDef hdma_sai1_a;
extern SPI_HandleTypeDef hspi2;
extern DAC_HandleTypeDef hdac1;
extern DAC_HandleTypeDef hdac2;
extern RTC_HandleTypeDef hrtc;

void SystemClock_Config();
void MX_GPIO_Init();
void MX_DMA_Init();
void MX_LTDC_Init();
void MX_SPI2_Init();
void MX_OCTOSPI1_Init();
void MX_SAI1_Init();
void MX_NVIC_Init();
void MX_DAC1_Init();
void MX_DAC2_Init();
void MX_RTC_Init();
void GW_Sleep();
void Error_Handler();

void config_init();
void config_update();

void snprinttime(char *buffer, int size);
void rtc_settimedate(uint8_t h, uint8_t m, uint8_t s, uint8_t D, uint8_t M, uint16_t Y);

uint32_t rtc_readreg(uint8_t reg);
void rtc_writereg(uint8_t reg, uint32_t data);

#define GPIO_Speaker_enable_Pin GPIO_PIN_3
#define GPIO_Speaker_enable_GPIO_Port GPIOE
#define BTN_PAUSE_Pin GPIO_PIN_13
#define BTN_PAUSE_GPIO_Port GPIOC
#define BTN_GAME_Pin GPIO_PIN_1
#define BTN_GAME_GPIO_Port GPIOC
#define BTN_TIME_Pin GPIO_PIN_5
#define BTN_TIME_GPIO_Port GPIOC
#define BTN_A_Pin GPIO_PIN_9
#define BTN_A_GPIO_Port GPIOD
#define BTN_Left_Pin GPIO_PIN_11
#define BTN_Left_GPIO_Port GPIOD
#define BTN_Down_Pin GPIO_PIN_14
#define BTN_Down_GPIO_Port GPIOD
#define BTN_Right_Pin GPIO_PIN_15
#define BTN_Right_GPIO_Port GPIOD
#define BTN_Up_Pin GPIO_PIN_0
#define BTN_Up_GPIO_Port GPIOD
#define BTN_B_Pin GPIO_PIN_5
#define BTN_B_GPIO_Port GPIOD
#define BTN_Power_Pin GPIO_PIN_0
#define BTN_Power_GPIO_Port GPIOA
