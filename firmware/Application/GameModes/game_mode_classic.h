#ifndef SOURCE_APP_GAMEMODES_GAME_MODE_CLASSIC_H_
#define SOURCE_APP_GAMEMODES_GAME_MODE_CLASSIC_H_
/**********************************************************************************************************************
 * Includes
 *********************************************************************************************************************/

#include <stdbool.h>
#include <stdint.h>
#include "reaction_test_app.h"
#include "message.h"

/**********************************************************************************************************************
 * Exported definitions and macros
 *********************************************************************************************************************/

/**********************************************************************************************************************
 * Exported types
 *********************************************************************************************************************/

/* clang-format off */
typedef struct sGameModeClassic {
    sMessage_t display_message;
    uint8_t difficulty;
    uint8_t total_attempts;
    uint32_t start_time;
    uint32_t end_time;
    uint16_t registerd_distance;
    void *game_mode_data;
} sGameModeClassic_t;
/* clang-format on */
 
/**********************************************************************************************************************
 * Exported variables
 *********************************************************************************************************************/

/**********************************************************************************************************************
 * Prototypes of exported functions
 *********************************************************************************************************************/

bool Game_Mode_Classic_Start (void *context);
void Game_Mode_Classic_Process (void *context);
void Game_Mode_Classic_Results (void *context);
bool Game_Mode_Classic_IsRestart (void *context);
void Game_Mode_Classic_Stop (void *context);
void Game_Mode_Classic_Reset (void *context);
eModule_t *Game_Mode_Classic_GetActiveModules (uint8_t *active_modules_count);
 
#endif /* SOURCE_APP_GAMEMODES_GAME_MODE_CLASSIC_H_ */
