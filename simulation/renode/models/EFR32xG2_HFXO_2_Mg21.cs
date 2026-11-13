// Renode's EFR32xG2_HFXO_2 sets STATUS.FSMLOCK when CTRL.DISONDEMAND is cleared but only releases it on
// CMD.MANUALOVERRIDE. The xG21 clock manager instead sets DISONDEMAND and waits for FSMLOCK to drop before it
// reconfigures the crystal, so boot hangs. Release FSMLOCK whenever DISONDEMAND gets set.

using System.Reflection;

using Antmicro.Renode.Core;
using Antmicro.Renode.Core.Structure.Registers;
using Antmicro.Renode.Peripherals.Bus;

namespace Antmicro.Renode.Peripherals.Miscellaneous.SiLabs
{
    public class EFR32xG2_HFXO_2_Mg21 : EFR32xG2_HFXO_2, IDoubleWordPeripheral
    {
        public EFR32xG2_HFXO_2_Mg21(Machine machine, uint startupDelayTicks) : base(machine, startupDelayTicks)
        {
        }

        uint IDoubleWordPeripheral.ReadDoubleWord(long offset)
        {
            return ReadDoubleWord(offset);
        }

        void IDoubleWordPeripheral.WriteDoubleWord(long offset, uint value)
        {
            WriteDoubleWord(offset, value);

            var alias = offset & AliasMask;
            if((offset & BaseMask) == ControlRegister && (value & DisableOnDemand) != 0 && alias != ClearAlias)
            {
                ((IFlagRegisterField)FsmLockField.GetValue(this)).Value = false;
            }
        }

        private const long BaseMask = 0xFFF;
        private const long AliasMask = 0x3000;
        private const long ClearAlias = 0x2000;
        private const long ControlRegister = 0x28;
        private const uint DisableOnDemand = 1u << 1;

        private static readonly FieldInfo FsmLockField =
            typeof(EFR32xG2_HFXO_2).GetField("fsmLock", BindingFlags.NonPublic | BindingFlags.Instance);
    }
}
