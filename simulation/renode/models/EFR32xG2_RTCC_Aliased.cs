// Renode's EFR32xG2_RTCC only decodes the base registers. Its "set" connection region also clears bits instead of
// setting them. The sleeptimer enables compare interrupts through IEN_SET (0x1018), so with plain registration the
// comparator never fires. Turn SET/CLR/TGL alias writes into read-modify-writes of the base register.

using Antmicro.Renode.Core;
using Antmicro.Renode.Peripherals.Bus;

namespace Antmicro.Renode.Peripherals.Timers
{
    public class EFR32xG2_RTCC_Aliased : EFR32xG2_RTCC, IDoubleWordPeripheral
    {
        public EFR32xG2_RTCC_Aliased(IMachine machine, ulong frequency) : base(machine, frequency)
        {
        }

        uint IDoubleWordPeripheral.ReadDoubleWord(long offset)
        {
            return base.ReadDoubleWord(offset & BaseMask);
        }

        void IDoubleWordPeripheral.WriteDoubleWord(long offset, uint value)
        {
            var register = offset & BaseMask;
            var alias = offset & AliasMask;
            if(alias == 0)
            {
                base.WriteDoubleWord(register, value);
                return;
            }
            var current = base.ReadDoubleWord(register);
            switch(alias)
            {
            case SetAlias:
                current |= value;
                break;
            case ClearAlias:
                current &= ~value;
                break;
            default:
                current ^= value;
                break;
            }
            base.WriteDoubleWord(register, current);
        }

        private const long BaseMask = 0xFFF;
        private const long AliasMask = 0x3000;
        private const long SetAlias = 0x1000;
        private const long ClearAlias = 0x2000;
    }
}
