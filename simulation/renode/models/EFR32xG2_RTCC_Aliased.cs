// Renode's EFR32xG2_RTCC only decodes the base registers. Its "set" connection region also clears bits instead of
// setting them. The sleeptimer enables compare interrupts through IEN_SET (0x1018), so with plain registration the
// comparator never fires. Turn SET/CLR/TGL alias writes into read-modify-writes of the base register.
// The model's IF/IEN layout (CC0/CC1/CC2 at bits 4/6/8) also differs from xG21 (bits 2/3/4), so those bits are moved.

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
            return ReadBase(offset & BaseMask);
        }

        void IDoubleWordPeripheral.WriteDoubleWord(long offset, uint value)
        {
            var register = offset & BaseMask;
            var alias = offset & AliasMask;
            if(alias == 0)
            {
                WriteBase(register, value);
                return;
            }
            var current = ReadBase(register);
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
            WriteBase(register, current);
        }

        private uint ReadBase(long register)
        {
            var value = base.ReadDoubleWord(register);
            return IsInterruptRegister(register) ? MoveBits(value, ModelInterruptBits, InterruptBits) : value;
        }

        private void WriteBase(long register, uint value)
        {
            base.WriteDoubleWord(register, IsInterruptRegister(register) ? MoveBits(value, InterruptBits, ModelInterruptBits) : value);
        }

        private static bool IsInterruptRegister(long register)
        {
            return register == InterruptFlags || register == InterruptEnable;
        }

        private static uint MoveBits(uint value, int[] from, int[] to)
        {
            var result = 0u;
            for(var i = 0; i < from.Length; i++)
            {
                result |= ((value >> from[i]) & 1) << to[i];
            }
            return result;
        }

        // OF, CNTTICK, CC0, CC1, CC2
        private static readonly int[] InterruptBits = { 0, 1, 2, 3, 4 };
        private static readonly int[] ModelInterruptBits = { 0, 1, 4, 6, 8 };

        private const long BaseMask = 0xFFF;
        private const long AliasMask = 0x3000;
        private const long SetAlias = 0x1000;
        private const long ClearAlias = 0x2000;
        private const long InterruptFlags = 0x14;
        private const long InterruptEnable = 0x18;
    }
}
