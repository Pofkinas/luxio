/**********************************************************************************************************************
 * Includes
 *********************************************************************************************************************/

#include "ws2812b_api.h"
#include "cmsis_os2.h"
#include "heap_api.h"
#include "debug_api.h"
#include "ws2812b_driver.h"
#include "timer_driver.h"
#include "pwm_driver.h"
#include "gpio_driver.h"

#include "animation_solidcolor.h"

/**********************************************************************************************************************
 * Private definitions and macros
 *********************************************************************************************************************/

#define DEBUG_WS2812B_API

#define MUTEX_TIMEOUT 0U
#define REFRESH_RATE 33U // 30 FPS

#define CALLBACK_TIMEOUT 20U
#define CALLBACK_FLAG 0x01U

/**********************************************************************************************************************
 * Private typedef
 *********************************************************************************************************************/

typedef enum eWs2812bState {
    eWs2812bState_First = 0,
    eWs2812bState_Idle = eWs2812bState_First,
    eWs2812bState_Building,
    eWs2812bState_Running,
    eWs2812bState_Last
} eWs2812bState_t;

typedef struct sWs2812bControlDesc {
    eWs2812bDriver_t device;
    size_t max_led;
    osTimerAttr_t timer_attributes;
    osMutexAttr_t mutex_attributes;
    osEventFlagsAttr_t flag_attributes;
} sWs2812bApiDesc_t;

typedef struct sWs2812bSequence {
    eLedAnimation_t animation;
    uint8_t brightness;
    void *data;
    struct sWs2812bSequence *next;
} sWs2812bSequence_t;

typedef struct sWs2812bDynamicDesc {
    uint8_t *led_data;
    size_t led_count;
    eWs2812bState_t led_state;
    sWs2812bSequence_t *dynamic_animations;
    sWs2812bSequence_t *current_animation;
    osTimerId_t timer;
    osMutexId_t mutex;
    osEventFlagsId_t flag; 
    void (*timer_callback) (void *arg);
    eLedTransferState_t transfer_status;
} sWs2812bApiDynamicDesc_t;

typedef struct sWs2812bTimerArg {
    eWs2812b_t device;
} sWs2812bTimerArg_t;

/**********************************************************************************************************************
 * Private constants
 *********************************************************************************************************************/

#ifdef DEBUG_WS2812B_API
CREATE_MODULE_NAME (WS2812B_API)
#else
CREATE_MODULE_NAME_EMPTY
#endif

/* clang-format off */ 
const static sWs2812bApiDesc_t g_ws2812b_api_static_lut[eWs2812b_Last] = {
    [eWs2812b_1] = {
        .device = eWs2812bDriver_1,
        .max_led = WS2812B_1_LED_COUNT,
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
        .led_count = 0,
        .led_state = eWs2812bState_Idle,
        .dynamic_animations = NULL,
        .current_animation = NULL,
        .timer = NULL,
        .mutex = NULL,
        .flag = NULL,
        .timer_callback = NULL,
        .transfer_status = eLedTransferState_Start
    }
};

