// Renode's EFR32xG2_EUSART_2 misbehaves across a machine reset in two ways:
//  - Reset() drops the attached SPI device, so an LED or sensor on the bus stops receiving data.
//  - Reset() re-asserts TxFifoLevel while it is already high, so the freshly reset LDMA never sees the TX request and
//    DMA-fed transfers never start.
// Restore the device after the base reset and pulse TxFifoLevel on the first register access afterwards, when every
// peripheral has been reset.

using System.Reflection;

using Antmicro.Renode.Core;
using Antmicro.Renode.Peripherals.Bus;

namespace Antmicro.Renode.Peripherals.UART
{
    public class EFR32xG2_EUSART_2_KeepSpi : EFR32xG2_EUSART_2, IDoubleWordPeripheral
    {
        public EFR32xG2_EUSART_2_KeepSpi(Machine machine, uint clockFrequency = 19000000) : base(machine, clockFrequency)
        {
        }

        uint IDoubleWordPeripheral.ReadDoubleWord(long offset)
        {
            ReassertTxRequest();
            return ReadDoubleWord(offset);
        }

        void IDoubleWordPeripheral.WriteDoubleWord(long offset, uint value)
        {
            ReassertTxRequest();
            WriteDoubleWord(offset, value);
        }

        public override void Reset()
        {
            var device = SpiDeviceField.GetValue(this);
            base.Reset();
            SpiDeviceField.SetValue(this, device);
            resetPending = true;
        }

        private void ReassertTxRequest()
        {
            if(!resetPending)
            {
                return;
            }
            resetPending = false;
            TxFifoLevel.Unset();
            TxFifoLevel.Set(true);
        }

        private bool resetPending;

        private static readonly FieldInfo SpiDeviceField =
            typeof(EFR32xG2_EUSART_2).GetField("spiSlaveDevice", BindingFlags.NonPublic | BindingFlags.Instance);
    }
}
