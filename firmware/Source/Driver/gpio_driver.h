#ifndef SOURCE_DRIVER_GPIO_DRIVER_H_
#define SOURCE_DRIVER_GPIO_DRIVER_H_
/**********************************************************************************************************************
 * Includes
 *********************************************************************************************************************/

#include <stdbool.h>
#include <stdint.h>

/**********************************************************************************************************************
 * Exported definitions and macros
 *********************************************************************************************************************/

/**********************************************************************************************************************
 * Exported types
 *********************************************************************************************************************/

/* clang-format off */
typedef enum eGpioPin {
    eGpioPin_First = 0, 
    eGpioPin_StartButton = eGpioPin_First,
    eGpioPin_DebugTx,
    eGpioPin_DebugRx,
    eGpioPin_I2c1_SCL,
    eGpioPin_I2c1_SDA,
    eGpioPin_vl53l0_Xshut_1,
    eGpioPin_vl53l0_Xshut_2,
    eGpioPin_Ws2812B_1,
    eGpioPin_Ws2812B_2,
    eGpioPin_Last
} eGpioPin_t;
/* clang-format on */

/**********************************************************************************************************************
 * Exported variables
 *********************************************************************************************************************/

/**********************************************************************************************************************
 * Prototypes of exported functions
 *********************************************************************************************************************/

bool GPIO_Driver_InitAllPins (void);
bool GPIO_Driver_WritePin (const eGpioPin_t gpio_pin, const bool pin_state);
bool GPIO_Driver_ReadPin (const eGpioPin_t gpio_pin, bool *pin_state);
bool GPIO_Driver_TogglePin (const eGpioPin_t gpio_pin);
bool GPIO_Driver_ResetPin (const eGpioPin_t gpio_pin);

#endif /* SOURCE_DRIVER_GPIO_DRIVER_H_ */
