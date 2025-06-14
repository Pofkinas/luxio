/**********************************************************************************************************************
 * Includes
 *********************************************************************************************************************/

#include "reaction_test_app.h"
#include <stddef.h>
#include <stdlib.h>
#include <math.h>
#include "cmsis_os2.h"
#include "vl53l0xv2_api.h"
#include "ws2812b_api.h"
#include "io_api.h"
#include "heap_api.h"
#include "debug_api.h"
#include "led_color.h"
#include "math_utils.h"
#include "message.h"
#include "framework_config.h"

#include "game_mode_classic.h"

/**********************************************************************************************************************
 * Private definitions and macros
 *********************************************************************************************************************/

#define DEBUG_REACTION_TEST_APP

#define START_STOP_BUTTON eIo_StartStopButton

#define DISPLAY_MESSAGE_SIZE 64

#define SINGLE_SEGMENT_LENGTH_UM 16670
#define DEFAULT_LED_BRIGHTNESS 192
#define DEFAULT_GET_DISTANCE_TIMEOUT 100
#define WAIT_CLEAR_TIME 3000
#define ERROR_LED_COLOR eLedColor_Red
#define DEFAULT_MEASURE_TIMEOUT_FLAG 0x04U
#define WAIT_BETWEEN_ATTEMPTS 2000

#define DEFAULT_ATTEMPTS 5
#define DEFAULT_TARGET_LED_COUNT 5
#define DEFAULT_DIFFICULTY 1
#define DEFAULT_MEASURE_TIMEOUT 10000

/**********************************************************************************************************************
 * Private typedef
 *********************************************************************************************************************/

typedef enum eReactionTestState {
    eReactionTestState_First = 0,
    eReactionTestState_Off = eReactionTestState_First,
    eReactionTestState_Init,
    eReactionTestState_Start,
    eReactionTestState_Measure,
    eReactionTestState_Process,
    eReactionTestState_End,
    eReactionTestState_Last
} eReactionTestState_t;

typedef enum eGameMode {
    eGameMode_First = 0,
    eGameMode_Classic = eGameMode_First,
    eGameMode_Last
} eGameMode_t;

typedef struct sReactionModuleDesc {
    eVl53l0x_t vl53l0x;
    eWs2812b_t ws2812b;
    osTimerAttr_t segment_timer_attributes;
    eLedColor_t base_color;
    eLedColor_t target_color;
    uint8_t led_brightness;
} sReactionTestDesc_t;

typedef struct sReactionModuleDynamicDesc {
    eModule_t module;
    sModuleState_t state;
    osTimerId_t segment_timer;
    sLedAnimationSolidColor_t led_solid_color;
    sLedAnimationSegmentFill_t led_segment_fill;
    uint16_t total_led_count;
    uint8_t target_led_count;
    uint16_t led_strip_length;
    uint16_t target_distance;
    uint32_t start_time;
    uint32_t end_time;
    uint16_t registerd_distance;
    uint32_t timer_event_flag;
} sReactionTestDynamicDesc_t;

/**********************************************************************************************************************
 * Private constants
 *********************************************************************************************************************/
 
#ifdef DEBUG_REACTION_TEST_APP
CREATE_MODULE_NAME (REACTION_TEST_APP)
#else
CREATE_MODULE_NAME_EMPTY
#endif

/* clang-format off */ 
const static osThreadAttr_t g_reaction_test_thread_attributes = {
    .name = "Reaction_Test_Thread",
    .stack_size = 256 * 12,
    .priority = (osPriority_t) osPriorityNormal
};

const static osEventFlagsAttr_t g_start_button_event_attributes = {
    .name = "Start_Button_Event",
    .attr_bits = 0,
    .cb_mem = NULL,
    .cb_size = 0
};

const static osTimerAttr_t g_measure_timeout_timer_attributes = {
    .name = "Measure_Timeout_Timer",
    .attr_bits = 0,
    .cb_mem = NULL,
    .cb_size = 0
};

