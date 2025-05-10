/**********************************************************************************************************************
 * Includes
 *********************************************************************************************************************/

#include "cli_cmd_handlers.h"
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "cmd_api_helper.h"
#include "heap_api.h"
#include "debug_api.h"
#include "error_messages.h"

#include "led_color.h"

/**********************************************************************************************************************
 * Private definitions and macros
 *********************************************************************************************************************/

#define DEBUG_CLI_APP

#define CMD_SEPARATOR ","
#define CMD_SEPARATOR_LENGHT (sizeof(CMD_SEPARATOR) - 1)

/**********************************************************************************************************************
 * Private typedef
 *********************************************************************************************************************/

/**********************************************************************************************************************
 * Private constants
 *********************************************************************************************************************/

#ifdef DEBUG_CLI_APP
CREATE_MODULE_NAME (CLI_CMD_HANDLERS)
#else
CREATE_MODULE_NAME_EMPTY
#endif

/**********************************************************************************************************************
 * Private variables
 *********************************************************************************************************************/

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

bool CLI_APP_Led_Handlers_RgbToHsv (sMessage_t arguments, sMessage_t *response) {
    if (response == NULL) {
        TRACE_ERR("Invalid data pointer\n");

        return false;
    }

    if ((response->data == NULL)) {
        TRACE_ERR("Invalid response data pointer\n");

        return false;
    }

    size_t red = 0;
    size_t green = 0;
    size_t blue = 0;

    if (CMD_API_Helper_FindNextArgUInt(&arguments, &red, CMD_SEPARATOR, CMD_SEPARATOR_LENGHT, response) != eErrorCode_OSOK) {
        return false;
    }

    if (CMD_API_Helper_FindNextArgUInt(&arguments, &green, CMD_SEPARATOR, CMD_SEPARATOR_LENGHT, response) != eErrorCode_OSOK) {
        return false;
    }

    if (CMD_API_Helper_FindNextArgUInt(&arguments, &blue, CMD_SEPARATOR, CMD_SEPARATOR_LENGHT, response) != eErrorCode_OSOK) {
        return false;
    }
    
    if (arguments.size != 0) {
        snprintf(response->data, response->size, "Too many arguments\n");

        return false;
    }

    if ((red > 255) || (green > 255) || (blue > 255)) {
        snprintf(response->data, response->size, "Invalid RGB values\n");

        return false;
    }

    sLedColorRgb_t rgb = {0};
    sLedColorHsv_t hsv = {0};
    rgb.color = (red << 16) | (green << 8) | blue;

    LED_RgbToHsv(rgb, &hsv);

    TRACE_INFO("hue: %d, sat: %d, val: %d\n", hsv.hue, hsv.saturation, hsv.value);

    snprintf(response->data, response->size, "Operation successful\n");

    return true;
}

bool CLI_APP_Led_Handlers_HsvToRgb (sMessage_t arguments, sMessage_t *response) {
    if (response == NULL) {
        TRACE_ERR("Invalid data pointer\n");

        return false;
    }

    if ((response->data == NULL)) {
        TRACE_ERR("Invalid response data pointer\n");

        return false;
    }

    size_t hue = 0;
    size_t saturation = 0;
    size_t value = 0;

    if (CMD_API_Helper_FindNextArgUInt(&arguments, &hue, CMD_SEPARATOR, CMD_SEPARATOR_LENGHT, response) != eErrorCode_OSOK) {
        return false;
    }

    if (CMD_API_Helper_FindNextArgUInt(&arguments, &saturation, CMD_SEPARATOR, CMD_SEPARATOR_LENGHT, response) != eErrorCode_OSOK) {
        return false;
    }

    if (CMD_API_Helper_FindNextArgUInt(&arguments, &value, CMD_SEPARATOR, CMD_SEPARATOR_LENGHT, response) != eErrorCode_OSOK) {
        return false;
    }
    
    if (arguments.size != 0) {
        snprintf(response->data, response->size, "Too many arguments\n");

        return false;
    }

    if ((hue > 255) || (saturation > 255) || (value > 255)) {
        snprintf(response->data, response->size, "Invalid RGB values\n");

        return false;
    }

    sLedColorHsv_t hsv = {0};
    sLedColorRgb_t rgb = {0};

    hsv.hue = hue;
    hsv.saturation = saturation;
    hsv.value = value;

    LED_HsvToRgb(hsv, &rgb);

    TRACE_INFO("red: %d, green: %d, blue: %d\n", (rgb.color >> 16) & 0xFF, (rgb.color >> 8) & 0xFF, rgb.color & 0xFF);
    
    snprintf(response->data, response->size, "Operation successful\n");

    return true;
}
