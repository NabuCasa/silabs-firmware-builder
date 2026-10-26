// QMA6100P 3-axis accelerometer, just enough for the ZBT-2 qma6100p_driver: a 256-byte register file with
// auto-incrementing reads, chip ID 0x90, and X/Y/Z output registers driven by the Acceleration* properties (in g).

using System;

using Antmicro.Renode.Logging;
using Antmicro.Renode.Peripherals.I2C;

namespace Antmicro.Renode.Peripherals.Sensors
{
    public class QMA6100P : II2CPeripheral
    {
        public QMA6100P()
        {
            Reset();
        }

        public void Write(byte[] data)
        {
            if(data.Length == 0)
            {
                return;
            }

            pointer = data[0];
            for(var i = 1; i < data.Length; i++)
            {
                WriteRegister(pointer, data[i]);
                pointer++;
            }
        }

        public byte[] Read(int count = 1)
        {
            var result = new byte[count];
            for(var i = 0; i < count; i++)
            {
                result[i] = ReadRegister(pointer);
                pointer++;
            }
            return result;
        }

        public void FinishTransmission()
        {
        }

        public void Reset()
        {
            Array.Clear(registers, 0, registers.Length);
            registers[ChipIdRegister] = ChipId;
            pointer = 0;
        }

        public decimal AccelerationX { get; set; }

        public decimal AccelerationY { get; set; }

        // Lying flat
        public decimal AccelerationZ { get; set; } = 1;

        private void WriteRegister(byte register, byte value)
        {
            if(register == ResetRegister && value == ResetCommand)
            {
                this.Log(LogLevel.Debug, "Soft reset");
                Reset();
                return;
            }
            if(register == ChipIdRegister || (register >= XOutLow && register <= ZOutHigh))
            {
                return;
            }
            registers[register] = value;
        }

        private byte ReadRegister(byte register)
        {
            if(register >= XOutLow && register <= ZOutHigh)
            {
                var axis = (register - XOutLow) / 2;
                var sample = Sample(axis == 0 ? AccelerationX : axis == 1 ? AccelerationY : AccelerationZ);
                return (register - XOutLow) % 2 == 0 ? (byte)(sample & 0xFF) : (byte)((sample >> 8) & 0xFF);
            }
            return registers[register];
        }

        // 14-bit sample, left-justified in 16 bits. The driver negates and divides by 1024 LSB/g (8 g range).
        private ushort Sample(decimal g)
        {
            var lsb = (int)Math.Round(-g * LsbPerG);
            lsb = Math.Max(-8192, Math.Min(8191, lsb));
            return (ushort)(lsb << 2);
        }

        private byte pointer;
        private readonly byte[] registers = new byte[256];

        private const byte ChipIdRegister = 0x00;
        private const byte ChipId = 0x90;
        private const byte XOutLow = 0x01;
        private const byte ZOutHigh = 0x06;
        private const byte ResetRegister = 0x36;
        private const byte ResetCommand = 0xB6;
        private const decimal LsbPerG = 1024;
    }
}