const static sReactionTestDesc_t g_static_reaction_test_desc[eModule_Last] = {
    [eModule_1] = {
        .vl53l0x = eVl53l0x_1,
        .ws2812b = eWs2812b_1,
        .segment_timer_attributes = {.name = "Reaction_Module_1_Timer", .attr_bits = 0, .cb_mem = NULL, .cb_size = 0U},
        .base_color = eLedColor_Blue,
        .target_color = eLedColor_Yellow,
        .led_brightness = DEFAULT_LED_BRIGHTNESS
    },
    [eModule_2] = {
        .vl53l0x = eVl53l0x_2,
        .ws2812b = eWs2812b_2,
        .segment_timer_attributes = {.name = "Reaction_Module_2_Timer", .attr_bits = 0, .cb_mem = NULL, .cb_size = 0U},
        .base_color = eLedColor_Blue,
        .target_color = eLedColor_Yellow,
        .led_brightness = DEFAULT_LED_BRIGHTNESS
    }
};
/* clang-format on */ 

/**********************************************************************************************************************
 * Private variables
 *********************************************************************************************************************/
 
static bool g_is_initialized = false;
static osThreadId_t g_reaction_test_thread_id = NULL;
static osTimerId_t g_measure_timeout_timer = NULL;
static osEventFlagsId_t g_start_button_event = NULL;

static osEventFlagsId_t g_timer_flag = NULL;

static eReactionTestState_t g_reaction_test_state = eReactionTestState_Off;
static eGameMode_t g_game_mode = eGameMode_Classic;
static sGameModeInstance_t g_game_mode_instance;
static sLedAnimationDesc_t g_led_animation = {.brightness = DEFAULT_LED_BRIGHTNESS};
static char g_display_message[DISPLAY_MESSAGE_SIZE];
static sMessage_t g_message = {.data = g_display_message, .size = DISPLAY_MESSAGE_SIZE};

static eModule_t *g_active_modules;
static uint8_t g_active_modules_count = 0;

static uint8_t g_difficulty = DEFAULT_DIFFICULTY;
static uint8_t g_total_attempts = DEFAULT_ATTEMPTS;

/* clang-format off */ 
static sReactionTestDynamicDesc_t g_dynamic_reaction_test_desc[eModule_Last] = {
    [eModule_1] = {
        .state = eReactionTestState_Init,
        .segment_timer = NULL,
        .target_led_count = DEFAULT_TARGET_LED_COUNT,
        .target_distance = 0,
        .start_time = 0,
        .end_time = 0,
        .registerd_distance = 0,
        .timer_event_flag = 0x02U
    },
    [eModule_2] = {
        .state = eReactionTestState_Init,
        .segment_timer = NULL,
        .target_led_count = DEFAULT_TARGET_LED_COUNT,
        .target_distance = 0,
        .start_time = 0,
        .end_time = 0,
        .registerd_distance = 0,
        .timer_event_flag = 0x02U
    }
};
/* clang-format on */ 

/**********************************************************************************************************************
 * Exported variables and references
 *********************************************************************************************************************/

/**********************************************************************************************************************
 * Prototypes of private functions
 *********************************************************************************************************************/
 
static void Reaction_Test_Thread (void* arg);
static bool Reaction_Test_InitModules (void);
static void Reaction_Test_DelayStartTimer (void *arg);
static void Reaction_Test_MeasureTimeoutTimer (void *arg);
static sModuleState_t Reaction_Test_IsModuleClear (const eModule_t module);

/**********************************************************************************************************************
 * Definitions of private functions
 *********************************************************************************************************************/
 
