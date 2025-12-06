/***************************************************************************//**
 * @file
 * @brief Configuration header for XNCP Common Commands
 ******************************************************************************/
#ifndef XNCP_CONFIG_H_
#define XNCP_CONFIG_H_

#include "sl_iostream_usart_vcom_config.h"

// <<< Use Configuration Wizard in Context Menu >>>

// <h>XNCP Common Commands Configuration

// <o XNCP_MANUAL_SOURCE_ROUTE_TABLE_SIZE> Manual source route table size
// <i> Number of manual source routes that can be stored
// <i> Default: 200
#ifndef XNCP_MANUAL_SOURCE_ROUTE_TABLE_SIZE
#define XNCP_MANUAL_SOURCE_ROUTE_TABLE_SIZE 200
#endif

// <s XNCP_MFG_MANUF_NAME> Manufacturer name override
// <i> String returned for EZSP_MFG_STRING token override
#ifndef XNCP_MFG_MANUF_NAME
#define XNCP_MFG_MANUF_NAME ""
#endif

// <s XNCP_MFG_BOARD_NAME> Board name override
// <i> String returned for EZSP_MFG_BOARD_NAME token override
#ifndef XNCP_MFG_BOARD_NAME
#define XNCP_MFG_BOARD_NAME ""
#endif

// <s XNCP_BUILD_STRING> Build string
// <i> Build identifier string (e.g. timestamp)
#ifndef XNCP_BUILD_STRING
#define XNCP_BUILD_STRING ""
#endif

// <o XNCP_FLOW_CONTROL_TYPE> Flow control type
// <i> Default: auto-detect from UART config (SL_IOSTREAM_USART_VCOM_FLOW_CONTROL_TYPE)
// <i> Override only if external flow control differs from internal UART (e.g. ZBT-2)
#ifndef XNCP_FLOW_CONTROL_TYPE
#define XNCP_FLOW_CONTROL_TYPE SL_IOSTREAM_USART_VCOM_FLOW_CONTROL_TYPE
#endif

// <o XNCP_EZSP_VERSION_PATCH_NUM_OVERRIDE> EZSP version patch number override
// <i> Override the EZSP patch version in GBL metadata (0xFF = no override)
// <i> Default: 0xFF
#ifndef XNCP_EZSP_VERSION_PATCH_NUM_OVERRIDE
#define XNCP_EZSP_VERSION_PATCH_NUM_OVERRIDE 0xFF
#endif

// </h>

// <<< end of configuration section >>>

#endif // XNCP_CONFIG_H_
