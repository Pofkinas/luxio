/**********************************************************************************************************************
 * Includes
 *********************************************************************************************************************/

#include "i2c_driver.h"
#include "stm32f4xx_ll_bus.h"
#include "stm32f4xx_ll_i2c.h"
#include "stm32f4xx_ll_gpio.h"
#include "gpio_driver.h"

/**********************************************************************************************************************
 * Private definitions and macros
 *********************************************************************************************************************/

#define GENERAL_CALL_ADDRESS 0x7F

/**********************************************************************************************************************
 * Private typedef
 *********************************************************************************************************************/

typedef enum eI2cDriverState {
    eI2cDriverState_First = 0,
    eI2cDriverState_Off = eI2cDriverState_First,
    eI2cDriverState_Init,
    eI2cDriverState_Reset,
    eI2cDriverState_Last
} eI2cDriverState_t;

typedef struct sI2cDesc {
    I2C_TypeDef *periph;
    uint32_t peripheral_mode;
    uint32_t clock_speed;
    uint32_t duty_cycle;
    uint32_t analog_filter;
    uint32_t digital_filter;
    uint32_t own_address1;
    uint32_t own_address2;
    uint32_t type_acknowledge;
    uint32_t own_addr_size;
    uint32_t clock;
    void (*enable_clock_fp) (uint32_t);
    bool is_enabled_it;
    IRQn_Type nvic;
    eGpioPin_t scl_pin;
    eGpioPin_t sda_pin;
} sI2cDesc_t;

typedef struct sI2cDynamicDesc {
    eI2cDriverState_t state;
    uint8_t address;
    uint8_t rw_operation;
    i2c_callback_t flag_callback;
    void *callback_context;
} sI2cDynamicDesc_t;

/**********************************************************************************************************************
 * Private constants
 *********************************************************************************************************************/

/* clang-format off */
static const sI2cDesc_t g_static_i2c_lut[eI2cDriver_Last] = {
    [eI2cDriver_1] = {
        .periph = I2C1,
        .peripheral_mode = LL_I2C_MODE_I2C,
        .clock_speed = 100000,
        .duty_cycle = LL_I2C_DUTYCYCLE_2,
        .own_address1 = 0,
        .own_address2 = 0,
        .type_acknowledge = LL_I2C_ACK,
        .own_addr_size = LL_I2C_OWNADDRESS1_7BIT,
        .clock = LL_APB1_GRP1_PERIPH_I2C1,
        .enable_clock_fp = LL_APB1_GRP1_EnableClock,
        .is_enabled_it = true,
        .nvic = I2C1_EV_IRQn,
        .scl_pin = eGpioPin_I2c1_SCL,
        .sda_pin = eGpioPin_I2c1_SDA
    }
};
/* clang-format on */

/**********************************************************************************************************************
 * Private variables
 *********************************************************************************************************************/

/* clang-format off */
static sI2cDynamicDesc_t g_dynamic_i2c_lut[eI2cDriver_Last] = {
    [eI2cDriver_1] = {
        .state = eI2cDriverState_Off,
        .address = 0,
        .rw_operation = 0,
        .flag_callback = NULL,
        .callback_context = NULL
    }
};
/* clang-format on */

/**********************************************************************************************************************
 * Exported variables and references
 *********************************************************************************************************************/

/**********************************************************************************************************************
 * Prototypes of private functions
 *********************************************************************************************************************/

void I2C1_EV_IRQHandler (void);
void I2C_Driver_ClearAllFlags (const eI2cDriver_t i2c);

/**********************************************************************************************************************
 * Definitions of private functions
 *********************************************************************************************************************/

