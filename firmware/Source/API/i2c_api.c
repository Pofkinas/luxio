/**********************************************************************************************************************
 * Includes
 *********************************************************************************************************************/

#include "i2c_api.h"
#include "cmsis_os2.h"
#include "i2c_driver.h"
#include "debug_api.h"
#include "system_config.h"

/**********************************************************************************************************************
 * Private definitions and macros
 *********************************************************************************************************************/

#define DEBUG_I2C_API

#define MUTEX_TIMEOUT 0U
#define I2C_MAX_DATA_SIZE 16
#define BUS_RESET_TIMEOUT 1U

#define I2C_FLAG_SUCCESS 0x01U
#define I2C_FLAG_ERROR 0x02U
#define I2C_FLAG_BUS_RESET 0x04U

/**********************************************************************************************************************
 * Private typedef
 *********************************************************************************************************************/

typedef enum eI2cState {
    eI2cState_First = 0,
    eI2cState_Off = eI2cState_First,
    eI2cState_Idle,
    eI2cState_StartComms,
    eI2cState_SendMemAddress,
    eI2cState_RestartComms,
    eI2cState_SendData,
    eI2cState_PrepRead,
    eI2cState_ReadData,
    eI2cState_Success,
    eI2cState_Error,
    eI2cState_Last
} eI2cState_t;

typedef struct sI2cStaticDesc {
    eI2cDriver_t i2c_driver;
    osEventFlagsAttr_t flag_attributes;
    osMutexAttr_t mutex_attributes;
} sI2cStaticDesc_t;

typedef struct sI2cDynamicDesc {
    eI2c_t device;
    eI2cState_t state;
    eI2cState_t previous_state;
    uint8_t rw_operation;
    osEventFlagsId_t flag;
    osMutexId_t mutex;
    uint8_t device_address;
    uint8_t data[I2C_MAX_DATA_SIZE];
    size_t processed_data;
    size_t data_size;
    uint16_t mem_address;
    uint8_t mem_address_size;
} sI2cDynamicDesc_t;

/**********************************************************************************************************************
 * Private constants
 *********************************************************************************************************************/

#ifdef DEBUG_I2C_API
CREATE_MODULE_NAME (I2C_API)
#else
CREATE_MODULE_NAME_EMPTY
#endif

/* clang-format off */
static const sI2cStaticDesc_t g_static_i2c_lut[eI2c_Last] = {
    [eI2c_1] = {
        .i2c_driver = eI2cDriver_1,
        .flag_attributes = {.name = "I2C_API_1_EventFlag", .attr_bits = 0, .cb_mem = NULL, .cb_size = 0U},
        .mutex_attributes = {.name = "I2C_API_1_Mutex", .attr_bits = osMutexRecursive | osMutexPrioInherit, .cb_mem = NULL, .cb_size = 0U}
    }
};
/* clang-format on */

/**********************************************************************************************************************
 * Private variables
 *********************************************************************************************************************/

/* clang-format off */
static sI2cDynamicDesc_t g_dynamic_i2c[eI2cDriver_Last] = {
    [eI2cDriver_1] = {
        .state = eI2cState_Off,
        .previous_state = eI2cState_Off,
        .flag = NULL,
        .mutex = NULL,
    }
};
/* clang-format on */

/**********************************************************************************************************************
 * Exported variables and references
 *********************************************************************************************************************/

/**********************************************************************************************************************
 * Prototypes of private functions
 *********************************************************************************************************************/
 
static void I2C_API_IrsCallback (const eI2cDriver_Flags_t flag, void *context);
static bool I2C_API_IsCorrectDevice (const eI2c_t i2c);
static char *I2C_API_GetStateString (const eI2cState_t state);
static bool I2C_API_StartComms (const eI2c_t i2c);
static bool I2C_API_SendMemAddress (const eI2c_t i2c);
static bool I2C_API_SendData (const eI2c_t i2c);
static bool I2C_API_PrepRead (const eI2c_t i2c);
static bool I2C_API_ReadData (const eI2c_t i2c);
static void I2C_API_HandleSuccess (const eI2c_t i2c);
static void I2C_API_HandleError (const eI2c_t i2c);

/**********************************************************************************************************************
 * Definitions of private functions
 *********************************************************************************************************************/

