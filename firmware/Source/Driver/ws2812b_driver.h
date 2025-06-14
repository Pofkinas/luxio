#ifndef SOURCE_DRIVER_WS2812B_DRIVER_H_
#define SOURCE_DRIVER_WS2812B_DRIVER_H_
/**********************************************************************************************************************
 * Includes
 *********************************************************************************************************************/

#include "stdbool.h"
#include "stdint.h"
#include "stddef.h"

/**********************************************************************************************************************
 * Exported definitions and macros
 *********************************************************************************************************************/

#define LED_DATA_CHANNELS 3 
#define WS2812B_1_LED_COUNT 86
#define WS2812B_2_LED_COUNT 86

/**********************************************************************************************************************
 * Exported types
 *********************************************************************************************************************/

/* clang-format off */
typedef enum eWs2812bDriver {
    eWs2812bDriver_First = 0,
    eWs2812bDriver_1 = eWs2812bDriver_First,
    eWs2812bDriver_2,
    eWs2812bDriver_Last
} eWs2812bDriver_t;

typedef enum eLedTransferState {
    eLedTransferState_First = 0,
    eLedTransferState_Start = eLedTransferState_First,
    eLedTransferState_Complete,
    eLedTransferState_TransferError,
    eLedTransferState_Last
} eLedTransferState_t;
/* clang-format on */

typedef void (*led_driver_callback_t) (void *context, const eLedTransferState_t transfer_state);

/**********************************************************************************************************************
 * Exported variables
 *********************************************************************************************************************/

/**********************************************************************************************************************
 * Prototypes of exported functions
 *********************************************************************************************************************/

bool WS2812B_Driver_Init (const eWs2812bDriver_t device, led_driver_callback_t callback, void *callback_context);
bool WS2812B_Driver_Set (const eWs2812bDriver_t device, uint8_t *led_data, size_t led_count);
bool WS2812B_Driver_Reset (const eWs2812bDriver_t device);
uint16_t WS2812B_Driver_GetMinRefreshRate (const eWs2812bDriver_t device);

#endif /* SOURCE_DRIVER_WS2812B_DRIVER_H_ */
