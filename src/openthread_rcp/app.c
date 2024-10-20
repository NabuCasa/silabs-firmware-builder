/*******************************************************************************
 * @file
 * @brief Core application logic.
 *******************************************************************************
 * # License
 * <b>Copyright 2024 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * SPDX-License-Identifier: Zlib
 *
 * The licensor of this software is Silicon Laboratories Inc.
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
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

#include "sl_component_catalog.h"
#include "sl_memory_manager.h"

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

#if OPENTHREAD_CONFIG_MULTIPLE_INSTANCE_ENABLE && !OPENTHREAD_CONFIG_MULTIPAN_RCP_ENABLE
static uint8_t *sOtInstanceBuffer = NULL;
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
    size_t otInstanceBufferLength = 0;

    // Call to query the buffer size
    (void)otInstanceInit(NULL, &otInstanceBufferLength);

    // Call to allocate the buffer
    sOtInstanceBuffer = (uint8_t *)sl_malloc(otInstanceBufferLength);
    assert(sOtInstanceBuffer);

    // Initialize OpenThread with the buffer
    sInstance = otInstanceInit(sOtInstanceBuffer, &otInstanceBufferLength);
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
    sl_free(sOtInstanceBuffer);
#endif
    // TO DO : pseudo reset?
}
