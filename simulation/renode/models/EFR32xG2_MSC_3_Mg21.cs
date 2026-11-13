// Renode's EFR32xG2_MSC_3 hard-codes the xG24 flash base (0x08000000) and flags every erase/write address below it as
// invalid. xG21 flash starts at 0x0, so NVM3 can never format or write its pages and the Zigbee stack asserts in
// sli_zigbee_stack_init. Shift ADDRB into the xG24 range; the platform also maps the flash there.

using Antmicro.Renode.Core;
using Antmicro.Renode.Peripherals.Bus;
using Antmicro.Renode.Peripherals.CPU;

namespace Antmicro.Renode.Peripherals.Miscellaneous.SiLabs
{
    public class EFR32xG2_MSC_3_Mg21 : EFR32xG2_MSC_3, IDoubleWordPeripheral
    {
        public EFR32xG2_MSC_3_Mg21(Machine machine, CPUCore cpu, uint flashSize, uint flashPageSize) : base(machine, cpu, flashSize, flashPageSize)
        {
        }

        uint IDoubleWordPeripheral.ReadDoubleWord(long offset)
        {
            var value = ReadDoubleWord(offset);
            if((offset & BaseMask) == PageEraseOrWriteAddress && value >= ModelFlashBase)
            {
                value -= ModelFlashBase;
            }
            return value;
        }

        void IDoubleWordPeripheral.WriteDoubleWord(long offset, uint value)
        {
            if((offset & BaseMask) == PageEraseOrWriteAddress && value < ModelFlashBase)
            {
                value += ModelFlashBase;
            }
            WriteDoubleWord(offset, value);
        }

        private const long BaseMask = 0xFFF;
        private const long PageEraseOrWriteAddress = 0x14;
        private const uint ModelFlashBase = 0x08000000;
    }
}
