#ifndef SOURCE_APP_REACTION_TEST_APP_H_
#define SOURCE_APP_REACTION_TEST_APP_H_
/**********************************************************************************************************************
 * Includes
 *********************************************************************************************************************/

#include <stdbool.h>
#include <stdint.h>

/**********************************************************************************************************************
 * Exported definitions and macros
 *********************************************************************************************************************/

#define MAX_DIFFICULTY 5
#define MIN_START_DELAY 500
#define MAX_START_DELAY 5000

#define DEFAULT_HAND_OFFSET 50
#define DEFAULT_DISTANCE_THRESHOLD_MM 10
#define ACCURACY_SIGMA 100

/**********************************************************************************************************************
 * Exported types
 *********************************************************************************************************************/

/* clang-format off */
typedef enum eModule {
    eModule_First = 0,
    eModule_1 = eModule_First,
    eModule_2,
    eModule_Last
} eModule_t;

typedef enum sModuleState {
    eModuleState_First = 0,
    eModuleState_Off = eModuleState_First,
    eModuleState_Default,
    eModuleState_Active,
    eModuleState_Ready,
    eModuleState_FailedGetDistance,
    eModuleState_Measuring,
    eModuleState_Registered,
    eModuleState_Last
} sModuleState_t;

typedef enum eGameError {
    eGameError_First = 0,
    eGameError_ClearStripTimeout = eGameError_First,
    eGameError_InvalidStart,
    eGameError_MeasureTimeout,
    eGameError_Last
} eGameError_t;

typedef struct sGameModeInstance {
    void *game_mode_data;
    bool (*game_mode_start)(void *context);
    void (*game_mode_process)(void *context);
    bool (*game_mode_is_restart)(void *context);
    void (*game_mode_stop)(void *context);
    void (*game_mode_reset)(void *context);
    eModule_t* (*get_active_modules)(uint8_t *active_modules_count);
} sGameModeInstance_t;
/* clang-format on */

/**********************************************************************************************************************
 * Exported variables
 *********************************************************************************************************************/

/**********************************************************************************************************************
 * Prototypes of exported functions
 *********************************************************************************************************************/

bool Reaction_Test_App_Init (void);
sModuleState_t Reaction_Test_App_GetModuleState (const eModule_t module);
bool Reaction_Test_App_UpdateModuleState (const eModule_t module, const sModuleState_t state);
bool Reaction_Test_App_SetRandomTargetPossition (const eModule_t module_data);
uint16_t Reaction_Test_App_GetTargetDistanceMm (const eModule_t module_data);
bool Reaction_Test_App_ActiveteModule (const eModule_t module_data, const sModuleState_t state);
bool Reaction_Test_App_StartDelayTimer (const eModule_t module_data, const uint32_t delay);
bool Reaction_Test_App_Display (void);
bool Reaction_Test_WaitForClear (const eModule_t module);
void Reaction_Test_HandleGameError (eGameError_t error);
bool Reaction_Test_IsCorrectModule (const eModule_t module);

#endif /* SOURCE_APP_REACTION_TEST_APP_H_ */