static void I2C_API_IrsCallback (const eI2cDriver_Flags_t flag, void *context) {
    if (context == NULL) {
        return;
    }

    sI2cDynamicDesc_t *i2c = (sI2cDynamicDesc_t *) context;

    bool (*opperation) (const eI2c_t i2c) = NULL;

    switch (i2c->state) {
        case eI2cState_StartComms: {
            if (flag == eI2cDriver_Flags_Addr) {
                I2C_Driver_ClearFlag(i2c->device, flag);

                i2c->state = eI2cState_SendMemAddress;

                opperation = I2C_API_SendMemAddress;
            }
        } break;
        case eI2cState_SendMemAddress: {
            if (flag == eI2cDriver_Flags_Txe) {
                if (i2c->mem_address_size > 0) {
                    opperation = I2C_API_SendMemAddress;

                    break;
                }
                
                if (i2c->rw_operation == I2C_WRITE) {
                    i2c->state = eI2cState_SendData;

                    opperation = I2C_API_SendData;
                } else if (i2c->rw_operation == I2C_READ) {
                    i2c->state = eI2cState_RestartComms;

                    opperation = I2C_API_StartComms;
                }
            }
        } break;
        case eI2cState_RestartComms: {
            if (flag == eI2cDriver_Flags_Addr) {
                i2c->state = eI2cState_PrepRead;

                opperation = I2C_API_PrepRead;
            }
        } break;
        case eI2cState_SendData: {
            if (flag == eI2cDriver_Flags_Txe) {
                opperation = I2C_API_SendData;
            }
        } break;
        case eI2cState_PrepRead: {
            if (flag == eI2cDriver_Flags_Rxne) {
                i2c->state = eI2cState_ReadData;

                opperation = I2C_API_ReadData;
            }
        } break;
        case eI2cState_ReadData: {
            if (flag == eI2cDriver_Flags_Rxne) {
                opperation = I2C_API_ReadData;
            }
        } break;
        case eI2cState_Error: {
            if (flag == eI2cDriver_Flags_BusReset) {
                osEventFlagsSet(i2c->flag, I2C_FLAG_BUS_RESET);

                return;
            }
        }
        default: {
        } break;
    }

    if (opperation != NULL) {
        if (!opperation(i2c->device)) {
            TRACE_ERR("Invalid FSM State [%s]\n", I2C_API_GetStateString(i2c->state));

            i2c->state = eI2cState_Error;

            I2C_API_HandleError(i2c->device);
        }
    }

    return;
}

static bool I2C_API_IsCorrectDevice (const eI2c_t i2c) {
    return (i2c >= eI2c_First) && (i2c < eI2c_Last);
}

static char *I2C_API_GetStateString (const eI2cState_t state) {
    char *state_string = NULL;
    
    switch (state) {
        case eI2cState_StartComms: {
            state_string = "Start Comms";
        } break;
        case eI2cState_SendMemAddress: {
            state_string = "Send Memory Address";
        } break;
        case eI2cState_RestartComms: {
            state_string = "Restart Comms";
        } break;
        case eI2cState_SendData: {
            state_string = "Send Data";
        } break;
        case eI2cState_PrepRead: {
            state_string = "Prepare Read";
        } break;
        case eI2cState_ReadData: {
            state_string = "Read Data";
        } break;
        default: {
            state_string = "Unknown State";
        } break;
    }

    return state_string;
}

static bool I2C_API_StartComms (const eI2c_t i2c) {
    if (!I2C_API_IsCorrectDevice(i2c)) {
        return false;
    }

    uint8_t opperation = 0;

    if (g_dynamic_i2c[i2c].state == eI2cState_StartComms) {
        opperation = I2C_WRITE;
    } else if (g_dynamic_i2c[i2c].state == eI2cState_RestartComms) {
        opperation = I2C_READ;
    } else {
        return false;
    }

    if (!I2C_Driver_StartComms(g_static_i2c_lut[i2c].i2c_driver, g_dynamic_i2c[i2c].device_address, opperation)) {
        return false;
    }

    return true;
}

static bool I2C_API_SendMemAddress (const eI2c_t i2c) {
    if (!I2C_API_IsCorrectDevice(i2c)) {
        return false;
    }

    if (g_dynamic_i2c[i2c].state != eI2cState_SendMemAddress) {
        return false;
    }

    if (g_dynamic_i2c[i2c].data_size == 0) {
        g_dynamic_i2c[i2c].state = eI2cState_Success;

        I2C_Driver_StopComms(g_static_i2c_lut[i2c].i2c_driver);
        
        I2C_API_HandleSuccess(g_static_i2c_lut[i2c].i2c_driver);

        return true;
    }

    if (g_dynamic_i2c[i2c].mem_address_size != 0) {
        switch (g_dynamic_i2c[i2c].mem_address_size) {
            case 1: {
                I2C_Driver_SendByte(i2c, (uint8_t) (g_dynamic_i2c[i2c].mem_address & 0xFF));
            } break;
            case 2: {
                I2C_Driver_SendByte(i2c, (uint8_t) (g_dynamic_i2c[i2c].mem_address >> 8));
            } break;
            default: {
                return false;
            }    
        }
        g_dynamic_i2c[i2c].mem_address_size--;
    }

    return true;
}

