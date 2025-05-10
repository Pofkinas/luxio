/**********************************************************************************************************************
 * Includes
 *********************************************************************************************************************/

#include "animation_rainbow.h"

/**********************************************************************************************************************
 * Private definitions and macros
 *********************************************************************************************************************/

#define MAX_HUE 255
#define MAX_SPEED 64

/**********************************************************************************************************************
 * Private typedef
 *********************************************************************************************************************/

/**********************************************************************************************************************
 * Private constants
 *********************************************************************************************************************/

/**********************************************************************************************************************
 * Private variables
 *********************************************************************************************************************/

/**********************************************************************************************************************
 * Exported variables and references
 *********************************************************************************************************************/

/**********************************************************************************************************************
 * Prototypes of private functions
 *********************************************************************************************************************/

void Animation_Rainbow_FillBuffer (sLedRainbow_t *context);

/**********************************************************************************************************************
 * Definitions of private functions
 *********************************************************************************************************************/

void Animation_Rainbow_FillBuffer (sLedRainbow_t *context) {
    if (context == NULL) {
        return;
    }

    if (context->animation_data == NULL) {
        return;
    }
    
    if (!WS2812B_API_IsCorrectDevice(context->device)) {
        return;
    }

    if (context->brightness == 0) {
        return;
    }

    sLedAnimationRainbow_t *rainbow_data = context->animation_data;

    if (rainbow_data->segment_start_led >= rainbow_data->segment_end_led) {
        return;
    }

    size_t led_count = rainbow_data->segment_end_led - rainbow_data->segment_start_led;

    switch (context->state) {
        case eRainbowState_Init: {
            context->hue_step = MAX_HUE / led_count;
            context->hue_range = (rainbow_data->end_hsv_color.hue - rainbow_data->start_hsv_color.hue);
            context->state = eRainbowState_Run;
            context->base_hue = 0;
        }
        case eRainbowState_Run: {
            sLedColorHsv_t hsv = {0};
            sLedColorRgb_t rgb = {0};

            hsv.saturation = rainbow_data->start_hsv_color.saturation;
            hsv.value = rainbow_data->start_hsv_color.value;

            if (rainbow_data->speed > MAX_SPEED) {
                rainbow_data->speed = MAX_SPEED;
            }

            uint8_t red;
            uint8_t green;
            uint8_t blue;

            for (size_t led = 0; led < led_count; led++) {
                hsv.hue = (context->base_hue + led * context->hue_step) % context->hue_range + rainbow_data->start_hsv_color.hue;

                LED_HsvToRgb(hsv, &rgb);
                
                red = LED_ScaleBrightness(((rgb.color >> 16) & 0xFF), context->brightness);
                green = LED_ScaleBrightness(((rgb.color >> 8) & 0xFF), context->brightness);
                blue = LED_ScaleBrightness((rgb.color & 0xFF), context->brightness);

                if (!WS2812B_API_SetColor(context->device, rainbow_data->segment_start_led + led, red, green, blue)) {
                    context->state = eRainbowState_Init;
                    
                    return;
                }
            }

            context->base_hue = (context->base_hue + rainbow_data->speed) % context->hue_range;
        } break;
        default: {
            return;
        }
    }
}

/**********************************************************************************************************************
 * Definitions of exported functions
 *********************************************************************************************************************/

void Animation_Rainbow_Run (void *context) {
    if (context == NULL) {
        return;
    }

    Animation_Rainbow_FillBuffer((sLedRainbow_t*) context);

    return;
}
