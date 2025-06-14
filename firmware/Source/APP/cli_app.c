/**********************************************************************************************************************
 * Includes
 *********************************************************************************************************************/

#include "cli_app.h"
#include <ctype.h>
#include "cmsis_os2.h"
#include "cli_cmd_handlers.h"
#include "cmd_api.h"
#include "heap_api.h"
#include "debug_api.h"
#include "error_messages.h"

/**********************************************************************************************************************
 * Private definitions and macros
 *********************************************************************************************************************/

#define DEBUG_CLI_APP

#define RESPONSE_MESSAGE_CAPACITY 128
#define DEFINE_CMD(command_string) .command = command_string, .command_lenght = sizeof(command_string) - 1

/**********************************************************************************************************************
 * Private typedef
 *********************************************************************************************************************/

/**********************************************************************************************************************
 * Private constants
 *********************************************************************************************************************/

#ifdef DEBUG_CLI_APP
CREATE_MODULE_NAME (CLI_APP)
#else
CREATE_MODULE_NAME_EMPTY
#endif

const static osThreadAttr_t g_cli_thread_attributes = {
    .name = "CLI_APP_Thread",
    .stack_size = 256 * 4,
    .priority = (osPriority_t) osPriorityNormal
};

/**********************************************************************************************************************
 * Private variables
 *********************************************************************************************************************/

static bool g_is_initialized = false;

static osThreadId_t g_cli_thread_id = NULL;
static char g_response_buffer[RESPONSE_MESSAGE_CAPACITY];

static sMessage_t g_command = {.data = NULL, .size = 0};
static sMessage_t g_response = {.data = g_response_buffer, .size = RESPONSE_MESSAGE_CAPACITY};

/* clang-format off */
static sCmdDesc_t g_static_cli_lut[eCliCommand_Last] = {
    [eCliCommand_RgbToHsv] = {
        DEFINE_CMD("rgb:"),
        .handler = CLI_APP_Led_Handlers_RgbToHsv
    },
    [eCliCommand_HsvToRgb] = {
        DEFINE_CMD("hsv:"),
        .handler = CLI_APP_Led_Handlers_HsvToRgb
    }
};
/* clang-format on */

/**********************************************************************************************************************
 * Exported variables and references
 *********************************************************************************************************************/

/**********************************************************************************************************************
 * Prototypes of private functions
 *********************************************************************************************************************/

static void CLI_APP_Thread (void *arg);

/**********************************************************************************************************************
 * Definitions of private functions
 *********************************************************************************************************************/

static void CLI_APP_Thread (void *arg) {
    while (true) {
        if (UART_API_Receive(eUart_Debug, &g_command, osWaitForever)) {
            if (!CMD_API_FindCommand(g_command, &g_response, g_static_cli_lut, eCliCommand_Last)){
                TRACE_ERR(g_response.data);
            }

            Heap_API_Free(g_command.data);
        }
    }

    osThreadYield();
}

/**********************************************************************************************************************
 * Definitions of exported functions
 *********************************************************************************************************************/

bool CLI_APP_Init (const eUartBaudrate_t baudrate) {
    if (g_is_initialized) {
        return false;
    }
    
    if ((baudrate < eUartBaudrate_First) || (baudrate >= eUartBaudrate_Last)) {
        return false;
    }

    if (Heap_API_Init() == false) {
        return false;
    }

    if (Debug_API_Init(baudrate) == false) {
        return false;
    }

    if (g_cli_thread_id == NULL) {
        g_cli_thread_id = osThreadNew(CLI_APP_Thread, NULL, &g_cli_thread_attributes);
    }

    g_is_initialized = true;

    return g_is_initialized;
}
