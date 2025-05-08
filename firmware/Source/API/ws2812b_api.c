/**********************************************************************************************************************
 * Includes
 *********************************************************************************************************************/

#include "ws2812b_api.h"
#include "cmsis_os2.h"
#include "heap_api.h"
#include "ws2812b_driver.h"
#include "timer_driver.h"
#include "pwm_driver.h"
#include "gpio_driver.h"

#include "animation_off.h"
#include "animation_solidcolor.h"

/**********************************************************************************************************************
 * Private definitions and macros
 *********************************************************************************************************************/

#define MUTEX_TIMEOUT 0U
#define REFRESH_RATE 33U // 30 FPS

#define CALLBACK_TIMEOUT 20U
#define CALLBACK_FLAG 0x01U

/**********************************************************************************************************************
 * Private typedef
 *********************************************************************************************************************/
 
typedef struct sWs2812bControlDesc {
    eWs2812bDriver_t device;
    size_t led_count;
    osTimerAttr_t timer_attributes;
    osMutexAttr_t mutex_attributes;
    osEventFlagsAttr_t flag_attributes;
} sWs2812bApiDesc_t;

typedef struct sWs2812bDynamicDesc {
    uint8_t *led_data;
    osTimerId_t timer;
    osMutexId_t mutex;
    osEventFlagsId_t flag; 
    bool is_busy; 
    void (*timer_callback) (void *arg);
    eLedTransferState_t status;
} sWs2812bApiDynamicDesc_t;

typedef struct sWs2812bTimerArg {
    eWs2812b_t device;
    eLedAnimation_t animation;
} sWs2812bTimerArg_t;

/**********************************************************************************************************************
 * Private constants
 *********************************************************************************************************************/

/* clang-format off */ 
const static sWs2812bApiDesc_t g_ws2812b_api_static_lut[eWs2812b_Last] = {
    [eWs2812b_1] = {
        .device = eWs2812bDriver_1,
        .led_count = WS2812B_1_LED_COUNT,
        .timer_attributes = {.name = "WS2812B_API_1_Timer", .attr_bits = 0, .cb_mem = NULL, .cb_size = 0U},
        .mutex_attributes = {.name = "WS2812B_API_1_Mutex", .attr_bits = osMutexRecursive | osMutexPrioInherit, .cb_mem = NULL, .cb_size = 0U},
        .flag_attributes = {.name = "WS2812B_API_1_EventFlag", .attr_bits = 0, .cb_mem = NULL, .cb_size = 0U}
    }
};
/* clang-format on */ 

/**********************************************************************************************************************
 * Private variables
 *********************************************************************************************************************/
 
static bool g_ws2812b_api_is_init = false;

/* clang-format off */
static sWs2812bApiDynamicDesc_t g_ws2812b_api_dynamic_lut[eWs2812b_Last] = {
    [eWs2812b_1] = {
        .led_data = NULL,
        .timer = NULL,
        .mutex = NULL,
        .flag = NULL,
        .is_busy = false,
        .timer_callback = NULL,
        .status = eLedTransferState_Start
    }
};

static sWs2812bTimerArg_t g_ws2812b_api_timer_arg[eWs2812b_Last] = {
    [eWs2812b_1] = {
        .device = eWs2812b_1,
        .animation = eLedAnimation_Off
    }
};
/* clang-format on */

/**********************************************************************************************************************
 * Exported variables and references
 *********************************************************************************************************************/

/**********************************************************************************************************************
 * Prototypes of private functions
 *********************************************************************************************************************/
 
static void WS2812B_API_TimerCallback (void *arg);
static void WS2812B_API_DriverCallback (const eWs2812bDriver_t device, const eLedTransferState_t transfer_state);
static bool WS2812B_API_ExecuteStaticAnimation (sLedAnimationDesc_t *static_animation_data);
static sLedColorRgb_t *WS2812B_API_CastToRgb (const eColorFormat_t color_format, void *color_data);

/**********************************************************************************************************************
 * Definitions of private functions
 *********************************************************************************************************************/

