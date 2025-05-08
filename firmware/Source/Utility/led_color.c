/**********************************************************************************************************************
 * Includes
 *********************************************************************************************************************/

#include "led_color.h"

/**********************************************************************************************************************
 * Private definitions and macros
 *********************************************************************************************************************/

/**********************************************************************************************************************
 * Private typedef
 *********************************************************************************************************************/

/**********************************************************************************************************************
 * Private constants
 *********************************************************************************************************************/
 
const static uint32_t g_static_led_color_lut[eLedColor_Last] = {
    [eLedColor_Off] = 0x000000,
    [eLedColor_Red] = 0xFF0000,
    [eLedColor_Green] = 0x00FF00,
    [eLedColor_Blue] = 0x0000FF,
    [eLedColor_Yellow] = 0xFFFF00,
    [eLedColor_Cyan] = 0x00FFFF,
    [eLedColor_Magenta] = 0xFF00FF,
    [eLedColor_White] = 0xFFFFFF
};

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

const sLedColorRgb_t LED_GetColorRgb (const eLedColor_t color) {
    sLedColorRgb_t led_color = {0};
    
    if ((color < eLedColor_First) || (color >= eLedColor_Last)) {
        return led_color;
    }

    led_color.color = g_static_led_color_lut[color];

    return led_color;
}

void LED_HsvToRgb(sLedColorHsv_t *hsv) {
    sLedColorRgb_t *rgb_color = (sLedColorRgb_t *) hsv;

    uint8_t h = hsv->hue;
    uint8_t s = hsv->saturation;
    uint8_t v = hsv->value;

    uint8_t r, g, b;

    if (s == 0) {
        r = v;
        g = v;
        b = v;
    } else {
        uint8_t region = h / 43;
        uint8_t remainder = (h - region * 43) * 6;

        uint8_t p = (v * (255 - s)) >> 8;
        uint8_t q = (v * (255 - ((s * remainder) >> 8))) >> 8;
        uint8_t t = (v * (255 - ((s * (255 - remainder)) >> 8))) >> 8;

        switch (region) {
            case 0: {
                r = v;
                g = t;
                b = p;
            } break;
            case 1: {
                r = q;
                g = v;
                b = p;
            } break;
            case 2: {
                r = p;
                g = v;
                b = t;
            } break;
            case 3: {
                r = p;
                g = q;
                b = v;
            } break;
            case 4: {
                r = t;
                g = p;
                b = v;
            } break;
            default: {
                r = v;
                g = p;
                b = q;
            } break;
        }
    }

    rgb_color->color = ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
}

void LED_RgbToHsv(sLedColorRgb_t *rgb) {
    sLedColorHsv_t *hsv_color = (sLedColorHsv_t *) rgb;

    uint8_t r = (rgb->color >> 16) & 0xFF;
    uint8_t g = (rgb->color >> 8) & 0xFF;
    uint8_t b = rgb->color & 0xFF;

    uint8_t rgb_min = r < g ? (r < b ? r : b) : (g < b ? g : b);
    uint8_t rgb_max = r > g ? (r > b ? r : b) : (g > b ? g : b);
    
    uint8_t delta = rgb_max - rgb_min;

    hsv_color->value = rgb_max;

    if (rgb_max == 0) {
        hsv_color->saturation = 0;
        hsv_color->hue = 0;
        return;
    }

    hsv_color->saturation = (delta * 255) / rgb_max;

    if (delta == 0) {
        hsv_color->hue = 0;
        return;
    }

    int16_t hue;

    if (rgb_max == r) {
        hue = 0 + 43 * (g - b) / delta;
    } else if (rgb_max == g) {
        hue = 85 + 43 * (b - r) / delta;
    } else {
        hue = 171 + 43 * (r - g) / delta;
    }

    if (hue < 0) hue += 256;

    hsv_color->hue = (uint8_t)hue;

    return;
}