static bool I2C_API_SendData (const eI2c_t i2c) {
    if (!I2C_API_IsCorrectDevice(i2c)) {
        return false;
    }
    
    if (g_dynamic_i2c[i2c].state != eI2cState_SendData) {
        return false;
    }

    if (g_dynamic_i2c[i2c].processed_data >= g_dynamic_i2c[i2c].data_size) {
        if (!I2C_Driver_StopComms(g_static_i2c_lut[i2c].i2c_driver)) {
            return false;
        }

        g_dynamic_i2c[i2c].state = eI2cState_Success;
    
        I2C_API_HandleSuccess(g_static_i2c_lut[i2c].i2c_driver);

        return true;
    }

    if (!I2C_Driver_SendByte(g_static_i2c_lut[i2c].i2c_driver, g_dynamic_i2c[i2c].data[g_dynamic_i2c[i2c].processed_data])) {
        return false;
    }

    g_dynamic_i2c[i2c].processed_data++;

    return true;
}

static bool I2C_API_PrepRead (const eI2c_t i2c) {
    if (!I2C_API_IsCorrectDevice(i2c)) {
        return false;
    }

    if (g_dynamic_i2c[i2c].state != eI2cState_PrepRead) {
        return false;
    }

    if (g_dynamic_i2c[i2c].data_size == 1) {
        if (!I2C_Driver_Acknowledge(g_static_i2c_lut[i2c].i2c_driver, false)) {
            return false;
        }

        I2C_Driver_ClearFlag(g_dynamic_i2c[i2c].device, eI2cDriver_Flags_Addr);
        
        if (!I2C_Driver_StopComms(g_static_i2c_lut[i2c].i2c_driver)) {
            return false;
        }
    } else {
        I2C_Driver_ClearFlag(g_dynamic_i2c[i2c].device, eI2cDriver_Flags_Addr);
    
        if (!I2C_Driver_Acknowledge(g_static_i2c_lut[i2c].i2c_driver, true)) {
            return false;
        }
    }

    return true;
}

static bool I2C_API_ReadData (const eI2c_t i2c) {
    if (!I2C_API_IsCorrectDevice(i2c)) {
        return false;
    }

    if (g_dynamic_i2c[i2c].data_size == 0) {
        return false;
    }

    if (g_dynamic_i2c[i2c].data_size == 0) {
        return false;
    }

    switch (g_dynamic_i2c[i2c].data_size) {
        case 1: {
            if (!I2C_Driver_ReadByte(g_static_i2c_lut[i2c].i2c_driver, &g_dynamic_i2c[i2c].data[g_dynamic_i2c[i2c].processed_data])) {
                return false;
            }
        } break;
        case 2: {
            if (!I2C_Driver_ReadByteAck(g_static_i2c_lut[i2c].i2c_driver, &g_dynamic_i2c[i2c].data[g_dynamic_i2c[i2c].processed_data], false)) {
                return false;
            }

            if (!I2C_Driver_StopComms(g_static_i2c_lut[i2c].i2c_driver)) {
                return false;
            }
        } break;
        default: {
            if (!I2C_Driver_ReadByteAck(g_static_i2c_lut[i2c].i2c_driver, &g_dynamic_i2c[i2c].data[g_dynamic_i2c[i2c].processed_data], true)) {
                return false;
            }
        } break;
    }

    g_dynamic_i2c[i2c].data_size--;
    g_dynamic_i2c[i2c].processed_data++;

    if (g_dynamic_i2c[i2c].data_size == 0) {
        g_dynamic_i2c[i2c].state = eI2cState_Success;

        I2C_API_HandleSuccess(g_static_i2c_lut[i2c].i2c_driver);
    }

    return true;
}

static void I2C_API_HandleSuccess (const eI2c_t i2c) {
    if (!I2C_API_IsCorrectDevice(i2c)) {
        return;
    }

    if (g_dynamic_i2c[i2c].state != eI2cState_Success) {
        return;
    }

    I2C_Driver_DisableIt(i2c);

    while (I2C_Driver_CheckFlag(g_static_i2c_lut[i2c].i2c_driver, eI2cDriver_Flags_Busy)) {}

    osEventFlagsSet(g_dynamic_i2c[i2c].flag, I2C_FLAG_SUCCESS);

    return;
}