static void Reaction_Test_Thread (void* arg) {
    if (VL53L0X_API_InitAll()) {
        g_reaction_test_state = eReactionTestState_Init;
    } else {
        TRACE_ERR("Failed to init vl53l0x\n");
    }
    
    while (true) {
        if (g_reaction_test_state == eReactionTestState_Off) {
            TRACE_ERR("Reaction Test Thread terminated\n");
            osThreadTerminate(g_reaction_test_thread_id);
        }

        if (g_reaction_test_state != eReactionTestState_Init) {
            if (osEventFlagsWait(g_start_button_event, STARTSTOP_TRIGGERED_EVENT, osFlagsWaitAny, 0U) == STARTSTOP_TRIGGERED_EVENT) {
                TRACE_INFO("Stop reaction test\n");

                g_reaction_test_state = eReactionTestState_Init;
            }
        }
        
        switch (g_reaction_test_state) {
            case eReactionTestState_Init: {
                if (osEventFlagsWait(g_timer_flag, DEFAULT_MEASURE_TIMEOUT_FLAG, osFlagsWaitAny, 0U) == DEFAULT_MEASURE_TIMEOUT_FLAG) {
                    TRACE_ERR("Measure timeout\n");
                    
                    Reaction_Test_HandleGameError(eGameError_MeasureTimeout);
                }

                if (g_game_mode_instance.game_mode_data != NULL) {
                    Heap_API_Free(g_game_mode_instance.game_mode_data);
                }

                if (osTimerIsRunning(g_measure_timeout_timer)) {
                    osTimerStop(g_measure_timeout_timer);
                }
                
                if (!Reaction_Test_InitModules()) {
                    //TRACE_ERR("Failed to init reaction test\n");

                    g_reaction_test_state = eReactionTestState_Off;

                    break;
                }
                
                if (osEventFlagsWait(g_start_button_event, STARTSTOP_TRIGGERED_EVENT, osFlagsWaitAny, osWaitForever) != STARTSTOP_TRIGGERED_EVENT) {
                    TRACE_ERR("Failed to to receive start button flag\n");
                    
                    break;
                }

                srand(osKernelGetTickCount());

                switch (g_game_mode) {
                    case eGameMode_Classic: {
                        sGameModeClassic_t *data = Heap_API_Calloc(1, sizeof(sGameModeClassic_t));
                        
                        if (data == NULL) {
                            TRACE_ERR("Failed alloc memory for game mode data\n");

                            g_reaction_test_state = eReactionTestState_Init;
                            
                            break;
                        }

                        data->difficulty = g_difficulty;
                        data->total_attempts = g_total_attempts;
                        data->display_message.data = g_display_message;
                        data->display_message.size = sizeof(g_display_message);

                        g_game_mode_instance.game_mode_data = data;
                        g_game_mode_instance.game_mode_start = Game_Mode_Classic_Start;
                        g_game_mode_instance.game_mode_process = Game_Mode_Classic_Process;
                        g_game_mode_instance.game_mode_is_restart = Game_Mode_Classic_IsRestart;
                        g_game_mode_instance.game_mode_stop = Game_Mode_Classic_Stop;
                        g_game_mode_instance.game_mode_reset = Game_Mode_Classic_Reset;
                        g_game_mode_instance.get_active_modules = Game_Mode_Classic_GetActiveModules;
                    } break;
                    default: {
                        g_reaction_test_state = eReactionTestState_Init;
                        
                        break;
                    }
                }

                TRACE_INFO("Start reaction test\n");

                g_reaction_test_state = eReactionTestState_Start;
            }
            case eReactionTestState_Start: {
                if (!g_game_mode_instance.game_mode_start(g_game_mode_instance.game_mode_data)) {
                    break;
                }

                g_active_modules = g_game_mode_instance.get_active_modules(&g_active_modules_count);

                if (g_active_modules == NULL) {
                    TRACE_ERR("Failed to get active modules\n");

                    g_reaction_test_state = eReactionTestState_Init;

                    break;
                }

                if (g_reaction_test_state == eReactionTestState_Start) {
                    g_reaction_test_state = eReactionTestState_Measure;
                }
            } break;
            case eReactionTestState_Measure: {
                uint8_t registered_modules = 0;

                for (uint8_t module = 0; module < g_active_modules_count; module++) {
                    if (g_dynamic_reaction_test_desc[g_active_modules[module]].state == eModuleState_Ready) {
                        if (osEventFlagsWait(g_timer_flag, g_dynamic_reaction_test_desc[g_active_modules[module]].timer_event_flag, osFlagsWaitAny, 0U) == g_dynamic_reaction_test_desc[g_active_modules[module]].timer_event_flag) {
                            if (!WS2812B_API_Start(g_static_reaction_test_desc[g_active_modules[module]].ws2812b)) {
                                //TRACE_ERR("Failed to start animation on [%d] module\n", module);
                        
                                g_reaction_test_state = eReactionTestState_Init;
                        
                                return;
                            }
                        
                            g_dynamic_reaction_test_desc[g_active_modules[module]].state = eModuleState_Measuring;
                            g_dynamic_reaction_test_desc[g_active_modules[module]].start_time = osKernelGetTickCount();

                            continue;
                        }

                        if (Reaction_Test_IsModuleClear(g_active_modules[module]) == eModuleState_Last) {
                            Reaction_Test_HandleGameError(eGameError_InvalidStart);
                            g_reaction_test_state = eReactionTestState_Init;
                    
                            break;
                        }
                    } 

                    if (g_dynamic_reaction_test_desc[g_active_modules[module]].state != eModuleState_Measuring) {
                        continue;
                    }

                    if (!VL53L0X_API_GetDistance(g_static_reaction_test_desc[g_active_modules[module]].vl53l0x, &g_dynamic_reaction_test_desc[g_active_modules[module]].registerd_distance, DEFAULT_GET_DISTANCE_TIMEOUT)) {
                        continue;
                    }

                    if (g_dynamic_reaction_test_desc[g_active_modules[module]].registerd_distance > g_dynamic_reaction_test_desc[g_active_modules[module]].led_strip_length) {
                        continue;
                    }

                    g_dynamic_reaction_test_desc[g_active_modules[module]].end_time = osKernelGetTickCount();
                    g_dynamic_reaction_test_desc[g_active_modules[module]].state = eModuleState_Registered;

                    WS2812B_API_Reset(g_static_reaction_test_desc[g_active_modules[module]].ws2812b);

                    registered_modules++;
                }

                if (registered_modules != g_active_modules_count) {
                    break;
                }

                g_reaction_test_state = eReactionTestState_Process;
            } break;
            case eReactionTestState_Process: {
                osTimerStop(g_measure_timeout_timer);

                for (uint8_t module = 0; module < g_active_modules_count; module++) {
                    if (g_dynamic_reaction_test_desc[g_active_modules[module]].state != eModuleState_Registered) {
                        continue;
                    }
                    
                    switch (g_game_mode) {
                        case eGameMode_Classic: {
                            sGameModeClassic_t *data = (sGameModeClassic_t*) g_game_mode_instance.game_mode_data;
                            
                            data->start_time = g_dynamic_reaction_test_desc[g_active_modules[module]].start_time;
                            data->end_time = g_dynamic_reaction_test_desc[g_active_modules[module]].end_time;
                            data->registerd_distance = g_dynamic_reaction_test_desc[g_active_modules[module]].registerd_distance;
                        } break;
                        default: {
                            g_reaction_test_state = eReactionTestState_Init;
                            
                            break;
                        }
                    }

                    g_game_mode_instance.game_mode_process(g_game_mode_instance.game_mode_data);
                }

                if (g_reaction_test_state == eReactionTestState_Process) {
                    g_reaction_test_state = eReactionTestState_End;
                }
            } break;
            case eReactionTestState_End: {
                if (g_game_mode_instance.game_mode_is_restart(g_game_mode_instance.game_mode_data)) {
                    g_reaction_test_state = eReactionTestState_Start;

                    osDelay(WAIT_BETWEEN_ATTEMPTS);

                    break;
                } 

                g_game_mode_instance.game_mode_stop(g_game_mode_instance.game_mode_data);
                g_game_mode_instance.game_mode_reset(g_game_mode_instance.game_mode_data);

                if (g_game_mode_instance.game_mode_data != NULL) {
                    Heap_API_Free(g_game_mode_instance.game_mode_data);
                }

                g_reaction_test_state = eReactionTestState_Init;
            } break;
            default: {
                break;
            }
        }
    }

    osThreadYield();
}

