/**********************************************************************************************************************
 * Includes
 *********************************************************************************************************************/

#include "game_mode_classic.h"

#include <string.h>
#include <math.h>
#include "ws2812b_api.h"
#include "lcd_api.h"
#include "debug_api.h"
#include "heap_api.h"
#include "framework_config.h"
#include "math_utils.h"
#include "message.h"

/**********************************************************************************************************************
 * Private definitions and macros
 *********************************************************************************************************************/

#define DEBUG_GAME_MODE_CLASSIC

/**********************************************************************************************************************
 * Private typedef
 *********************************************************************************************************************/

typedef struct sGameModeClassicData {
    uint8_t attempt;
    uint16_t target_distance;
    uint8_t current_accuracy;
    uint16_t current_reaction_time;
    uint16_t average_accuracy;
    uint16_t average_reaction_time;
} sGameModeClassicData_t;

/**********************************************************************************************************************
 * Private constants
 *********************************************************************************************************************/
 
#ifdef DEBUG_GAME_MODE_CLASSIC
CREATE_MODULE_NAME (GAME_MODE_CLASSIC)
#else
CREATE_MODULE_NAME_EMPTY
#endif

/**********************************************************************************************************************
 * Private variables
 *********************************************************************************************************************/
 
static eModule_t *g_active_modules_index = NULL;
static uint8_t g_active_modules_count = 0;

/**********************************************************************************************************************
 * Exported variables and references
 *********************************************************************************************************************/

/**********************************************************************************************************************
 * Prototypes of private functions
 *********************************************************************************************************************/
 
/**********************************************************************************************************************
 * Definitions of private functions
 *********************************************************************************************************************/
 
/**********************************************************************************************************************
 * Definitions of exported functions
 *********************************************************************************************************************/

bool Game_Mode_Classic_Start (void *context) {
    if (context == NULL) {
        return false;
    }

    sGameModeClassic_t *game_mode = (sGameModeClassic_t *) context;

    if (game_mode->difficulty > eModule_Last) {
        TRACE_ERR("Game mode difficulty [%d] is greater than max difficulty [%d]\n", game_mode->difficulty, eModule_Last);

        return false;
    }

    if (game_mode->game_mode_data == NULL) {
        game_mode->game_mode_data = Heap_API_Calloc(1 ,sizeof(sGameModeClassicData_t));
    }

    if (game_mode->game_mode_data == NULL) {
        return false;
    }

    sGameModeClassicData_t *data = (sGameModeClassicData_t*) game_mode->game_mode_data;

    if (g_active_modules_index == NULL) {
        g_active_modules_index = Heap_API_Calloc(game_mode->difficulty, sizeof(eModule_t));
    }
    
    if (g_active_modules_index == NULL) {
        return false;
    }

    uint8_t active_modules = 0;

    if (game_mode->difficulty > eModule_Last) {
        active_modules = eModule_Last;
    } else {
        active_modules = game_mode->difficulty;
    }

    g_active_modules_count = active_modules;

    for (eModule_t module = eModule_First; module < eModule_Last; module++) {
        if (!Reaction_Test_App_ActiveteModule(module, eModuleState_Default)) {
            TRACE_ERR("Failed to active [%d] module\n", module);

            return false;
        }
    }

    while (active_modules > 0) {
        uint32_t module_index = Math_Utils_RandomRange(0, eModule_Last);

        if (!Reaction_Test_App_SetRandomTargetPossition(module_index)) {
            continue;
        }

        data->target_distance = Reaction_Test_App_GetTargetDistanceMm(module_index);
        
        g_active_modules_index[game_mode->difficulty - active_modules] = module_index;

        if (!Reaction_Test_App_ActiveteModule(module_index, eModuleState_Active)) {
            TRACE_ERR("Failed to active [%d] module\n", module_index);

            continue;
        }

        if (!Reaction_Test_WaitForClear(module_index)) {
            Reaction_Test_HandleGameError(eGameError_ClearStripTimeout);

            return false;
        }

        uint32_t start_delay = Math_Utils_RandomRange(MIN_START_DELAY, MAX_START_DELAY);

        if (!Reaction_Test_App_StartDelayTimer(module_index, start_delay)) {
            return false;
        }

        active_modules --;
    }

    return true;
}

