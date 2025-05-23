/**********************************************************************************************************************
 * Includes
 *********************************************************************************************************************/

#include "button_api.h"
#include <stdio.h>
#include "debug_api.h"
#include "exti_driver.h"
#include "gpio_driver.h"

/**********************************************************************************************************************
 * Private definitions and macros
 *********************************************************************************************************************/

//#define DEBUG_BUTTON_API

#define MESSAGE_QUEUE_PRIORITY 0U
#define MESSAGE_QUEUE_TIMEOUT 5U
#define MUTEX_TIMEOUT 0U
#define BUTTON_MESSAGE_CAPACITY 10

/**********************************************************************************************************************
 * Private typedef
 *********************************************************************************************************************/

typedef enum eButtonState {
    eButtonState_First = 0,
    eButtonState_Default = eButtonState_First,
    eButtonState_Init,
    eButtonState_Debounce,
    eButtonState_Last
} eButtonState_t;

typedef struct sButtonDesc {
    eGpioPin_t gpio_pin;
    bool active_state;
    bool is_debounce_enable;
    osMutexAttr_t button_mutex_attributes;
    osTimerAttr_t debouce_timer_attributes;
    bool is_exti;
    eExtiDriver_t exti_device;
} sButtonDesc_t;

typedef struct sButtonDynamic {
    eButton_t button;
    eButtonState_t debounce_state;
    osMutexId_t button_mutex;
    osEventFlagsId_t callback_flag;
    osTimerId_t debouce_timer;
    bool button_value;
} sButtonDynamic_t;

/**********************************************************************************************************************
 * Private constants
 *********************************************************************************************************************/

#ifdef DEBUG_BUTTON_API
CREATE_MODULE_NAME (BUTTON_API)
#else
CREATE_MODULE_NAME_EMPTY
#endif

const static osThreadAttr_t g_button_thread_attributes = {
    .name = "Button_Thread",
    .stack_size = 128 * 3,
    .priority = (osPriority_t) osPriorityNormal
};

const static osMessageQueueAttr_t g_button_message_queue_attributes = {
    .name = "Button_API_MessageQueue", 
    .attr_bits = 0, 
    .cb_mem = NULL, 
    .cb_size = 0, 
    .mq_mem = NULL, 
    .mq_size = 0
};

/* clang-format off */
const static sButtonDesc_t g_static_button_desc_lut[eButton_Last] = {
    [eButton_StartStop] = {
        .gpio_pin = eGpioPin_StartButton,
        .active_state = false,
        .is_debounce_enable = true,
        .button_mutex_attributes = {.name = "Button_StartStop_Mutex", .attr_bits = osMutexRecursive | osMutexPrioInherit, .cb_mem = NULL, .cb_size = 0U},
        .debouce_timer_attributes = {.name = "Button_StartStop_Debounce_Timer", .attr_bits = 0, .cb_mem = NULL, .cb_size = 0},
        .is_exti = true,
        .exti_device = eExtiDriver_StartButton
    }
};
/* clang-format on */

/**********************************************************************************************************************
 * Private variables
 *********************************************************************************************************************/

static osThreadId_t g_button_thread_id = NULL;
static osMessageQueueId_t g_button_message_queue_id = NULL;
static bool g_has_pooled_buttons = false;

/* clang-format off */
static sButtonDynamic_t g_dynamic_button_lut[eButton_Last] = {
    [eButton_StartStop] = {
        .debounce_state = eButtonState_Default,
        .button_mutex = NULL,
        .callback_flag = NULL,
        .debouce_timer = NULL,
        .button_value = false
    }
};
/* clang-format on */

/**********************************************************************************************************************
 * Exported variables and references
 *********************************************************************************************************************/

/**********************************************************************************************************************
 * Prototypes of private functions
 *********************************************************************************************************************/

static void Button_API_Thread (void *arg);
static void Button_API_DebounceTimerCallback (void *context);
static void Button_API_ExtiTriggered (void *context);
static void Button_API_StartDebounceTimer (const eButton_t button);

/**********************************************************************************************************************
 * Definitions of private functions
 *********************************************************************************************************************/

static void Button_API_Thread (void *arg) {
    eButton_t button;
    
    while(1) {
        if (osMessageQueueGet(g_button_message_queue_id, &button, MESSAGE_QUEUE_PRIORITY, MESSAGE_QUEUE_TIMEOUT) == osOK) {    
            if (g_static_button_desc_lut[button].is_debounce_enable) {
                Button_API_StartDebounceTimer(button);

                TRACE_INFO("Button [%d] Debounce triggered\n", button);
            }
        }

        if (!g_has_pooled_buttons) {
            continue;
        }

        for (button = eButton_First; button < eButton_Last; button++) {
            if (g_static_button_desc_lut[button].is_debounce_enable && (g_dynamic_button_lut[button].debounce_state == eButtonState_Debounce)) {
                continue;
            }

            if (!GPIO_Driver_ReadPin(g_static_button_desc_lut[button].gpio_pin, &g_dynamic_button_lut[button].button_value)) {
                continue;
            }
            
            if (g_dynamic_button_lut[button].button_value != g_static_button_desc_lut[button].active_state) {
                continue;
            }

            if (g_static_button_desc_lut[button].is_debounce_enable) {
                Button_API_StartDebounceTimer(button);
            } else {
                TRACE_INFO("Button [%d] triggered\n", button);

                osEventFlagsSet(g_dynamic_button_lut[button].callback_flag, BUTTON_TRIGGERED_EVENT);
            }
        }

        osThreadYield();
    }
}

