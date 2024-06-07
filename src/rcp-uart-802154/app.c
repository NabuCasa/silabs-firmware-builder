/***************************************************************************//**
 * @file
 * @brief Core application logic.
 *******************************************************************************
 * # License
 * <b>Copyright 2020 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * The licensor of this software is Silicon Laboratories Inc. Your use of this
 * software is governed by the terms of Silicon Labs Master Software License
 * Agreement (MSLA) available at
 * www.silabs.com/about-us/legal/master-software-license-agreement. This
 * software is distributed to you in Source Code format and is governed by the
 * sections of the MSLA applicable to Source Code.
 *
 ******************************************************************************/
#include <assert.h>
#include <openthread-core-config.h>
#include <openthread/config.h>

#include <openthread/ncp.h>
#include <openthread/diag.h>
#include <openthread/tasklet.h>
#include <openthread/logging.h>

#include "openthread-system.h"
#include "app.h"

#include "reset_util.h"

#include "sl_gsdk_version.h"
#include "config/internal_app_config.h"

void otPlatAssertFail(const char *aFilename, int aLineNumber)
{
#if OPENTHREAD_CONFIG_LOG_PLATFORM && OPENTHREAD_CONFIG_LOG_LEVEL < OT_LOG_LEVEL_CRIT
    OT_UNUSED_VARIABLE(aFilename);
    OT_UNUSED_VARIABLE(aLineNumber);
#else
    otLogCritPlat("assert failed at %s:%d", aFilename, aLineNumber);
#endif
    // For debug build, use assert to generate a core dump
    assert(false);
}

const char* sl_cpc_secondary_app_version(void)
{
    return (
        SL_GSDK_VERSION_STR
        CPC_SECONDARY_APP_VERSION_SUFFIX
    );
}

/**
 * This function initializes the NCP app.
 *
 * @param[in]  aInstance  The OpenThread instance structure.
 *
 */
extern void otAppNcpInit(otInstance *aInstance);

static otInstance* sInstance = NULL;

otInstance *otGetInstance(void)
{
    return sInstance;
}

void sl_ot_create_instance(void)
{
#if OPENTHREAD_CONFIG_MULTIPLE_INSTANCE_ENABLE
    size_t   otInstanceBufferLength = 0;
    uint8_t *otInstanceBuffer       = NULL;

    // Call to query the buffer size
    (void)otInstanceInit(NULL, &otInstanceBufferLength);

    // Call to allocate the buffer
    otInstanceBuffer = (uint8_t *)malloc(otInstanceBufferLength);
    assert(otInstanceBuffer);

    // Initialize OpenThread with the buffer
    sInstance = otInstanceInit(otInstanceBuffer, &otInstanceBufferLength);
#else
    sInstance = otInstanceInitSingle();
#endif
    assert(sInstance);
}

void sl_ot_ncp_init(void)
{
    otAppNcpInit(sInstance);
}

/**************************************************************************//**
 * Application Init.
 *****************************************************************************/

void app_init(void)
{
    OT_SETUP_RESET_JUMP(argv);
}

/**************************************************************************//**
 * Application Process Action.
 *****************************************************************************/
void app_process_action(void)
{
    otTaskletsProcess(sInstance);
    otSysProcessDrivers(sInstance);
}

/**************************************************************************//**
 * Application Exit.
 *****************************************************************************/
void app_exit(void)
{
    otInstanceFinalize(sInstance);
#if OPENTHREAD_CONFIG_MULTIPLE_INSTANCE_ENABLE
    free(otInstanceBuffer);
#endif
    // TO DO : pseudo reset?
}