static void I2C_API_HandleError (const eI2c_t i2c) {
    if (!I2C_API_IsCorrectDevice(i2c)) {
        return;
    }

    if (g_dynamic_i2c[i2c].state != eI2cState_Error) {
        return;
    }

    I2C_Driver_DisableIt(i2c);

    I2C_Driver_ResetLine(g_static_i2c_lut[i2c].i2c_driver);

    osEventFlagsSet(g_dynamic_i2c[i2c].flag, I2C_FLAG_ERROR);

    return;
}

/**********************************************************************************************************************
 * Definitions of exported functions
 *********************************************************************************************************************/

bool I2C_API_Init (const eI2c_t i2c) {
    if ((i2c < eI2c_First) || (i2c >= eI2c_Last)) {
        return false;
    }

    if (g_dynamic_i2c[i2c].state != eI2cState_Off) {
        return true;
    }

    g_dynamic_i2c[i2c].device = i2c;

    if (!I2C_Driver_Init(i2c, &I2C_API_IrsCallback, &g_dynamic_i2c[i2c])) {
        return false;
    }

    if (g_dynamic_i2c[i2c].flag == NULL) {
        g_dynamic_i2c[i2c].flag = osEventFlagsNew(&g_static_i2c_lut[i2c].flag_attributes);
    }

    if (g_dynamic_i2c[i2c].mutex == NULL) {
        g_dynamic_i2c[i2c].mutex = osMutexNew(&g_static_i2c_lut[i2c].mutex_attributes);
    }

    g_dynamic_i2c[i2c].state = eI2cState_Idle;

    return true;
}

bool I2C_API_Write (const eI2c_t i2c, const uint8_t device_address, uint8_t *data, const size_t data_size, const uint16_t mem_address, const uint8_t mem_address_size, const uint32_t timeout) {
    if (timeout == 0 || data_size > I2C_MAX_DATA_SIZE) {
        return false;
    }

    if (g_dynamic_i2c[i2c].state != eI2cState_Idle) {
        return false;
    }

    if (I2C_Driver_CheckFlag(g_static_i2c_lut[i2c].i2c_driver, eI2cDriver_Flags_Busy)) {
        TRACE_ERR("I2C Write: I2C is busy\n");

        return false;
    }

    if (osMutexAcquire(g_dynamic_i2c[i2c].mutex, MUTEX_TIMEOUT) != osOK) {
        return false;
    }

    memcpy(g_dynamic_i2c[i2c].data, data, data_size);

    g_dynamic_i2c[i2c].rw_operation = I2C_WRITE;
    g_dynamic_i2c[i2c].device_address = device_address;
    g_dynamic_i2c[i2c].processed_data = 0;
    g_dynamic_i2c[i2c].data_size = data_size;
    g_dynamic_i2c[i2c].mem_address = mem_address;
    g_dynamic_i2c[i2c].mem_address_size = mem_address_size;

    g_dynamic_i2c[i2c].state = eI2cState_StartComms;

    osMutexRelease(g_dynamic_i2c[i2c].mutex);

    I2C_Driver_EnableIt(i2c);
    I2C_API_StartComms(i2c);

    uint32_t flag = osEventFlagsWait(g_dynamic_i2c[i2c].flag, I2C_FLAG_SUCCESS | I2C_FLAG_ERROR, osFlagsWaitAny, timeout);

    if (osMutexAcquire(g_dynamic_i2c[i2c].mutex, MUTEX_TIMEOUT) != osOK) {
        return false;
    }

    if ((int32_t) flag < 0) {
        g_dynamic_i2c[i2c].previous_state = g_dynamic_i2c[i2c].state;
        g_dynamic_i2c[i2c].state = eI2cState_Error;
    } else {
        g_dynamic_i2c[i2c].state = eI2cState_Idle;
    }

    osMutexRelease(g_dynamic_i2c[i2c].mutex);

    if (g_dynamic_i2c[i2c].state == eI2cState_Error) {
        I2C_Driver_DisableIt(i2c);

        TRACE_ERR("I2C Write: Error event flag [%d], I2C state: [%s]\n", (int32_t) flag, I2C_API_GetStateString(g_dynamic_i2c[i2c].previous_state));

        I2C_API_HandleError(i2c);

        flag = osEventFlagsWait(g_dynamic_i2c[i2c].flag, I2C_FLAG_BUS_RESET, osFlagsWaitAny, BUS_RESET_TIMEOUT);

        if (flag != (I2C_FLAG_BUS_RESET | I2C_FLAG_ERROR)) {
            TRACE_ERR("I2C Read: Failed reset bus, received flag [%d]\n", (int32_t) flag);
        }

        osEventFlagsClear(g_dynamic_i2c[i2c].flag, I2C_FLAG_ERROR);

        if (osMutexAcquire(g_dynamic_i2c[i2c].mutex, MUTEX_TIMEOUT) != osOK) {
            return false;
        }

        g_dynamic_i2c[i2c].state = eI2cState_Idle;

        osMutexRelease(g_dynamic_i2c[i2c].mutex);

        return false;
    }

    return (flag == I2C_FLAG_SUCCESS);
}

