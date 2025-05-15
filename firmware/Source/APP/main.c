/**********************************************************************************************************************
 * Includes
 *********************************************************************************************************************/

#include "main.h"
#include "system_config.h"
#include "cmsis_os.h"
#include "FreeRTOSConfig.h"
#include "stm32f4xx_ll_rcc.h"
#include "stm32f4xx_ll_system.h"
#include "stm32f4xx_ll_utils.h"
#include "stm32f4xx_ll_pwr.h"
#include "stm32f4xx_ll_usart.h"
#include "usart.h"
#include "cli_app.h"
#include "reaction_test_app.h"
#include "debug_api.h"
#include "timer_driver.h"

#include "vl53l0xv2_api.h"

/**********************************************************************************************************************
 * Private definitions and macros
 *********************************************************************************************************************/

#define DEBUG_MAIN

/**********************************************************************************************************************
 * Private typedef
 *********************************************************************************************************************/

/**********************************************************************************************************************
 * Private constants
 *********************************************************************************************************************/

#ifdef DEBUG_MAIN
CREATE_MODULE_NAME (MAIN)
#else
CREATE_MODULE_NAME_EMPTY
#endif

const static osThreadAttr_t g_main_thread_attributes = {
    .name = "Main_Test_Thread",
    .stack_size = 128 * 16,
    .priority = (osPriority_t) osPriorityNormal
};

/**********************************************************************************************************************
 * Private variables
 *********************************************************************************************************************/

static osThreadId_t g_test_thread_id = NULL;
static bool g_is_test_thread_init = false;

static bool g_is_vl53l0x_init = false;

/**********************************************************************************************************************
 * Exported variables and references
 *********************************************************************************************************************/

/**********************************************************************************************************************
 * Prototypes of private functions
 *********************************************************************************************************************/

static void SystemClock_Config (void);

volatile unsigned long ulHighFrequencyTimerTicks;

void configureTimerForRunTimeStats (void);
void TIM1_UP_TIM10_IRQnHandler (void);

unsigned long getRunTimeCounterValue (void);

static void Main_Test_Thread (void* arg);

/**********************************************************************************************************************
 * Definitions of private functions
 *********************************************************************************************************************/

static void SystemClock_Config (void) {
    LL_FLASH_SetLatency(LL_FLASH_LATENCY_3);
    while(LL_FLASH_GetLatency()!= LL_FLASH_LATENCY_3) {}
    LL_PWR_SetRegulVoltageScaling(LL_PWR_REGU_VOLTAGE_SCALE1);
    LL_RCC_HSI_SetCalibTrimming(16);
    LL_RCC_HSI_Enable();

    while(LL_RCC_HSI_IsReady() != 1) {}
    LL_RCC_PLL_ConfigDomain_SYS(LL_RCC_PLLSOURCE_HSI, LL_RCC_PLLM_DIV_8, 100, LL_RCC_PLLP_DIV_2);
    LL_RCC_PLL_Enable();

    while(LL_RCC_PLL_IsReady() != 1) {}
    while (LL_PWR_IsActiveFlag_VOS() == 0) {}
    LL_RCC_SetAHBPrescaler(LL_RCC_SYSCLK_DIV_1);
    LL_RCC_SetAPB1Prescaler(LL_RCC_APB1_DIV_2);
    LL_RCC_SetAPB2Prescaler(LL_RCC_APB2_DIV_1);
    LL_RCC_SetSysClkSource(LL_RCC_SYS_CLKSOURCE_PLL);

    while(LL_RCC_GetSysClkSource() != LL_RCC_SYS_CLKSOURCE_STATUS_PLL) {}
    LL_SetSystemCoreClock(SYSTEM_CLOCK);

    if (HAL_InitTick (TICK_INT_PRIORITY) != HAL_OK) {
        __disable_irq();
        while (1) {}
    }
}

void configureTimerForRunTimeStats (void) {
    ulHighFrequencyTimerTicks = 0;
    LL_TIM_EnableIT_UPDATE(TIM10);
    LL_TIM_EnableCounter(TIM10);
}

unsigned long getRunTimeCounterValue (void) {
    return ulHighFrequencyTimerTicks;
}

void TIM1_UP_TIM10_IRQHandler (void) {
    if (LL_TIM_IsActiveFlag_UPDATE(TIM10)) {
        ulHighFrequencyTimerTicks++;
        LL_TIM_ClearFlag_UPDATE(TIM10);
    }
}

/**********************************************************************************************************************
 * Definitions of exported functions
 *********************************************************************************************************************/

int main (void) {
    LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_SYSCFG);
    LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_PWR);

    NVIC_SetPriorityGrouping(NVIC_PRIORITYGROUP_4);

    NVIC_SetPriority(PendSV_IRQn, NVIC_EncodePriority(NVIC_GetPriorityGrouping(), 15, 0));
    NVIC_SetPriority(SysTick_IRQn, NVIC_EncodePriority(NVIC_GetPriorityGrouping(), 15, 0));

    SystemClock_Config();

    osKernelInitialize();

    // Init TIM10 for debbuging stack size
    Timer_Driver_InitAllTimers();
    Timer_Driver_Start(eTimerDriver_TIM10);

    CLI_APP_Init(eUartBaudrate_115200);
    Reaction_Test_App_Init();
    
    // Init test thread
    //g_test_thread_id = osThreadNew(Main_Test_Thread, NULL, &g_main_thread_attributes);

    TRACE_INFO("Start OK\n");

    osKernelStart();

    while (1) {}
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM2) {
        HAL_IncTick();
    }
}

/**********************************************************************************************************************
 * Testing Thread
 *********************************************************************************************************************/

static void Main_Test_Thread(void* arg) {
    if (!g_is_test_thread_init) {
        // for (uint8_t addr = 1; addr < 255; addr++) {
        //         if (I2C_API_Write(eI2c_1, addr, NULL, 0, 0, 0, 1000)) {
        //             TRACE_INFO("Found I2C device at 0x%02X\n", addr);
        //         }

        //         osDelay(1);
        // }


        VL53L0X_API_Init(eVl53l0x_1);
        VL53L0X_API_Enable(eVl53l0x_1);

        g_is_test_thread_init = true;
    }

    uint16_t distance = 0;

    while (1) {
        if (VL53L0X_API_GetDistance(eVl53l0x_1, &distance, 1000)) {
            TRACE_INFO("Distance: %d mm\n", distance);
        }

        osThreadYield();
    }
}
 
