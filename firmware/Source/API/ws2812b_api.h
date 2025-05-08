#ifndef SOURCE_API_WS2812B_API_H_
#define SOURCE_API_WS2812B_API_H_
/**********************************************************************************************************************
 * Includes
 *********************************************************************************************************************/

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "led_color.h"

/**********************************************************************************************************************
 * Exported definitions and macros
 *********************************************************************************************************************/

/**********************************************************************************************************************
 * Exported types
 *********************************************************************************************************************/

/* clang-format off */ 
typedef enum eWs2812b {
    eWs2812b_First = 0,
    eWs2812b_1 = eWs2812b_First,
    eWs2812b_Last
} eWs2812b_t;

typedef enum eLedAnimation {
    eLedAnimation_First = 0,
    eLedAnimation_Off = eLedAnimation_First,
    eLedAnimation_SolidColor,
    eLedAnimation_SegmentFill,
    eLedAnimation_Blink,
    eLedAnimation_Rainbow,
    eLedAnimation_Last
} eLedAnimation_t;

typedef enum eColorFormat {
    eColorFormat_First = 0,
    eColorFormat_RGB = eColorFormat_First,
    eColorFormat_HSV,
    eColorFormat_Last
} eColorFormat_t;

typedef struct sLedAnimationDesc {
    eWs2812b_t device;
    eLedAnimation_t animation;
    uint8_t brightness;
    void *data;
} sLedAnimationDesc_t;

typedef struct sLedAnimationCommon {
    bool is_loop;
} sLedAnimationCommon_t;

typedef struct sLedAnimationSolidColor {
    eColorFormat_t color_format;
    union {
        sLedColorRgb_t rgb;
        sLedColorHsv_t hsv;
    } led_color;
} sLedAnimationSolidColor_t;

typedef struct sLedAnimationSegmentFill {
    eColorFormat_t color_format;
    union {
        sLedColorRgb_t rgb;
        sLedColorHsv_t hsv;
    } base_color;
    union {
        sLedColorRgb_t rgb;
        sLedColorHsv_t hsv;
    } segment_color;
    size_t segment_start_led;
    size_t segment_end_led;
} sLedAnimationSegmentFill_t;
/* clang-format on */

/**********************************************************************************************************************
 * Exported variables
 *********************************************************************************************************************/

/**********************************************************************************************************************
 * Prototypes of exported functions
 *********************************************************************************************************************/

bool WS2812B_API_Init (void);
bool WS2812B_API_Execute (sLedAnimationDesc_t *animation_data);
bool WS2812B_API_Stop (const eWs2812b_t device);
bool WS2812B_API_SetColor (const void *device_context, size_t led_number, const uint8_t r, const uint8_t g, const uint8_t b);
bool WS2812B_API_FillColor (const void *device_context, const uint8_t r, const uint8_t g, const uint8_t b);
bool WS2812B_API_FillSegment (const void *device_context, const size_t start_led, const size_t end_led, const uint8_t r, const uint8_t g, const uint8_t b);

#endif /* SOURCE_API_WS2812B_API_H_ */
