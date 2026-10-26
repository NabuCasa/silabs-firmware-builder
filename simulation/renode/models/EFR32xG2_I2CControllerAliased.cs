// Renode's EFR32xG2_I2CController only maps the base registers plus IF_CLR. The Series 2 driver enables interrupts
// through IEN_SET (0x1040) and polls IF & IEN, so with IEN stuck at 0 every transfer spins until its timeout.
// Turn SET/CLR/TGL alias writes into read-modify-writes of the base register.
// EN is a tagged flag in the model and always reads 0, but the driver only issues START when EN reads back as 1,
// so EN is stored here.

using Antmicro.Renode.Core;
using Antmicro.Renode.Peripherals.Bus;

namespace Antmicro.Renode.Peripherals.I2C
{
    public class EFR32xG2_I2CControllerAliased : EFR32xG2_I2CController, IDoubleWordPeripheral
    {
        public EFR32xG2_I2CControllerAliased(IMachine machine) : base(machine)
        {
        }

        uint IDoubleWordPeripheral.ReadDoubleWord(long offset)
        {
            if((offset & BaseMask) == Enable)
            {
                return enable;
            }
            return base.ReadDoubleWord(offset & BaseMask);
        }

        void IDoubleWordPeripheral.WriteDoubleWord(long offset, uint value)
        {
            var alias = offset & AliasMask;
            if(offset == InterruptFlagClear)
            {
                base.WriteDoubleWord(offset, value);
                return;
            }
            if(alias == 0)
            {
                WriteBase(offset, value);
                return;
            }

            var register = offset & BaseMask;
            var current = register == Enable ? enable : base.ReadDoubleWord(register);
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

        public override void Reset()
        {
            base.Reset();
            enable = 0;
        }

        private void WriteBase(long register, uint value)
        {
            if(register == Enable)
            {
                enable = value & 1;
                return;
            }
            base.WriteDoubleWord(register, value);
        }

        private uint enable;

        private const long Enable = 0x4;
        private const long BaseMask = 0xFFF;
        private const long AliasMask = 0x3000;
        private const long SetAlias = 0x1000;
        private const long ClearAlias = 0x2000;
        private const long InterruptFlagClear = 0x203C;
    }
}
