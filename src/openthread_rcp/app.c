/*******************************************************************************
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

#include <openthread/diag.h>
#include <openthread/ncp.h>
#include <openthread/tasklet.h>

#include "app.h"
#include "openthread-system.h"

#include "reset_util.h"

#if OPENTHREAD_CONFIG_MULTIPAN_RCP_ENABLE
#if OPENTHREAD_CONFIG_MULTIPLE_STATIC_INSTANCE_ENABLE == 0
#error "Support for multiple OpenThread static instance is disabled."
#endif
otInstance *sInstances[OPENTHREAD_CONFIG_MULTIPLE_INSTANCE_NUM] = {NULL};
#endif // OPENTHREAD_CONFIG_MULTIPAN_RCP_ENABLE

/**
 * This function initializes the NCP app.
 *
 * @param[in]  aInstance  The OpenThread instance structure.
 *
 */
#if OPENTHREAD_CONFIG_MULTIPAN_RCP_ENABLE
extern void otAppNcpInitMulti(otInstance **aInstances, uint8_t aCount);
#else
extern void otAppNcpInit(otInstance *aInstance);
#endif

static otInstance *sInstance = NULL;

otInstance *otGetInstance(void)
{
    return sInstance;
}

void sl_ot_create_instance(void)
{
#if OPENTHREAD_CONFIG_MULTIPAN_RCP_ENABLE
    for (int i = 0; i < OPENTHREAD_CONFIG_MULTIPLE_INSTANCE_NUM; i++)
    {
        sInstances[i] = otInstanceInitMultiple(i);

        assert(sInstances[i]);
    }
    sInstance = sInstances[0];
#elif OPENTHREAD_CONFIG_MULTIPLE_INSTANCE_ENABLE && !OPENTHREAD_CONFIG_MULTIPAN_RCP_ENABLE
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
#if OPENTHREAD_CONFIG_MULTIPAN_RCP_ENABLE
    otAppNcpInitMulti(sInstances, OPENTHREAD_CONFIG_MULTIPLE_INSTANCE_NUM);
#else
    otAppNcpInit(sInstance);
#endif
}

/******************************************************************************
 * Application Init.
 *****************************************************************************/

void app_init(void)
{
    OT_SETUP_RESET_JUMP(argv);
}

/******************************************************************************
 * Application Process Action.
 *****************************************************************************/
void app_process_action(void)
{
    otTaskletsProcess(sInstance);
    otSysProcessDrivers(sInstance);
}

/******************************************************************************
 * Application Exit.
 *****************************************************************************/
void app_exit(void)
{
    otInstanceFinalize(sInstance);
#if OPENTHREAD_CONFIG_MULTIPLE_INSTANCE_ENABLE && !OPENTHREAD_CONFIG_MULTIPAN_RCP_ENABLE
    free(otInstanceBuffer);
#endif
    // TO DO : pseudo reset?
}