void I2C1_EV_IRQHandler (void) {
    eI2cDriver_Flags_t flag = eI2cDriver_Flags_Last;
    
    if (LL_I2C_IsActiveFlag_SB(I2C1)) {
        LL_I2C_TransmitData8(I2C1, (uint8_t)((g_dynamic_i2c_lut[eI2cDriver_1].address << 1) | (g_dynamic_i2c_lut[eI2cDriver_1].rw_operation & 0x1)));

        if (g_dynamic_i2c_lut[eI2cDriver_1].state != eI2cDriverState_Reset) {
            return; 
        }

        g_dynamic_i2c_lut[eI2cDriver_1].state = eI2cDriverState_Init;

        I2C_Driver_StopComms(eI2cDriver_1);
        I2C_Driver_DisableIt(eI2cDriver_1);

        I2C_Driver_ClearFlag(eI2cDriver_1, eI2cDriver_Flags_AckFailure);

        flag = eI2cDriver_Flags_BusReset;
    } else if (LL_I2C_IsActiveFlag_ADDR(I2C1)) {
        flag = eI2cDriver_Flags_Addr;
    } else if (LL_I2C_IsActiveFlag_TXE(I2C1)) {
        flag = eI2cDriver_Flags_Txe;
    } else if (LL_I2C_IsActiveFlag_BTF(I2C1)) {
        flag = eI2cDriver_Flags_ByteTransferFinish;
    } else if (LL_I2C_IsActiveFlag_RXNE(I2C1)) {
        flag = eI2cDriver_Flags_Rxne;
    } else if (LL_I2C_IsActiveFlag_AF(I2C1)) {
        LL_I2C_ClearFlag_AF(I2C1);

        flag = eI2cDriver_Flags_AckFailure;
    } else if (LL_I2C_IsActiveFlag_BERR(I2C1)) {
        NVIC_DisableIRQ(I2C1_EV_IRQn);

        LL_I2C_ClearFlag_BERR(I2C1);
        I2C_Driver_ResetLine(eI2cDriver_1);

        NVIC_EnableIRQ(I2C1_EV_IRQn);

        flag = eI2cDriver_Flags_BusError;
    }

    if (g_dynamic_i2c_lut[eI2cDriver_1].flag_callback != NULL) {
        g_dynamic_i2c_lut[eI2cDriver_1].flag_callback(flag, g_dynamic_i2c_lut[eI2cDriver_1].callback_context);
    }

    return;
}

void I2C_Driver_ClearAllFlags (const eI2cDriver_t i2c) {
    if ((i2c < eI2cDriver_First) || (i2c >= eI2cDriver_Last)) {
        return;
    }

    if (g_dynamic_i2c_lut[i2c].state == eI2cDriverState_Off){ 
        return;
    }
    
    if (LL_I2C_IsActiveFlag_AF(g_static_i2c_lut[i2c].periph)) {
        LL_I2C_ClearFlag_AF(g_static_i2c_lut[i2c].periph);
    } 
    if (LL_I2C_IsActiveFlag_ARLO(g_static_i2c_lut[i2c].periph)) {
        LL_I2C_ClearFlag_ARLO(g_static_i2c_lut[i2c].periph);
    } 
    if (LL_I2C_IsActiveFlag_BERR(g_static_i2c_lut[i2c].periph)) {
        LL_I2C_ClearFlag_BERR(g_static_i2c_lut[i2c].periph);
    }
    if (LL_I2C_IsActiveFlag_STOP(g_static_i2c_lut[i2c].periph)) {
        LL_I2C_ClearFlag_STOP(g_static_i2c_lut[i2c].periph);
    }
    if (LL_I2C_IsActiveFlag_ADDR(g_static_i2c_lut[i2c].periph)) {
        LL_I2C_ClearFlag_ADDR(g_static_i2c_lut[i2c].periph);
    }

    return;
}

/**********************************************************************************************************************
 * Definitions of exported functions
 *********************************************************************************************************************/

bool I2C_Driver_Init (const eI2cDriver_t i2c, i2c_callback_t flag_callback, void *context) {
    if ((i2c < eI2cDriver_First) || (i2c >= eI2cDriver_Last)) {
        return false;
    }

    if (flag_callback == NULL) {
        return false;
    }

    if (context == NULL) {
        return false;
    }
    
    if (g_dynamic_i2c_lut[i2c].state != eI2cDriverState_Off) { 
        return true;
    }

    LL_I2C_InitTypeDef i2c_init_struct = {0};

    i2c_init_struct.PeripheralMode = g_static_i2c_lut[i2c].peripheral_mode;
    i2c_init_struct.ClockSpeed = g_static_i2c_lut[i2c].clock_speed;
    i2c_init_struct.DutyCycle = g_static_i2c_lut[i2c].duty_cycle;
    i2c_init_struct.OwnAddress1 = g_static_i2c_lut[i2c].own_address1;
    i2c_init_struct.TypeAcknowledge = g_static_i2c_lut[i2c].type_acknowledge;
    i2c_init_struct.OwnAddrSize = g_static_i2c_lut[i2c].own_addr_size;

    g_static_i2c_lut[i2c].enable_clock_fp(g_static_i2c_lut[i2c].clock);

    if (LL_I2C_Init(g_static_i2c_lut[i2c].periph, &i2c_init_struct) == ERROR) {
        return false;
    }

    LL_I2C_Disable(g_static_i2c_lut[i2c].periph);

    LL_I2C_DisableOwnAddress2(g_static_i2c_lut[i2c].periph);
    LL_I2C_DisableGeneralCall(g_static_i2c_lut[i2c].periph);
    LL_I2C_EnableClockStretching(g_static_i2c_lut[i2c].periph);

    if (g_static_i2c_lut[i2c].is_enabled_it) {
        NVIC_SetPriority(g_static_i2c_lut[i2c].nvic, NVIC_EncodePriority(NVIC_GetPriorityGrouping(), 5, 0));
        NVIC_EnableIRQ(g_static_i2c_lut[i2c].nvic);
    }

    LL_I2C_Enable(g_static_i2c_lut[i2c].periph);

    g_dynamic_i2c_lut[i2c].flag_callback = flag_callback;
    g_dynamic_i2c_lut[i2c].callback_context = context;
    g_dynamic_i2c_lut[i2c].state = eI2cDriverState_Init;

    return true;
}