static void Button_API_ExtiTriggered (void *context) {
    sButtonDynamic_t *button = (sButtonDynamic_t*) context;

    if (!g_static_button_desc_lut[button->button].is_debounce_enable) {
        osEventFlagsSet(button->callback_flag, BUTTON_TRIGGERED_EVENT);

        return;
    }

    Exti_Driver_Disable_IT(g_static_button_desc_lut[button->button].exti_device);
    osMessageQueuePut(g_button_message_queue_id, &button->button, MESSAGE_QUEUE_PRIORITY, 0);

    return;
}

static void Button_API_DebounceTimerCallback (void *context) {
    sButtonDynamic_t *debounce_button = (sButtonDynamic_t*) context;
    bool debounce_status = true;

    if (debounce_button->debounce_state != eButtonState_Debounce) {
        TRACE_WRN("Debounce callback exited early, state [%d]\n", debounce_button->debounce_state);
        
        return;
    }

    if (!GPIO_Driver_ReadPin(g_static_button_desc_lut[debounce_button->button].gpio_pin, &debounce_button->button_value)) {
        debounce_status = false;
    }

    if (debounce_button->button_value != g_static_button_desc_lut[debounce_button->button].active_state) {
        debounce_status = false;
    }

    if (osMutexAcquire(debounce_button->button_mutex, MUTEX_TIMEOUT) != osOK) {
        debounce_status = false;
    }

    if (g_static_button_desc_lut[debounce_button->button].is_exti) {
        if (!Exti_Driver_ClearFlag(g_static_button_desc_lut[debounce_button->button].exti_device)) {
            debounce_status = false;
        }

        Exti_Driver_Enable_IT(g_static_button_desc_lut[debounce_button->button].exti_device);
    }

    debounce_button->debounce_state = eButtonState_Init;

    if (!debounce_status) {  
        osMutexRelease(debounce_button->button_mutex);

        TRACE_WRN("Button [%d] debounce failed\n", debounce_button->button);

        return;
    }

    TRACE_INFO("Button [%d] triggered\n", debounce_button->button);

    osEventFlagsSet(debounce_button->callback_flag, BUTTON_TRIGGERED_EVENT);

    osMutexRelease(debounce_button->button_mutex);

    return;
}

static void Button_API_StartDebounceTimer (const eButton_t button) {
    if (!Button_API_IsCorrectButton(button)) {
        return;
    }

    if (g_dynamic_button_lut[button].debounce_state != eButtonState_Init) {
        return;
    }

    if (osMutexAcquire(g_dynamic_button_lut[button].button_mutex, MUTEX_TIMEOUT) != osOK) {
        return;
    }

    g_dynamic_button_lut[button].debounce_state = eButtonState_Debounce;
    osTimerStart(g_dynamic_button_lut[button].debouce_timer, BUTTON_DEBOUNCE_PERIOD);

    osMutexRelease(g_dynamic_button_lut[button].button_mutex);

    return;
}

/**********************************************************************************************************************
 * Definitions of exported functions
 *********************************************************************************************************************/

bool Button_API_Init (eButton_t button, osEventFlagsId_t event_flags_id) {
    if (!Button_API_IsCorrectButton(button)) {
        return false;
    }

    if (g_dynamic_button_lut[button].debounce_state != eButtonState_Default) {
        return true;
    }

    if (!GPIO_Driver_InitAllPins()) {
        return false;
    }

    if (!Exti_Driver_InitDevice(g_static_button_desc_lut[button].exti_device, &Button_API_ExtiTriggered, &g_dynamic_button_lut[button])) {
        return false;
    }

    if (g_button_thread_id == NULL) {
        g_button_thread_id = osThreadNew(Button_API_Thread, NULL, &g_button_thread_attributes);
    }

    if (g_button_message_queue_id == NULL) {
        g_button_message_queue_id = osMessageQueueNew(BUTTON_MESSAGE_CAPACITY, sizeof(eButton_t), &g_button_message_queue_attributes);
    }

    if (g_static_button_desc_lut[button].is_debounce_enable) {
        g_dynamic_button_lut[button].debouce_timer = osTimerNew(Button_API_DebounceTimerCallback, osTimerOnce, &g_dynamic_button_lut[button], &g_static_button_desc_lut[button].debouce_timer_attributes);
    }

    if (g_dynamic_button_lut[button].button_mutex == NULL) {
        g_dynamic_button_lut[button].button_mutex = osMutexNew(&g_static_button_desc_lut[button].button_mutex_attributes);
    }

    g_dynamic_button_lut[button].callback_flag = event_flags_id;

    if (!g_static_button_desc_lut[button].is_exti) {
        g_has_pooled_buttons = true;
    }

    if (!Exti_Driver_Enable_IT(g_static_button_desc_lut[button].exti_device)) {
        return false;
    }

    g_dynamic_button_lut[button].debounce_state = eButtonState_Init;
    g_dynamic_button_lut[button].button = button;

    return true;
}

bool Button_API_IsCorrectButton (const eButton_t button) {
    return (button >= eButton_First) && (button < eButton_Last);
}