static void WS2812B_API_TimerCallback (void *arg) {
    if (arg == NULL) {
        return;
    }

    sWs2812bTimerArg_t *timer_arg = (sWs2812bTimerArg_t *) arg;

    if ((timer_arg->device < eWs2812b_First) || (timer_arg->device >= eWs2812b_Last)) {
        return;
    }

    if ((timer_arg->animation < eLedAnimation_First) || (timer_arg->animation >= eLedAnimation_Last)) {
        return;
    }

    // Check flag from animation, if it is set, then stop the timer, return

    // if (/*flag is set*/) {
    //     if (osMutexAcquire(g_ws2812b_api_dynamic_lut[timer_arg->device].mutex, MUTEX_TIMEOUT) != osOK) {
    //         return;
    //     }
    
    //     g_ws2812b_api_dynamic_lut[timer_arg->device].is_busy = false;
    
    //     osMutexRelease(g_ws2812b_api_dynamic_lut[timer_arg->device].mutex);

    //     // Set flag that animation is stopped?
    // }

    switch (timer_arg->animation) {
        case eLedAnimation_Blink: {

        } return;
        case eLedAnimation_Rainbow: {

        } return;
        default: {
            return;
        }
    }
}

static void WS2812B_API_DriverCallback (const eWs2812bDriver_t callback_device, const eLedTransferState_t transfer_state) {
    for (eWs2812b_t device = eWs2812b_First; device < eWs2812b_Last; device++) {
        if (g_ws2812b_api_static_lut[device].device == callback_device) {
            g_ws2812b_api_dynamic_lut[device].status = transfer_state;
            
            osEventFlagsSet(g_ws2812b_api_dynamic_lut[device].flag, CALLBACK_FLAG);
            
            return;
        }
    }

    return;
}

static bool WS2812B_API_ExecuteStaticAnimation (sLedAnimationDesc_t *static_animation_data) {
    if (static_animation_data == NULL) {
        return false;
    }
    
    if ((static_animation_data->device < eWs2812b_First) || (static_animation_data->device >= eWs2812b_Last)) {
        return false;
    }

    if ((static_animation_data->animation < eLedAnimation_First) || (static_animation_data->animation >= eLedAnimation_Last)) {
        return false;
    }

    if (static_animation_data->animation != eLedAnimation_Off) {
        if (static_animation_data->data == NULL) {
            return false;
        }
    }

    bool is_execute_successful = true;

    switch (static_animation_data->animation) {
        case eLedAnimation_Off: {
            if (!Animation_Off()) {
                is_execute_successful = false;
            }

            if (!WS2812B_Driver_Reset(static_animation_data->device)) {
                is_execute_successful = false;
            }
        } break;
        case eLedAnimation_SolidColor: {
            sLedAnimationSolidColor_t *data = static_animation_data->data;
            sLedColorRgb_t *led_color = WS2812B_API_CastToRgb(data->color_format, &data->led_color);

            sSolidColorData_t solid_color_data = {
                .device_context = &static_animation_data->device,
                .fill_color_callback = WS2812B_API_FillColor,
                .brightness = static_animation_data->brightness,
                .led_color = led_color->color
            };
 
            if (!Animation_SolidColor_PepareBuffer(&solid_color_data)) {
                is_execute_successful = false;
            }
            
            if (!WS2812B_Driver_Set(static_animation_data->device, g_ws2812b_api_dynamic_lut[static_animation_data->device].led_data, g_ws2812b_api_static_lut[static_animation_data->device].led_count)) {
                is_execute_successful = false;
            }
        } break;
        case eLedAnimation_SegmentFill: {
            
        } break;
        default: {
            return false;
        } break;
    }

    if (osEventFlagsWait(g_ws2812b_api_dynamic_lut[static_animation_data->device].flag, CALLBACK_FLAG, osFlagsWaitAny, CALLBACK_TIMEOUT) != CALLBACK_FLAG) {
        is_execute_successful = false;
    }

    if (osMutexAcquire(g_ws2812b_api_dynamic_lut[static_animation_data->device].mutex, MUTEX_TIMEOUT) != osOK) {
        return false;
    }

    g_ws2812b_api_dynamic_lut[static_animation_data->device].is_busy = false;

    osMutexRelease(g_ws2812b_api_dynamic_lut[static_animation_data->device].mutex);

    if (g_ws2812b_api_dynamic_lut[static_animation_data->device].status != eLedTransferState_Complete) {
        is_execute_successful = false;
    }

    return is_execute_successful;
}