bool I2C_API_Read (const eI2c_t i2c, const uint8_t device_address, uint8_t *data, const size_t data_size, const uint16_t mem_address, const uint8_t mem_address_size, uint32_t timeout) {
    if (data == NULL || data_size == 0 || data_size > I2C_MAX_DATA_SIZE) {
        return false;
    }

    if (timeout == 0) {
        return false;
    }

    if (g_dynamic_i2c[i2c].state != eI2cState_Idle) {
        return false;
    }

    if (I2C_Driver_CheckFlag(g_static_i2c_lut[i2c].i2c_driver, eI2cDriver_Flags_Busy)) {
        TRACE_ERR("I2C Read: I2C is busy\n");

        return false;
    }

    if (osMutexAcquire(g_dynamic_i2c[i2c].mutex, MUTEX_TIMEOUT) != osOK) {
        return false;
    }

    memset(g_dynamic_i2c[i2c].data, 0, data_size);

    g_dynamic_i2c[i2c].rw_operation = I2C_READ;
    g_dynamic_i2c[i2c].device_address = device_address;
    g_dynamic_i2c[i2c].processed_data = 0;
    g_dynamic_i2c[i2c].data_size = data_size;
    g_dynamic_i2c[i2c].mem_address = mem_address;
    g_dynamic_i2c[i2c].mem_address_size = mem_address_size;

    g_dynamic_i2c[i2c].state = eI2cState_StartComms;

    osMutexRelease(g_dynamic_i2c[i2c].mutex);

    I2C_Driver_EnableIt(i2c);
    I2C_API_StartComms(i2c);

    uint32_t flag = osEventFlagsWait(g_dynamic_i2c[i2c].flag, I2C_FLAG_SUCCESS | I2C_FLAG_ERROR, osFlagsWaitAny, timeout);

    if (osMutexAcquire(g_dynamic_i2c[i2c].mutex, MUTEX_TIMEOUT) != osOK) {
        return false;
    }

    if ((int32_t) flag < 0) {
        g_dynamic_i2c[i2c].previous_state = g_dynamic_i2c[i2c].state;
        g_dynamic_i2c[i2c].state = eI2cState_Error;
    } else {
        g_dynamic_i2c[i2c].state = eI2cState_Idle;
    }

    if (g_dynamic_i2c[i2c].state == eI2cState_Error) {
        osMutexRelease(g_dynamic_i2c[i2c].mutex);
        
        I2C_Driver_DisableIt(i2c);

        TRACE_ERR("I2C Read: Error event flag [%d], I2C state: [%s]\n", (int32_t) flag, I2C_API_GetStateString(g_dynamic_i2c[i2c].previous_state));

        I2C_API_HandleError(i2c);

        flag = osEventFlagsWait(g_dynamic_i2c[i2c].flag, I2C_FLAG_BUS_RESET, osFlagsWaitAny, BUS_RESET_TIMEOUT);
        
        if (flag != (I2C_FLAG_BUS_RESET | I2C_FLAG_ERROR)) {
            TRACE_ERR("I2C Read: Failed reset bus, received flag [%d]\n", (int32_t) flag);
        }

        osEventFlagsClear(g_dynamic_i2c[i2c].flag, I2C_FLAG_ERROR);

        if (osMutexAcquire(g_dynamic_i2c[i2c].mutex, MUTEX_TIMEOUT) != osOK) {
            return false;
        }

        g_dynamic_i2c[i2c].state = eI2cState_Idle;

        osMutexRelease(g_dynamic_i2c[i2c].mutex);

        return false;
    }

    if (flag == I2C_FLAG_SUCCESS) {
        memcpy(data, g_dynamic_i2c[i2c].data, data_size);
    }

    osMutexRelease(g_dynamic_i2c[i2c].mutex);

    return (flag == I2C_FLAG_SUCCESS);
}