static bool Reaction_Test_InitModules (void) {
    bool is_init_successful = true;

    for (eModule_t module = eModule_First; module < eModule_Last; module++) {
        g_dynamic_reaction_test_desc[module].registerd_distance = 0;
        g_dynamic_reaction_test_desc[module].state = eModuleState_Off;
        
        if (osTimerIsRunning(g_dynamic_reaction_test_desc[module].segment_timer)) {
            if (osTimerStop(g_dynamic_reaction_test_desc[module].segment_timer) != osOK) {
                TRACE_ERR("Failed to stop timer on [%d] module\n", module);
            }
        }

        if (!WS2812B_API_Reset(g_static_reaction_test_desc[module].ws2812b)) {
            TRACE_ERR("Failed to init [%d] module: WS2812B API Reset failed\n", module);

            is_init_successful = false;

            break;
        }
        
       if (!VL53L0X_API_StopMeasuring(g_static_reaction_test_desc[module].vl53l0x)) {
           TRACE_ERR("Failed to init [%d] module: VL53L0X API Disable failed\n", module);

           is_init_successful = false;

           return false;
       }
    }

    return is_init_successful;
}

static void Reaction_Test_DelayStartTimer (void *arg) {
    if (arg == NULL) {
        TRACE_ERR("Failed timer: Invalid argument\n");

        return;
    }

    sReactionTestDynamicDesc_t *module = (sReactionTestDynamicDesc_t *)arg;

    if (!Reaction_Test_IsCorrectModule(module->module)) {
        TRACE_ERR("Failed timer: Incorrect [%d] Module\n", module->module);

        return;
    }

    if (g_reaction_test_state != eReactionTestState_Measure) {
        TRACE_ERR("Failed timer: Incorrect FSM state[%d]\n", g_reaction_test_state);

        return;
    }

    if (module->state != eModuleState_Ready) {
        TRACE_ERR("Failed timer: Module [%d] state [%d] incorrect\n", module->module, module->state);

        g_reaction_test_state = eReactionTestState_Init;

        return;
    }

    g_led_animation.device = g_static_reaction_test_desc[module->module].ws2812b;
    g_led_animation.animation = eLedAnimation_SegmentFill;
    g_led_animation.data = &g_dynamic_reaction_test_desc[module->module].led_segment_fill; 

    if (!WS2812B_API_AddAnimation(&g_led_animation)) {
        //TRACE_ERR("Failed to add animation to [%d] module\n", module->module);

        g_reaction_test_state = eReactionTestState_Init;

        return;
    }

    osEventFlagsSet(g_timer_flag, module->timer_event_flag);

    if (osTimerStart(g_measure_timeout_timer, DEFAULT_MEASURE_TIMEOUT) != osOK) {
        TRACE_ERR("Failed to start measure timeout timer\n");

        g_reaction_test_state = eReactionTestState_Init;
    }

    return;
}

