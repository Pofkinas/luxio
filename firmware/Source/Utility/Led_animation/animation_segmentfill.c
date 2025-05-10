/**********************************************************************************************************************
 * Includes
 *********************************************************************************************************************/

#include "animation_segmentfill.h"

/**********************************************************************************************************************
 * Private definitions and macros
 *********************************************************************************************************************/

#define MAX_BRIGHTNESS 255

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

void Animation_SegmentFill_FillBuffer (const sSegmentFillData_t *data) {
    if (data == NULL) {
        return;
    }
    
    if (!WS2812B_API_IsCorrectDevice(data->device)) {
        return;
    }

    if (data->brightness == 0) {
        return;
    }

    uint8_t r_base = (data->base_rgb.color >> 16) & 0xFF;
    uint8_t g_base = (data->base_rgb.color >> 8) & 0xFF;
    uint8_t b_base = data->base_rgb.color & 0xFF;

    uint8_t r_segment = (data->segment_rgb.color >> 16) & 0xFF;
    uint8_t g_segment = (data->segment_rgb.color >> 8) & 0xFF;
    uint8_t b_segment = data->segment_rgb.color & 0xFF;

    if (r_base != 0 || g_base != 0 || b_base != 0) {
        r_base = (r_base * data->brightness) / MAX_BRIGHTNESS;
        g_base = (g_base * data->brightness) / MAX_BRIGHTNESS;
        b_base = (b_base * data->brightness) / MAX_BRIGHTNESS;

        WS2812B_API_FillColor(data->device, r_base, g_base, b_base);
    }

    r_segment = (r_segment * data->brightness) / MAX_BRIGHTNESS;
    g_segment = (g_segment * data->brightness) / MAX_BRIGHTNESS;
    b_segment = (b_segment * data->brightness) / MAX_BRIGHTNESS;

    if ((data->end_led - data->start_led) == 1) {
        WS2812B_API_SetColor(data->device, data->start_led, r_segment, g_segment, b_segment);
        
        return;
    }

    WS2812B_API_FillSegment(data->device, data->start_led, data->end_led, r_segment, g_segment, b_segment);

    return;
}

/**********************************************************************************************************************
 * Definitions of private functions
 *********************************************************************************************************************/
 
/**********************************************************************************************************************
 * Definitions of exported functions
 *********************************************************************************************************************/

void Animation_SegmentFill_Run (const void *context) {
    Animation_SegmentFill_FillBuffer((const sSegmentFillData_t *)context);
}

