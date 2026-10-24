// WS2812 LED chain driven through an SPI MOSI line, as the ZBT-2 does over EUSART1. Each SPI bit is one time slot
// on the data line: a 1-slot high pulse is a 0 bit and a 2+-slot high pulse is a 1 bit (3.2 Mbit/s, 312.5 ns
// slots). Every 24 bits are one LED's G, R, B. A low run of LatchSlots or more ends the frame and latches the colors.

using System;
using System.Collections.Generic;
using System.Linq;

using Antmicro.Renode.Logging;
using Antmicro.Renode.Peripherals.SPI;

namespace Antmicro.Renode.Peripherals.Miscellaneous
{
    public class WS2812 : ISPIPeripheral
    {
        public WS2812()
        {
            Reset();
        }

        public byte Transmit(byte data)
        {
            for(var i = 7; i >= 0; i--)
            {
                Slot(((data >> i) & 1) != 0);
            }
            return 0;
        }

        public void FinishTransmission()
        {
        }

        public void Reset()
        {
            highRun = 0;
            lowRun = 0;
            word = 0;
            wordBits = 0;
            pending.Clear();
            latched = new List<uint>();
            Frames = 0;
        }

        // Latched colors as "#RRGGBB" per LED, in chain order
        public string Colors => string.Join(" ", latched.Select(c => $"#{c:X6}"));

        public int LedCount => latched.Count;

        public ulong Frames { get; private set; }

        public byte Red => Channel(0, 16);

        public byte Green => Channel(0, 8);

        public byte Blue => Channel(0, 0);

        private byte Channel(int led, int shift)
        {
            return led < latched.Count ? (byte)(latched[led] >> shift) : (byte)0;
        }

        private void Slot(bool high)
        {
            if(high)
            {
                highRun++;
                lowRun = 0;
                return;
            }

            if(highRun > 0)
            {
                Bit(highRun >= 2);
                highRun = 0;
            }

            lowRun++;
            if(lowRun == LatchSlots)
            {
                Latch();
            }
        }

        private void Bit(bool one)
        {
            word = (word << 1) | (one ? 1u : 0u);
            wordBits++;
            if(wordBits == 24)
            {
                // Wire order is G, R, B; store as 0xRRGGBB
                var green = (word >> 16) & 0xFF;
                var red = (word >> 8) & 0xFF;
                var blue = word & 0xFF;
                pending.Add((red << 16) | (green << 8) | blue);
                word = 0;
                wordBits = 0;
            }
        }

        private void Latch()
        {
            if(wordBits != 0)
            {
                this.Log(LogLevel.Warning, "Frame ended with {0} stray bits", wordBits);
                word = 0;
                wordBits = 0;
            }
            if(pending.Count == 0)
            {
                return;
            }

            Frames++;
            if(!pending.SequenceEqual(latched))
            {
                latched = new List<uint>(pending);
                this.Log(LogLevel.Debug, "Colors: {0}", Colors);
            }
            pending.Clear();
        }

        private int highRun;
        private int lowRun;
        private uint word;
        private int wordBits;
        private readonly List<uint> pending = new List<uint>();
        private List<uint> latched;

        // WS2812 reset is >50 us low; 32 slots = 10 us at 3.2 Mbit/s is far longer than any gap inside a frame
        private const int LatchSlots = 32;
    }
}