static void Reaction_Test_MeasureTimeoutTimer (void *arg) {
    if (g_reaction_test_state == eReactionTestState_Measure) {
        g_reaction_test_state = eReactionTestState_Init;

        //Reaction_Test_HandleGameError(eGameError_MeasureTimeout);

        osEventFlagsSet(g_timer_flag, DEFAULT_MEASURE_TIMEOUT_FLAG);
    }

    return;
}

static sModuleState_t Reaction_Test_IsModuleClear (const eModule_t module) {
    if (!Reaction_Test_IsCorrectModule(module)) {
        TRACE_ERR("Failed to check module [%d] state: Incorrect Module\n", module);

        return eModuleState_Last;
    }

    if ((g_dynamic_reaction_test_desc[module].state == eModuleState_Off) || (g_dynamic_reaction_test_desc[module].state == eModuleState_Default)) {
        TRACE_ERR("Failed to check module [%d] state: Module state [%d]\n", module, g_dynamic_reaction_test_desc[module].state);

        return eModuleState_Last;
    }

    return eModuleState_Ready;

    if (!VL53L0X_API_GetDistance(g_static_reaction_test_desc[module].vl53l0x, &g_dynamic_reaction_test_desc[module].registerd_distance, DEFAULT_GET_DISTANCE_TIMEOUT)) {
        return eModuleState_FailedGetDistance;
    }

    // if ((g_dynamic_reaction_test_desc[module].registerd_distance > g_dynamic_reaction_test_desc[module].led_strip_length) || g_dynamic_reaction_test_desc[module].registerd_distance == 0) {
    //     return eModuleState_Ready;
    // } else {
    //     return eModuleState_Last;
    // }
}

