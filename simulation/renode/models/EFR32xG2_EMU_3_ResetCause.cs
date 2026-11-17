// Renode's EFR32xG2_EMU_3 always reads RSTCAUSE as 0, so EmberZNet classifies every boot as an unknown reset and ASH
// answers with ERROR instead of RSTACK. Report POR until the first machine reset and SYSREQ afterwards: in this setup a
// machine reset only comes from the firmware's SYSRESETREQ. CMD.RSTCAUSECLR clears it, as on hardware.

using Antmicro.Renode.Core;
using Antmicro.Renode.Peripherals.Bus;

namespace Antmicro.Renode.Peripherals.Miscellaneous.SiLabs
{
    public class EFR32xG2_EMU_3_ResetCause : EFR32xG2_EMU_3, IDoubleWordPeripheral
    {
        public EFR32xG2_EMU_3_ResetCause(Machine machine) : base(machine)
        {
            ResetCause = PowerOn;
        }

        uint IDoubleWordPeripheral.ReadDoubleWord(long offset)
        {
            if((offset & BaseMask) == ResetCauseRegister)
            {
                return ResetCause;
            }
            return ReadDoubleWord(offset);
        }

        void IDoubleWordPeripheral.WriteDoubleWord(long offset, uint value)
        {
            if((offset & BaseMask) == CommandRegister && (value & ResetCauseClear) != 0)
            {
                ResetCause = 0;
            }
            WriteDoubleWord(offset, value);
        }

        public override void Reset()
        {
            base.Reset();
            ResetCause = SystemRequest;
        }

        public uint ResetCause { get; set; }

        private const long BaseMask = 0xFFF;
        private const long CommandRegister = 0x70;
        private const long ResetCauseRegister = 0x94;
        private const uint ResetCauseClear = 1u << 17;
        private const uint PowerOn = 1u << 0;
        private const uint SystemRequest = 1u << 6;
    }
}