bool I2C_Driver_EnableIt (const eI2cDriver_t i2c) {
    if ((i2c < eI2cDriver_First) || (i2c >= eI2cDriver_Last)) {
        return false;
    }

    if (g_dynamic_i2c_lut[i2c].state == eI2cDriverState_Off) {
        return false;
    }

    LL_I2C_EnableIT_EVT(g_static_i2c_lut[i2c].periph);
    LL_I2C_EnableIT_BUF(g_static_i2c_lut[i2c].periph);

    return true;
}

bool I2C_Driver_DisableIt (const eI2cDriver_t i2c) {
    if ((i2c < eI2cDriver_First) || (i2c >= eI2cDriver_Last)) {
        return false;
    }

    if (g_dynamic_i2c_lut[i2c].state == eI2cDriverState_Off) {
        return false;
    }

    LL_I2C_DisableIT_EVT(g_static_i2c_lut[i2c].periph);
    LL_I2C_DisableIT_BUF(g_static_i2c_lut[i2c].periph);

    return true;
}

bool I2C_Driver_StartComms (const eI2cDriver_t i2c, const uint8_t address, const uint8_t rw_operation) {
    if ((i2c < eI2cDriver_First) || (i2c >= eI2cDriver_Last)) {
        return false;
    }

    if (g_dynamic_i2c_lut[i2c].state == eI2cDriverState_Off) {
        return false;
    }

    if (rw_operation != I2C_WRITE && rw_operation != I2C_READ) {
        return false;
    }

    g_dynamic_i2c_lut[i2c].address = address;
    g_dynamic_i2c_lut[i2c].rw_operation = rw_operation;

    LL_I2C_GenerateStartCondition(g_static_i2c_lut[i2c].periph);

    return true;
}

bool I2C_Driver_StopComms (const eI2cDriver_t i2c) {
    if ((i2c < eI2cDriver_First) || (i2c >= eI2cDriver_Last)) {
        return false;
    }

    if (g_dynamic_i2c_lut[i2c].state == eI2cDriverState_Off) {
        return false;
    }

    LL_I2C_GenerateStopCondition(g_static_i2c_lut[i2c].periph);

    return true;
}

bool I2C_Driver_Acknowledge (const eI2cDriver_t i2c, const bool ack) {
    if ((i2c < eI2cDriver_First) || (i2c >= eI2cDriver_Last)) {
        return false;
    }

    if (g_dynamic_i2c_lut[i2c].state != eI2cDriverState_Init) {
        return false;
    }

    if (ack) {
        LL_I2C_AcknowledgeNextData(g_static_i2c_lut[i2c].periph, LL_I2C_ACK);
    } else {
        LL_I2C_AcknowledgeNextData(g_static_i2c_lut[i2c].periph, LL_I2C_NACK);
    }

    return true;
}

bool I2C_Driver_SendByte (const eI2cDriver_t i2c, const uint8_t data) {
    if ((i2c < eI2cDriver_First) || (i2c >= eI2cDriver_Last)) {
        return false;
    }

    if (g_dynamic_i2c_lut[i2c].state != eI2cDriverState_Init) {
        return false;
    }

    LL_I2C_TransmitData8(g_static_i2c_lut[i2c].periph, data);

    return true;
}