/**********************************************************************************************************************
 * Definitions of exported functions
 *********************************************************************************************************************/

bool Reaction_Test_App_Init (void) {
    if (g_is_initialized) {
        return true;
    }

    if (g_start_button_event == NULL) {
        g_start_button_event = osEventFlagsNew(&g_start_button_event_attributes);
    }

    if (!IO_API_Init(START_STOP_BUTTON, g_start_button_event)) {
        return false;
    }

    if (!WS2812B_API_Init()) {
        return false;
    }

    if (eModule_Last == 0) {
        TRACE_ERR("Failed to init reaction test: No modules available\n");

        return false;
    }

    for (eModule_t module = eModule_First; module < eModule_Last; module++) {
        g_dynamic_reaction_test_desc[module].led_solid_color.rgb = LED_GetColorRgb(g_static_reaction_test_desc[module].base_color);

        g_dynamic_reaction_test_desc[module].led_segment_fill.rgb_base = LED_GetColorRgb(g_static_reaction_test_desc[module].base_color);
        g_dynamic_reaction_test_desc[module].led_segment_fill.rgb_segment = LED_GetColorRgb(g_static_reaction_test_desc[module].target_color);

        g_dynamic_reaction_test_desc[module].total_led_count = WS2812B_API_GetLedCount(g_static_reaction_test_desc[module].ws2812b);
        g_dynamic_reaction_test_desc[module].led_strip_length = (g_dynamic_reaction_test_desc[module].total_led_count * SINGLE_SEGMENT_LENGTH_UM) / 1000;

        if (g_dynamic_reaction_test_desc[module].segment_timer == NULL) {
            g_dynamic_reaction_test_desc[module].segment_timer = osTimerNew(Reaction_Test_DelayStartTimer, osTimerOnce, &g_dynamic_reaction_test_desc[module], &g_static_reaction_test_desc[module].segment_timer_attributes);
        }

        g_dynamic_reaction_test_desc[module].module = module;
    }

    if (g_reaction_test_thread_id == NULL) {
        g_reaction_test_thread_id = osThreadNew(Reaction_Test_Thread, NULL, &g_reaction_test_thread_attributes);
    }

    if (g_timer_flag == NULL) {
        g_timer_flag = osEventFlagsNew(&g_start_button_event_attributes);
    }

    if (g_measure_timeout_timer == NULL) {
        g_measure_timeout_timer = osTimerNew(Reaction_Test_MeasureTimeoutTimer, osTimerOnce, NULL, &g_measure_timeout_timer_attributes);
    }
    
    g_is_initialized = true;

    return true;
}

sModuleState_t Reaction_Test_App_GetModuleState (const eModule_t module) {
    if (!Reaction_Test_IsCorrectModule(module)) {
        TRACE_ERR("Failed to get module state: Incorrect Module\n");

        return eModuleState_Off;
    }

    return g_dynamic_reaction_test_desc[module].state;
}

bool Reaction_Test_App_UpdateModuleState (const eModule_t module, const sModuleState_t state) {
    if (!Reaction_Test_IsCorrectModule(module)) {
        TRACE_ERR("Failed to update module state: Incorrect Module\n");

        return false;
    }

    if (g_dynamic_reaction_test_desc[module].state != state) {
        g_dynamic_reaction_test_desc[module].state = state;
    }

    return true;
}