static sWs2812bTimerArg_t g_ws2812b_api_timer_arg[eWs2812b_Last] = {
    [eWs2812b_1] = {
        .device = eWs2812b_1,
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
static bool WS2812B_API_Update (const eWs2812b_t device);
static void WS2812B_API_DriverCallback (const eWs2812bDriver_t device, const eLedTransferState_t transfer_state);
static bool WS2812B_API_BuildStaticAnimation (sLedAnimationDesc_t *static_animation_data);
static void WS2812B_API_CastToRgb (const eColorFormat_t color_format, sLedColorRgb_t *rgb, const sLedColorHsv_t hsv);

/**********************************************************************************************************************
 * Definitions of private functions
 *********************************************************************************************************************/

static void WS2812B_API_TimerCallback (void *arg) {
    if (arg == NULL) {
        return;
    }

    sWs2812bTimerArg_t *timer_arg = (sWs2812bTimerArg_t *) arg;

    if (!WS2812B_API_IsCorrectDevice(timer_arg->device)) {
        return;
    }

    g_ws2812b_api_dynamic_lut[timer_arg->device].current_animation = g_ws2812b_api_dynamic_lut[timer_arg->device].dynamic_animations;

    while (g_ws2812b_api_dynamic_lut[timer_arg->device].current_animation->next != NULL) {
        switch (g_ws2812b_api_dynamic_lut[timer_arg->device].current_animation->animation) {
            case eLedAnimation_Blink: {
                // Update blink animation
            } break;
            case eLedAnimation_Rainbow: {
                // Update rainbow animation
            } break;
            default: {
                break;
            }
        }

        g_ws2812b_api_dynamic_lut[timer_arg->device].current_animation = g_ws2812b_api_dynamic_lut[timer_arg->device].current_animation->next;
    }

    if (!WS2812B_API_Update(timer_arg->device)) {
        
        g_ws2812b_api_dynamic_lut[timer_arg->device].led_state = eWs2812bState_Idle;

        osTimerStop(g_ws2812b_api_dynamic_lut[timer_arg->device].timer);
    }

    return;
}

static bool WS2812B_API_Update (const eWs2812b_t device) {
    if (!WS2812B_API_IsCorrectDevice(device)) {
        return false;
    }

    if (!g_ws2812b_api_is_init) {
        return false;
    }

    if (g_ws2812b_api_dynamic_lut[device].led_state != eWs2812bState_Running) {
        return false;
    }

    // TODO: Rework this g_ws2812b_api_dynamic_lut[device].led_count logic. Its mainly for optimization.

    // if (g_ws2812b_api_dynamic_lut[device].led_count > g_ws2812b_api_static_lut[device].max_led) {
    //     g_ws2812b_api_dynamic_lut[device].led_count = g_ws2812b_api_static_lut[device].max_led;
    // }

    // if (!WS2812B_Driver_Set(g_ws2812b_api_static_lut[device].device, g_ws2812b_api_dynamic_lut[device].led_data, g_ws2812b_api_dynamic_lut[device].led_count)) {
    //     is_execute_successful = false;
    // }

    if (!WS2812B_Driver_Set(g_ws2812b_api_static_lut[device].device, g_ws2812b_api_dynamic_lut[device].led_data, g_ws2812b_api_static_lut[device].max_led)) {
        return false;
    }

    if (osEventFlagsWait(g_ws2812b_api_dynamic_lut[device].flag, CALLBACK_FLAG, osFlagsWaitAny, CALLBACK_TIMEOUT) != CALLBACK_FLAG) {
        return false;
    }

    if (g_ws2812b_api_dynamic_lut[device].transfer_status != eLedTransferState_Complete) {
        return false;
    }

    //g_ws2812b_api_dynamic_lut[device].led_count = 0;

    return true;
}

static void WS2812B_API_DriverCallback (const eWs2812bDriver_t callback_device, const eLedTransferState_t transfer_state) {
    for (eWs2812b_t device = eWs2812b_First; device < eWs2812b_Last; device++) {
        if (g_ws2812b_api_static_lut[device].device == callback_device) {
            g_ws2812b_api_dynamic_lut[device].transfer_status = transfer_state;
            
            osEventFlagsSet(g_ws2812b_api_dynamic_lut[device].flag, CALLBACK_FLAG);
            
            return;
        }
    }

    return;
}

static bool WS2812B_API_BuildStaticAnimation (sLedAnimationDesc_t *static_animation_data) {
    if (static_animation_data == NULL) {
        return false;
    }
    
    if (!WS2812B_API_IsCorrectDevice(static_animation_data->device)) {
        return false;
    }

    if ((static_animation_data->animation < eLedAnimation_First) || (static_animation_data->animation >= eLedAnimation_Last)) {
        return false;
    }

    if (static_animation_data->data == NULL) {
        return false;
    }

    switch (static_animation_data->animation) {
        case eLedAnimation_SolidColor: {
            sLedAnimationSolidColor_t *data = static_animation_data->data;
            WS2812B_API_CastToRgb(data->color_format, &data->rgb, data->hsv);

            sSolidAnimationData_t solid_ctx = {
                .device = static_animation_data->device,
                .brightness = static_animation_data->brightness,
                .rgb = data->rgb
            };
        
            sLedAnimationInstance_t animation_instance = {
                .context = &solid_ctx,
                .build_animation = Animation_SolidColor_Run
            };
        
            animation_instance.build_animation(animation_instance.context);
        } break;
        case eLedAnimation_SegmentFill: {
            
        } break;
        default: {
            return false;
        } break;
    }

    if (osMutexAcquire(g_ws2812b_api_dynamic_lut[static_animation_data->device].mutex, MUTEX_TIMEOUT) != osOK) {
        return false;
    }

    g_ws2812b_api_dynamic_lut[static_animation_data->device].led_state = eWs2812bState_Idle;

    osMutexRelease(g_ws2812b_api_dynamic_lut[static_animation_data->device].mutex);

    return true;
}

static void WS2812B_API_CastToRgb (const eColorFormat_t color_format, sLedColorRgb_t *rgb, const sLedColorHsv_t hsv) {
    if (rgb == NULL) {
        return;
    }
    
    switch (color_format) {
        case eColorFormat_RGB: {
        } break;
        case eColorFormat_HSV: {
            LED_HsvToRgb(hsv, rgb);
        } break;
        default: {
        } break;
    }

    return;
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

        g_ws2812b_api_dynamic_lut[device].led_data = Heap_API_Calloc(g_ws2812b_api_static_lut[device].max_led * LED_DATA_CHANNELS, sizeof(uint8_t));

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

bool WS2812B_API_BuildAnimation (sLedAnimationDesc_t *animation_data) {
    if (animation_data == NULL) {
        TRACE_ERR("No animation data\n");
        
        return false;
    }

    if (!WS2812B_API_IsCorrectDevice(animation_data->device)) {
        TRACE_ERR("Incorrect device\n");
        
        return false;
    }

    if (!g_ws2812b_api_is_init) {
        TRACE_ERR("Device not initialized\n");
        
        return false;
    }

    if (g_ws2812b_api_dynamic_lut[animation_data->device].led_state != eWs2812bState_Idle) {
        TRACE_ERR("Device state %d\n", g_ws2812b_api_dynamic_lut[animation_data->device].led_state);
        
        return false;
    }

    if (osMutexAcquire(g_ws2812b_api_dynamic_lut[animation_data->device].mutex, MUTEX_TIMEOUT) != osOK) {
        return false;
    }

    g_ws2812b_api_dynamic_lut[animation_data->device].led_state = eWs2812bState_Building;

    osMutexRelease(g_ws2812b_api_dynamic_lut[animation_data->device].mutex);

    switch (animation_data->animation) {
        case eLedAnimation_SolidColor: {
            return WS2812B_API_BuildStaticAnimation(animation_data);
        }
        case eLedAnimation_SegmentFill: {

        } 
        case eLedAnimation_Blink: {

        } break;
        case eLedAnimation_Rainbow: {

        } break; 
        default: {
        } break;       
    }

    sWs2812bSequence_t *new_animation = Heap_API_Malloc(sizeof(sWs2812bSequence_t));
    
    if (new_animation == NULL) {
        TRACE_ERR("Malloc failed\n");
        
        return false;
    }

    new_animation->animation = animation_data->animation;
    new_animation->brightness = animation_data->brightness;
    new_animation->data = animation_data->data;
    new_animation->next = NULL;

    if (g_ws2812b_api_dynamic_lut[animation_data->device].current_animation != NULL) {
        new_animation->next = g_ws2812b_api_dynamic_lut[animation_data->device].current_animation;
    } 

    g_ws2812b_api_dynamic_lut[animation_data->device].dynamic_animations = new_animation;
    g_ws2812b_api_dynamic_lut[animation_data->device].current_animation = new_animation;

    return false;
}

bool WS2812B_API_ClearAnimations (const eWs2812b_t device) {
    if (!WS2812B_API_IsCorrectDevice(device)) {
        TRACE_ERR("Incorrect device\n");
        
        return false;
    }

    if (!g_ws2812b_api_is_init) {
        TRACE_ERR("Device not initialized\n");

        return false;
    }

    if (g_ws2812b_api_dynamic_lut[device].led_state != eWs2812bState_Idle) {
        TRACE_ERR("Device state %d\n", g_ws2812b_api_dynamic_lut[device].led_state);

        
        return false;
    }

    if (g_ws2812b_api_dynamic_lut[device].dynamic_animations == NULL) {
        return true;
    }

    if (osMutexAcquire(g_ws2812b_api_dynamic_lut[device].mutex, MUTEX_TIMEOUT) != osOK) {
        return false;
    }

    while (g_ws2812b_api_dynamic_lut[device].dynamic_animations->next != NULL) {
        if (!Heap_API_Free(g_ws2812b_api_dynamic_lut[device].dynamic_animations->data)) {
            TRACE_ERR("Heap API failed\n");
            
            osMutexRelease(g_ws2812b_api_dynamic_lut[device].mutex);
            
            return false;
        }

        g_ws2812b_api_dynamic_lut[device].dynamic_animations = g_ws2812b_api_dynamic_lut[device].dynamic_animations->next;
    }

    if (!Heap_API_Free(g_ws2812b_api_dynamic_lut[device].dynamic_animations->data)) {
        TRACE_ERR("Heap API failed\n");
        
        osMutexRelease(g_ws2812b_api_dynamic_lut[device].mutex);

        return false;
    }

    osMutexRelease(g_ws2812b_api_dynamic_lut[device].mutex);

    return true;
}

bool WS2812B_API_Start (const eWs2812b_t device) {
    if (!WS2812B_API_IsCorrectDevice(device)) {
        TRACE_ERR("Incorrect device\n");
        
        return false;
    }

    if (!g_ws2812b_api_is_init) {
        TRACE_ERR("Device not initialized\n");
        
        return false;
    }

    if (g_ws2812b_api_dynamic_lut[device].led_state != eWs2812bState_Idle) {
        TRACE_ERR("Device state %d\n", g_ws2812b_api_dynamic_lut[device].led_state);

        return false;
    }

    if (osMutexAcquire(g_ws2812b_api_dynamic_lut[device].mutex, MUTEX_TIMEOUT) != osOK) {
        return false;
    }
    
    g_ws2812b_api_dynamic_lut[device].led_state = eWs2812bState_Running;

    if (!WS2812B_API_Update(device)) {
        TRACE_ERR("WS2812B_API_Update failed\n");
        
        osMutexRelease(g_ws2812b_api_dynamic_lut[device].mutex);
        
        return false;
    }

    if (g_ws2812b_api_dynamic_lut[device].dynamic_animations == NULL) {
        g_ws2812b_api_dynamic_lut[device].led_state = eWs2812bState_Idle;

        osMutexRelease(g_ws2812b_api_dynamic_lut[device].mutex);

        return true;
    }

    osTimerStart(g_ws2812b_api_dynamic_lut[device].timer, REFRESH_RATE);

    osMutexRelease(g_ws2812b_api_dynamic_lut[device].mutex);

    // Do i need to wait for fist timer callback flag? (in case of only static data i would know when to update)

    return true;
}

bool WS2812B_API_Stop (const eWs2812b_t device) {
    if (!WS2812B_API_IsCorrectDevice(device)) {
        TRACE_ERR("Incorrect device\n");
        
        return false;
    }

    if (!g_ws2812b_api_is_init) {
        TRACE_ERR("Device not initialized\n");

        return false;
    }

    if (g_ws2812b_api_dynamic_lut[device].led_state != eWs2812bState_Idle) {
        TRACE_ERR("Device state %d\n", g_ws2812b_api_dynamic_lut[device].led_state);

        return false;
    }

    if (osMutexAcquire(g_ws2812b_api_dynamic_lut[device].mutex, MUTEX_TIMEOUT) != osOK) {
        return false;
    }

    osTimerStop(g_ws2812b_api_dynamic_lut[device].timer);
    osEventFlagsClear(g_ws2812b_api_dynamic_lut[device].flag, CALLBACK_FLAG);

    g_ws2812b_api_dynamic_lut[device].led_state = eWs2812bState_Idle;

    osMutexRelease(g_ws2812b_api_dynamic_lut[device].mutex);

    return WS2812B_Driver_Reset(g_ws2812b_api_static_lut[device].device);
}

bool WS2812B_API_Reset (const eWs2812b_t device) {
    if (!WS2812B_API_IsCorrectDevice(device)) {
        TRACE_ERR("Incorrect device\n");
        
        return false;
    }

    if (!g_ws2812b_api_is_init) {
        TRACE_ERR("Device not initialized\n");

        return false;
    }

    if (g_ws2812b_api_dynamic_lut[device].led_state != eWs2812bState_Idle) {
        TRACE_ERR("Device state %d\n", g_ws2812b_api_dynamic_lut[device].led_state);

        return false;
    }

    if (!WS2812B_Driver_Reset(g_ws2812b_api_static_lut[device].device)) {
        TRACE_ERR("WS2812B_Driver_Reset failed\n");
        
        return false;
    }

    if (osEventFlagsWait(g_ws2812b_api_dynamic_lut[device].flag, CALLBACK_FLAG, osFlagsWaitAny, CALLBACK_TIMEOUT) != CALLBACK_FLAG) {
        TRACE_ERR("Failed to receive event flag\n");

        return false;
    }

    if (g_ws2812b_api_dynamic_lut[device].transfer_status != eLedTransferState_Complete) {
        TRACE_ERR("Transfer status %d\n", g_ws2812b_api_dynamic_lut[device].transfer_status);
        
        return false;
    }

    return true;
}

bool WS2812B_API_IsCorrectDevice (const eWs2812b_t device) {
    return (device >= eWs2812b_First) && (device < eWs2812b_Last);
}

bool WS2812B_API_SetColor (const eWs2812b_t device, size_t led_number, const uint8_t r, const uint8_t g, const uint8_t b) {
    if (!WS2812B_API_IsCorrectDevice(device)) {
        TRACE_ERR("Incorrect device\n");
        
        return false;
    }

    if (!g_ws2812b_api_is_init) {
        TRACE_ERR("Device not initialized\n");

        return false;
    }

    if (led_number >= g_ws2812b_api_static_lut[device].max_led) {
        TRACE_ERR("Led number %d is out of range\n", led_number);
        
        return false;
    }

    led_number *= LED_DATA_CHANNELS;

    g_ws2812b_api_dynamic_lut[device].led_data[led_number] = r;
    g_ws2812b_api_dynamic_lut[device].led_data[led_number + 1] = g;
    g_ws2812b_api_dynamic_lut[device].led_data[led_number + 2] = b;

    return true;
}

bool WS2812B_API_FillColor (const eWs2812b_t device, const uint8_t r, const uint8_t g, const uint8_t b) {
    if (!WS2812B_API_IsCorrectDevice(device)) {
        TRACE_ERR("Incorrect device\n");
        
        return false;
    }

    if (!g_ws2812b_api_is_init) {
        TRACE_ERR("Device not initialized\n");

        return false;
    }

    size_t led_byte = 0;

    for (size_t led = 0; led < g_ws2812b_api_static_lut[device].max_led; led++) {
        led_byte = led * LED_DATA_CHANNELS;
        g_ws2812b_api_dynamic_lut[device].led_data[led_byte] = r;
        g_ws2812b_api_dynamic_lut[device].led_data[led_byte + 1] = g;
        g_ws2812b_api_dynamic_lut[device].led_data[led_byte + 2] = b;
    }

    return true;
}

bool WS2812B_API_FillSegment (const eWs2812b_t device, const size_t start_led, const size_t end_led, const uint8_t r, const uint8_t g, const uint8_t b) {
    if (!WS2812B_API_IsCorrectDevice(device)) {
        TRACE_ERR("Incorrect device\n");
        
        return false;
    }

    if (!g_ws2812b_api_is_init) {
        TRACE_ERR("Device not initialized\n");

        return false;
    }

    if (start_led >= end_led || end_led > g_ws2812b_api_static_lut[device].max_led) {
        TRACE_ERR("Incorect segment range; start: %d, end: %d\n", start_led, end_led);
        
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