static sLedColorRgb_t *WS2812B_API_CastToRgb (const eColorFormat_t color_format, void *color_data) {
    if (color_data == NULL) {
        return NULL;
    }

    switch (color_format) {
        case eColorFormat_RGB: {
        } break;
        case eColorFormat_HSV: {
            LED_HsvToRgb((sLedColorHsv_t*) color_data);
        } break;
        default: {
        } break;
    }

    return (sLedColorRgb_t*) color_data;
}

/**********************************************************************************************************************
 * Definitions of exported functions
 *********************************************************************************************************************/

bool WS2812B_API_Init (void) {
    if (g_ws2812b_api_is_init) {
        return true;
    }

    if (!GPIO_Driver_InitAllPins()) {
        return false;
    }

    if (!Timer_Driver_InitAllTimers()) {
        return false;
    }

    if (!PWM_Driver_InitAllDevices()) {
        return false;
    }

    g_ws2812b_api_is_init = true;

    for (eWs2812b_t device = eWs2812b_First; device < eWs2812b_Last; device++) {
        if (!WS2812B_Driver_Init(g_ws2812b_api_static_lut[device].device, &WS2812B_API_DriverCallback)) {

            g_ws2812b_api_is_init = false;
        }

        g_ws2812b_api_dynamic_lut[device].led_data = Heap_API_Calloc(g_ws2812b_api_static_lut[device].led_count * LED_DATA_CHANNELS, sizeof(uint8_t));

        if (g_ws2812b_api_dynamic_lut[device].led_data == NULL) {
            g_ws2812b_api_is_init = false;
        }

        if (g_ws2812b_api_dynamic_lut[device].timer == NULL) {
            g_ws2812b_api_dynamic_lut[device].timer = osTimerNew(WS2812B_API_TimerCallback, osTimerPeriodic, &g_ws2812b_api_timer_arg[device], &g_ws2812b_api_static_lut[device].timer_attributes);
        }

        if (g_ws2812b_api_dynamic_lut[device].mutex == NULL) {
            g_ws2812b_api_dynamic_lut[device].mutex = osMutexNew(&g_ws2812b_api_static_lut[device].mutex_attributes);
        }

        if (g_ws2812b_api_dynamic_lut[device].flag == NULL) {
            g_ws2812b_api_dynamic_lut[device].flag = osEventFlagsNew(&g_ws2812b_api_static_lut[device].flag_attributes);
        }
    }

    return g_ws2812b_api_is_init;
}

bool WS2812B_API_Execute (sLedAnimationDesc_t *animation_data) {
    if (animation_data == NULL) {
        return false;
    }

    if ((animation_data->device < eWs2812b_First) || (animation_data->device >= eWs2812b_Last)) {
        return false;
    }

    if (!g_ws2812b_api_is_init) {
        return false;
    }

    if (g_ws2812b_api_dynamic_lut[animation_data->device].is_busy) {
        return false;
    }

    if (osMutexAcquire(g_ws2812b_api_dynamic_lut[animation_data->device].mutex, MUTEX_TIMEOUT) != osOK) {
        return false;
    }

    g_ws2812b_api_dynamic_lut[animation_data->device].is_busy = true;
    g_ws2812b_api_dynamic_lut[animation_data->device].status = eLedTransferState_Start;

    osMutexRelease(g_ws2812b_api_dynamic_lut[animation_data->device].mutex);

    switch (animation_data->animation) {
        case eLedAnimation_Off: {
            return WS2812B_API_ExecuteStaticAnimation(animation_data);
        }
        case eLedAnimation_SolidColor: {
            return WS2812B_API_ExecuteStaticAnimation(animation_data);
        } break;
        case eLedAnimation_SegmentFill: {

        } break;   
        case eLedAnimation_Blink: {

        } break;
        case eLedAnimation_Rainbow: {

        } break; 
        default: {
        } break;       
    }

    return false;
}

