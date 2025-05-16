#ifndef SOURCE_UTILITY_LED_COLOR_H_
#define SOURCE_UTILITY_LED_COLOR_H_
/**********************************************************************************************************************
 * Includes
 *********************************************************************************************************************/

#include <stdint.h>

/**********************************************************************************************************************
 * Exported definitions and macros
 *********************************************************************************************************************/

#define MAX_BRIGHTNESS 255

/**********************************************************************************************************************
 * Exported types
 *********************************************************************************************************************/

/* clang-format off */
typedef enum eLedColor {
    eLedColor_First = 0,
    eLedColor_Off = eLedColor_First,
    eLedColor_Red,
    eLedColor_Green,
    eLedColor_Blue,
    eLedColor_Yellow,
    eLedColor_Cyan,
    eLedColor_Magenta,
    eLedColor_White,
    eLedColor_Last
} eLedColor_t;

typedef struct sLedColorRgb {
    uint32_t color;
} sLedColorRgb_t;

typedef struct sLedColorHsv {
    uint8_t hue;
    uint8_t saturation;
    uint8_t value;
} sLedColorHsv_t;
/* clang-format on */

/**********************************************************************************************************************
 * Exported variables
 *********************************************************************************************************************/

/**********************************************************************************************************************
 * Prototypes of exported functions
 *********************************************************************************************************************/

const sLedColorRgb_t LED_GetColorRgb (const eLedColor_t color);
const sLedColorHsv_t LED_GetColorHsv (const eLedColor_t color);
void LED_HsvToRgb (const sLedColorHsv_t hsv, sLedColorRgb_t *rgb);
void LED_RgbToHsv (const sLedColorRgb_t rgb, sLedColorHsv_t *hsv);
uint8_t LED_ScaleBrightness (const uint8_t value, const uint8_t brightness);

#endif /* SOURCE_UTILITY_LED_COLOR_H_ */
