#include "sl_component_catalog.h"
#include "sl_status.h"
#include "stack/include/sl_zigbee.h"
#include "stack/include/sl_zigbee_types.h"
#include "stack/include/sl_zigbee_types_internal.h"
#include "stack/include/message.h"
#include "stack/include/stack-info.h"
#include "stack/include/sl_zigbee_random_api.h"
#include "ezsp-enum.h"
#include "em_usart.h"
#include "em_device.h"
/* The VCOM's own flow control, the default XNCP_FLOW_CONTROL_TYPE */
#if defined(SL_CATALOG_IOSTREAM_EUSART_PRESENT)
#include "sl_iostream_eusart.h"
#include "sl_iostream_eusart_vcom_config.h"
#define OHF_VCOM_FLOW_CONTROL_TYPE SL_IOSTREAM_EUSART_VCOM_FLOW_CONTROL_TYPE
#elif defined(SL_CATALOG_IOSTREAM_USART_PRESENT)
#include "sl_iostream_usart_vcom_config.h"
#define OHF_VCOM_FLOW_CONTROL_TYPE SL_IOSTREAM_USART_VCOM_FLOW_CONTROL_TYPE
#endif