void Game_Mode_Classic_Process (void *context) {
    if (context == NULL) {
        return;
    }

    sGameModeClassic_t *game_mode = (sGameModeClassic_t *) context;
    sGameModeClassicData_t *data = (sGameModeClassicData_t*) game_mode->game_mode_data;

    if (data == NULL) {
        return;
    }

    data->current_reaction_time = (game_mode->end_time - game_mode->start_time);

    uint32_t spacial_error = abs(data->target_distance - game_mode->registerd_distance);

    if (spacial_error > DEFAULT_DISTANCE_THRESHOLD_MM) {
        data->current_accuracy = exp(-(pow((spacial_error - DEFAULT_DISTANCE_THRESHOLD_MM), 2)) / pow((2 * ACCURACY_SIGMA), 2)) * 100;
    } else {
        data->current_accuracy = 100;
    }

    data->average_accuracy += data->current_accuracy;
    data->average_reaction_time += data->current_reaction_time;

    char uart_message[UART_MESSAGE_SIZE];
    char lcd_message[LCD_MESSAGE_SIZE + 1];

    sMessage_t message = {0};

    snprintf(uart_message, UART_MESSAGE_SIZE, "Time: %d ms, Target: %d mm, Reg: %d mm, Acc: %d\n", data->current_reaction_time, data->target_distance, game_mode->registerd_distance, data->current_accuracy);
    message.data = uart_message;

    Reaction_Test_App_DisplayUart(message);

    snprintf(lcd_message, LCD_MESSAGE_SIZE + 1, "Time: %d ms", data->current_reaction_time);
    message.data = lcd_message;
    message.size = strlen(message.data);

    Reaction_Test_App_DisplayLcd(message, eLcdRow_1, eLcdColumn_1, eLcdOption_None);

    snprintf(lcd_message, LCD_MESSAGE_SIZE + 1, "Acc: %d", data->current_accuracy);
    message.data = lcd_message;
    message.size = strlen(message.data);

    Reaction_Test_App_DisplayLcd(message, eLcdRow_2, eLcdColumn_1, eLcdOption_None);

    return;
}

bool Game_Mode_Classic_IsRestart (void *context) {
    if (context == NULL) {
        return false;
    }

    sGameModeClassic_t *game_mode = (sGameModeClassic_t *) context;
    sGameModeClassicData_t *data = (sGameModeClassicData_t*) game_mode->game_mode_data;

    data->attempt++;

    return (data->attempt < game_mode->total_attempts);
}

void Game_Mode_Classic_Stop (void *context) {
    if (context == NULL) {
        return;
    }

    sGameModeClassic_t *game_mode = (sGameModeClassic_t *) context;
    sGameModeClassicData_t *data = (sGameModeClassicData_t*) game_mode->game_mode_data;

    if (data == NULL) {
        return;
    }

    data->average_accuracy /= game_mode->total_attempts;
    data->average_reaction_time /= game_mode->total_attempts;

    char uart_message[UART_MESSAGE_SIZE];
    char lcd_message[LCD_MESSAGE_SIZE + 1];

    sMessage_t message = {0};

    snprintf(uart_message, UART_MESSAGE_SIZE, "Average reaction time: %d ms\n", data->average_reaction_time);
    message.data = uart_message;

    Reaction_Test_App_DisplayUart(message);

    snprintf(uart_message, UART_MESSAGE_SIZE, "Average accuracy: %d\n", data->average_accuracy);
    message.data = uart_message;

    Reaction_Test_App_DisplayUart(message);

    LCD_API_Clear(eLcd_1);

    snprintf(lcd_message, LCD_MESSAGE_SIZE + 1, "Avg time %4dms", data->average_reaction_time);
    message.data = lcd_message;
    message.size = strlen(message.data);

    Reaction_Test_App_DisplayLcd(message, eLcdRow_1, eLcdColumn_1, eLcdOption_None);

    snprintf(lcd_message, LCD_MESSAGE_SIZE + 1, "Avg acc: %d", data->average_accuracy);
    message.data = lcd_message;
    message.size = strlen(message.data);

    Reaction_Test_App_DisplayLcd(message, eLcdRow_2, eLcdColumn_1, eLcdOption_None);

    return;
}

void Game_Mode_Classic_Reset (void *context) {
    if (context == NULL) {
        return;
    }

    sGameModeClassic_t *game_mode = (sGameModeClassic_t *) context;

    if (game_mode->game_mode_data != NULL) {
        Heap_API_Free(game_mode->game_mode_data);
        game_mode->game_mode_data = NULL;
    }

    if (g_active_modules_index != NULL) {
        Heap_API_Free(g_active_modules_index);
        g_active_modules_index = NULL;
    }

    return;
}

eModule_t *Game_Mode_Classic_GetActiveModules (uint8_t *active_modules_count) {
    if (active_modules_count == NULL) {
        return NULL;
    }

    if (g_active_modules_index == NULL) {
        *active_modules_count = 0;

        return NULL;
    }

    *active_modules_count = g_active_modules_count; 
    
    return g_active_modules_index;
}