bool I2C_Driver_ReadByte (const eI2cDriver_t i2c, uint8_t *data) {
    if ((i2c < eI2cDriver_First) || (i2c >= eI2cDriver_Last)) {
        return false;
    }

    if (g_dynamic_i2c_lut[i2c].state != eI2cDriverState_Init) {
        return false;
    }

    if (data == NULL) {
        return false;
    }

    *data = LL_I2C_ReceiveData8(g_static_i2c_lut[i2c].periph);

    return true;
}

bool I2C_Driver_ReadByteAck (const eI2cDriver_t i2c, uint8_t *data, const bool ack) {
    if ((i2c < eI2cDriver_First) || (i2c >= eI2cDriver_Last)) {
        return false;
    }

    if (g_dynamic_i2c_lut[i2c].state != eI2cDriverState_Init) {
        return false;
    }

    if (data == NULL) {
        return false;
    }

    *data = LL_I2C_ReceiveData8(g_static_i2c_lut[i2c].periph);

    if (ack) {
        LL_I2C_AcknowledgeNextData(g_static_i2c_lut[i2c].periph, LL_I2C_ACK);
    } else {
        LL_I2C_AcknowledgeNextData(g_static_i2c_lut[i2c].periph, LL_I2C_NACK);
    }

    return true;
}

bool I2C_Driver_CheckFlag (const eI2cDriver_t i2c, const eI2cDriver_Flags_t flag) {
    if ((i2c < eI2cDriver_First) || (i2c >= eI2cDriver_Last)) {
        return false;
    }

    if (g_dynamic_i2c_lut[i2c].state == eI2cDriverState_Off) {
        return false;
    }
    
    uint32_t (*flag_fp) (I2C_TypeDef *i2c) = NULL;
    
    switch (flag) {
        case eI2cDriver_Flags_Busy: {
            flag_fp = LL_I2C_IsActiveFlag_BUSY;
        } break;
        case eI2cDriver_Flags_Addr: {
            flag_fp = LL_I2C_IsActiveFlag_ADDR;
        } break;
        case eI2cDriver_Flags_Txe: {
            flag_fp = LL_I2C_IsActiveFlag_TXE;
        } break;
        case eI2cDriver_Flags_Rxne: {
            flag_fp = LL_I2C_IsActiveFlag_RXNE;
        } break;
        case eI2cDriver_Flags_StartBit : {
            flag_fp = LL_I2C_IsActiveFlag_SB;
        } break;
        case eI2cDriver_Flags_ByteTransferFinish: {
            flag_fp = LL_I2C_IsActiveFlag_BTF;
        } break;
        case eI2cDriver_Flags_AckFailure: {
            flag_fp = LL_I2C_IsActiveFlag_AF;
        } break;
        default: {
            return false;
        }
    }

    return flag_fp(g_static_i2c_lut[i2c].periph);
}

void I2C_Driver_ClearFlag (const eI2cDriver_t i2c, const eI2cDriver_Flags_t flag) {
    if ((i2c < eI2cDriver_First) || (i2c >= eI2cDriver_Last)) {
        return;
    }

    if (g_dynamic_i2c_lut[i2c].state == eI2cDriverState_Off) {
        return;
    }
    
    void (*flag_fp) (I2C_TypeDef *i2c) = NULL;

    switch (flag) {
        case eI2cDriver_Flags_Addr: {
            flag_fp = LL_I2C_ClearFlag_ADDR;
        } break;
        case eI2cDriver_Flags_AckFailure: {
            flag_fp = LL_I2C_ClearFlag_AF;
        } break;
        case eI2cDriver_Flags_BusError: {
            flag_fp = LL_I2C_ClearFlag_BERR;
        } break;
        default: {
            return;
        }
    }

    flag_fp(g_static_i2c_lut[i2c].periph);

    return;
}

bool I2C_Driver_ResetLine (const eI2cDriver_t i2c) {
    if ((i2c < eI2cDriver_First) || (i2c >= eI2cDriver_Last)) {
        return false;
    }

    if (g_dynamic_i2c_lut[i2c].state == eI2cDriverState_Off) {
        return false;
    }

    LL_I2C_Disable(g_static_i2c_lut[i2c].periph);

    I2C_Driver_ClearAllFlags(i2c);

    g_dynamic_i2c_lut[i2c].state = eI2cDriverState_Reset;

    LL_I2C_Enable(g_static_i2c_lut[i2c].periph);

    I2C_Driver_EnableIt(i2c);

    I2C_Driver_StartComms(i2c, GENERAL_CALL_ADDRESS, I2C_WRITE);

    return true;
}
