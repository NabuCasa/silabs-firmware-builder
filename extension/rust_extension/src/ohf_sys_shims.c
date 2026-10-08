/* Real symbols for inline SDK helpers, which bindgen can't bind */
#include "em_device.h"

void ohf_system_reset(void)
{
    NVIC_SystemReset();
}