bool Reaction_Test_App_SetRandomTargetPossition (const eModule_t module_data) {
    if (!Reaction_Test_IsCorrectModule(module_data)) {
        return false;
    }

    uint32_t start_led = Math_Utils_RandomRange(0, g_dynamic_reaction_test_desc[module_data].total_led_count - g_dynamic_reaction_test_desc[module_data].target_led_count);

    uint32_t distance = (start_led * SINGLE_SEGMENT_LENGTH_UM) / 1000 + ((g_dynamic_reaction_test_desc[module_data].target_led_count / 2) * SINGLE_SEGMENT_LENGTH_UM) / 1000;

    if (distance < DEFAULT_HAND_OFFSET) {
        distance = DEFAULT_HAND_OFFSET / 2;
    } else {
        distance -= DEFAULT_HAND_OFFSET;
    }

    g_dynamic_reaction_test_desc[module_data].led_segment_fill.segment_start_led = start_led;
    g_dynamic_reaction_test_desc[module_data].led_segment_fill.segment_end_led = start_led + (g_dynamic_reaction_test_desc[module_data].target_led_count - 1);
    g_dynamic_reaction_test_desc[module_data].target_distance = distance;

    return true;
}

uint16_t Reaction_Test_App_GetTargetDistanceMm (const eModule_t module_data) {
    if (!Reaction_Test_IsCorrectModule(module_data)) {
        TRACE_ERR("Failed to activate [%d] module: Incorrect Module\n", module_data);

        return 0;
    }

    return g_dynamic_reaction_test_desc[module_data].target_distance;
}

bool Reaction_Test_App_ActiveteModule (const eModule_t module_data, const sModuleState_t state) {
    if (!Reaction_Test_IsCorrectModule(module_data)) {
        TRACE_ERR("Failed to activate [%d] module: Incorrect Module\n", module_data);

        return false;
    }

    if ((state < eModuleState_First) || (state >= eModuleState_Last) || (state == eModuleState_Off)) {
        TRACE_ERR("Failed to activate [%d] module: Incorrect state [%d]\n", module_data, state);

        return false;
    }

    if (g_dynamic_reaction_test_desc[module_data].state == state) {
        return true;
    }

    g_led_animation.device = g_static_reaction_test_desc[module_data].ws2812b;
    g_led_animation.animation = eLedAnimation_SolidColor;
    g_led_animation.data = &g_dynamic_reaction_test_desc[module_data].led_solid_color;

    if (!WS2812B_API_AddAnimation(&g_led_animation)) {
        //TRACE_ERR("Failed to activate [%d] module: WS2812B API Add Animation failed\n", module_data);

        return false;
    }

    if (!WS2812B_API_Start(g_static_reaction_test_desc[module_data].ws2812b)) {
        //TRACE_ERR("Failed to activate [%d] module: WS2812B API Start failed\n", module_data);

        return false;
    }

    switch (state) {
        case eModuleState_Default: {
            g_dynamic_reaction_test_desc[module_data].state = eModuleState_Default;
        } break;
        case eModuleState_Active: {
            if (!VL53L0X_API_StartMeasuring(g_static_reaction_test_desc[module_data].vl53l0x)) {
                //TRACE_ERR("Failed to enable vl53l0 on [%d] module\n", module);
        
                g_reaction_test_state = eReactionTestState_Init;
        
                return false;
            }

            g_dynamic_reaction_test_desc[module_data].state = eModuleState_Active;
        } break;

        default: {
            break;
        }
    }

    return true;
}

