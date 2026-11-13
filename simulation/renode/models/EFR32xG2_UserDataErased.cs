// Erased USERDATA page. Renode's EFR32xG24_FlashUserData returns 0 for every MFG token except the EUI64 and CCA
// threshold. A zero MFG_ASH_CONFIG gives ASH an ACK period of 0 and a retry limit of 0, so the NCP errors out right
// after its first DATA frame. On an erased page every token reads 0xFF and the stack falls back to its defaults.

using System;

using Antmicro.Renode.Peripherals.Bus;

namespace Antmicro.Renode.Peripherals.Miscellaneous.SiLabs
{
    public class EFR32xG2_UserDataErased : IBytePeripheral, IWordPeripheral, IDoubleWordPeripheral, IKnownSize
    {
        public EFR32xG2_UserDataErased(long size = 0x400)
        {
            data = new byte[size];
            for(var i = 0; i < data.Length; i++)
            {
                data[i] = 0xFF;
            }
        }

        public byte ReadByte(long offset)
        {
            return data[offset];
        }

        public void WriteByte(long offset, byte value)
        {
            data[offset] = value;
        }

        public ushort ReadWord(long offset)
        {
            return BitConverter.ToUInt16(data, (int)offset);
        }

        public void WriteWord(long offset, ushort value)
        {
            BitConverter.GetBytes(value).CopyTo(data, offset);
        }

        public uint ReadDoubleWord(long offset)
        {
            return BitConverter.ToUInt32(data, (int)offset);
        }

        public void WriteDoubleWord(long offset, uint value)
        {
            BitConverter.GetBytes(value).CopyTo(data, offset);
        }

        // Flash keeps its contents across resets
        public void Reset()
        {
        }

        public long Size => data.Length;

        private readonly byte[] data;
    }
}