bool WS2812B_API_Stop (const eWs2812b_t device) {
    if ((device < eWs2812b_First) || (device >= eWs2812b_Last)) {
        return false;
    }

    if (!g_ws2812b_api_is_init) {
        return false;
    }

    if (!g_ws2812b_api_dynamic_lut[device].is_busy) {
        return true;
    }

    osTimerStop(g_ws2812b_api_dynamic_lut[device].timer);
    osEventFlagsClear(g_ws2812b_api_dynamic_lut[device].flag, CALLBACK_FLAG);

    if (osMutexAcquire(g_ws2812b_api_dynamic_lut[device].mutex, MUTEX_TIMEOUT) != osOK) {
        return false;
    }

    g_ws2812b_api_dynamic_lut[device].is_busy = false;

    osMutexRelease(g_ws2812b_api_dynamic_lut[device].mutex);

    sLedAnimationDesc_t stop_animation = {.device = device, .animation = eLedAnimation_Off, .data = NULL};

    return WS2812B_API_ExecuteStaticAnimation(&stop_animation);
}

bool WS2812B_API_SetColor (const void *device_context, size_t led_number, const uint8_t r, const uint8_t g, const uint8_t b) {
    if (device_context == NULL) {
        return false;
    }
    
    eWs2812b_t device = (eWs2812b_t)device_context;
    
    if ((device < eWs2812b_First) || (device >= eWs2812b_Last)) {
        return false;
    }

    if (!g_ws2812b_api_is_init) {
        return false;
    }

    if (led_number >= g_ws2812b_api_static_lut[device].led_count) {
        return false;
    }

    led_number *= LED_DATA_CHANNELS;

    g_ws2812b_api_dynamic_lut[device].led_data[led_number] = r;
    g_ws2812b_api_dynamic_lut[device].led_data[led_number + 1] = g;
    g_ws2812b_api_dynamic_lut[device].led_data[led_number + 2] = b;

    return true;
}

bool WS2812B_API_FillColor (const void *device_context, const uint8_t r, const uint8_t g, const uint8_t b) {
    if (device_context == NULL) {
        return false;
    }
    
    eWs2812b_t *device = (eWs2812b_t*)device_context;

    if ((*device < eWs2812b_First) || (*device >= eWs2812b_Last)) {
        return false;
    }

    if (!g_ws2812b_api_is_init) {
        return false;
    }

    size_t led_byte = 0;

    for (size_t led = 0; led < g_ws2812b_api_static_lut[*device].led_count; led++) {
        led_byte = led * LED_DATA_CHANNELS;
        g_ws2812b_api_dynamic_lut[*device].led_data[led_byte] = r;
        g_ws2812b_api_dynamic_lut[*device].led_data[led_byte + 1] = g;
        g_ws2812b_api_dynamic_lut[*device].led_data[led_byte + 2] = b;
    }

    return true;
}

bool WS2812B_API_FillSegment (const void *device_context, const size_t start_led, const size_t end_led, const uint8_t r, const uint8_t g, const uint8_t b) {
    if (device_context == NULL) {
        return false;
    }
    
    eWs2812b_t device = (eWs2812b_t)device_context;
    
    if ((device < eWs2812b_First) || (device >= eWs2812b_Last)) {
        return false;
    }

    if (!g_ws2812b_api_is_init) {
        return false;
    }

    if (start_led >= end_led || end_led > g_ws2812b_api_static_lut[device].led_count) {
        return false;
    }

    size_t led_byte = 0;

    for (size_t led = start_led; led <= end_led; led++) {
        led_byte = led * LED_DATA_CHANNELS;
        g_ws2812b_api_dynamic_lut[device].led_data[led_byte] = r;
        g_ws2812b_api_dynamic_lut[device].led_data[led_byte + 1] = g;
        g_ws2812b_api_dynamic_lut[device].led_data[led_byte + 2] = b;
    }

    return true;
}