bool Reaction_Test_App_StartDelayTimer (const eModule_t module_data, const uint32_t delay) {
    if (!Reaction_Test_IsCorrectModule(module_data)) {
        TRACE_ERR("Failed to activate [%d] module: Incorrect Module\n", module_data);

        return false;
    }

    if (delay < MIN_START_DELAY || delay > MAX_START_DELAY) {
        TRACE_ERR("Failed to activate [%d] module: Delay [%d] incorrect\n", module_data, delay);

        return false;
    }

    if (g_dynamic_reaction_test_desc[module_data].segment_timer == NULL) {
        TRACE_ERR("Failed to activate [%d] module: Segment timer is NULL\n", module_data);

        return false;
    }

    if (osTimerStart(g_dynamic_reaction_test_desc[module_data].segment_timer, delay) != osOK) {
        TRACE_ERR("Failed to activate [%d] module: Failed to start segment timer\n", module_data);

        return false;
    }

    return true;
}

bool Reaction_Test_App_Display (void) {
    TRACE_INFO(g_message.data);

    g_display_message[0] = '\0';

    return true;
}

bool Reaction_Test_WaitForClear (const eModule_t module) {
    if (!Reaction_Test_IsCorrectModule(module)) {
        TRACE_ERR("Failed to wait for clear: Incorrect Module\n");

        return false;
    }

    if (g_dynamic_reaction_test_desc[module].state != eModuleState_Active) {
        TRACE_ERR("Failed to wait for clear: Module [%d] state [%d] incorrect\n", module, g_dynamic_reaction_test_desc[module].state);
        
        return false;
    }

    uint32_t start_time = osKernelGetSysTimerCount();
    uint32_t timeout = WAIT_CLEAR_TIME * SYSTEM_MS_TICS;
    sModuleState_t state;

    while ((osKernelGetSysTimerCount() - start_time) < timeout) {
        state = Reaction_Test_IsModuleClear(module);
        if (state == eModuleState_Ready) {
            g_dynamic_reaction_test_desc[module].state = eModuleState_Ready;

            return true;
        }

        TRACE_INFO("Distance: [%d]\n", g_dynamic_reaction_test_desc[module].registerd_distance);

        osDelay(5);
    }

    // if (state == eModuleState_FailedGetDistance) {
    //     TRACE_WRN("Failed to get distance\n");

    //     return true;
    // }

    TRACE_ERR("Failed to wait for clear: Timeout\n");
    
    return false;
}

void Reaction_Test_HandleGameError (eGameError_t error) {
    if ((error < eGameError_First) || (error >= eGameError_Last)) {
        TRACE_ERR("Failed to handle game error: Incorrect error code\n");

        return;
    }

    if (osTimerIsRunning(g_measure_timeout_timer)) {
        osTimerStop(g_measure_timeout_timer);
    }

    // TODO: Make leds blink

    g_led_animation.animation = eLedAnimation_SolidColor;

    sLedAnimationSolidColor_t led_error = {0};
    led_error.rgb = LED_GetColorRgb(ERROR_LED_COLOR);
    g_led_animation.data = &led_error;

    for (eModule_t module = eModule_First; module < eModule_Last; module++) {
        if (osTimerIsRunning(g_dynamic_reaction_test_desc[module].segment_timer)) {
            if (osTimerStop(g_dynamic_reaction_test_desc[module].segment_timer) != osOK) {
                TRACE_ERR("Failed to stop timer on [%d] module\n", module);
            }
        }

        g_led_animation.device = g_static_reaction_test_desc[module].ws2812b;

        if (!WS2812B_API_AddAnimation(&g_led_animation)) {
            //TRACE_ERR("Failed to activate [%d] module: WS2812B API Add Animation failed\n", module_data);
    
            return;
        }
    
        if (!WS2812B_API_Start(g_static_reaction_test_desc[module].ws2812b)) {
            //TRACE_ERR("Failed to activate [%d] module: WS2812B API Start failed\n", module_data);
    
            return;
        }
    }

    g_game_mode_instance.game_mode_reset(g_game_mode_instance.game_mode_data);

    snprintf(g_message.data, g_message.size, "Game Error [%d]\n", error);
    Reaction_Test_App_Display();

    g_reaction_test_state = eReactionTestState_Init;

    osDelay(5000);

    return;
}

bool Reaction_Test_IsCorrectModule (const eModule_t module) {
    return (module >= eModule_First) && (module < eModule_Last);
}
