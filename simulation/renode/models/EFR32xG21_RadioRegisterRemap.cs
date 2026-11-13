// Presents an xG21 radio register block (FRC, AGC, MODEM, BUFC) on top of Renode's EFR32xG24_Radio model.
//
// The xG24 model implements the xG24 register layouts. Many xG21 registers moved between the two chips (AGC RSSI
// 0x10 -> 0x18, FRC FCD0 0xB4 -> 0x108, ...), so RAIL for xG21 would program the wrong registers. The model's region
// is registered at a shadow address and this peripheral sits at the real one, translating offsets:
//  - map "src=dst,src=-,...": src is an xG21 offset, dst the xG24 offset, "-" keeps an xG21-only register locally.
//  - Offsets not in the map are forwarded unchanged. SET/CLR/TGL aliases (+0x1000/2000/3000) are preserved.
// The maps are generated from Silicon Labs' radio configurator register maps (claude/mg21seq/remap_tables.py).
//
// BUFC: on xG21 a buffer's ADDR holds the low 24 bits and RAMBASEADDR the top 16 bits of its RAM address, while the
// xG24 model expects full addresses. With composeBufferAddresses the two are combined.

using System;
using System.Collections.Generic;
using System.Globalization;

using Antmicro.Renode.Core;
using Antmicro.Renode.Peripherals.Bus;

namespace Antmicro.Renode.Peripherals.Wireless
{
    public class EFR32xG21_RadioRegisterRemap : IDoubleWordPeripheral, IKnownSize
    {
        public EFR32xG21_RadioRegisterRemap(IMachine machine, ulong target, string map, bool composeBufferAddresses = false)
        {
            this.machine = machine;
            this.target = target;
            this.composeBufferAddresses = composeBufferAddresses;
            foreach(var entry in map.Split(new[] { ',' }, StringSplitOptions.RemoveEmptyEntries))
            {
                var parts = entry.Split('=');
                var source = ParseHex(parts[0]);
                remap[source] = parts[1] == "-" ? Local : ParseHex(parts[1]);
            }
            Reset();
        }

        public long Size => 0x4000;

        public uint ReadDoubleWord(long offset)
        {
            var register = offset & BaseMask;
            if(composeBufferAddresses && IsBufferAddress(register))
            {
                return local.TryGetValue(register, out var raw) ? raw : 0;
            }
            if(Translate(register, out var mapped))
            {
                return machine.SystemBus.ReadDoubleWord(target + (ulong)((offset & AliasMask) | mapped));
            }
            return local.TryGetValue(register, out var value) ? value : 0;
        }

        public void WriteDoubleWord(long offset, uint value)
        {
            var register = offset & BaseMask;
            var alias = offset & AliasMask;

            if(composeBufferAddresses && (IsBufferAddress(register) || register == RamBaseAddress))
            {
                local[register] = Apply(alias, local.TryGetValue(register, out var current) ? current : 0, value);
                for(var n = 0; n < BufferCount; n++)
                {
                    ForwardBufferAddress(BufferAddress(n));
                }
                return;
            }
            if(Translate(register, out var mapped))
            {
                machine.SystemBus.WriteDoubleWord(target + (ulong)(alias | mapped), value);
                return;
            }
            local[register] = Apply(alias, local.TryGetValue(register, out var old) ? old : 0, value);
        }

        public void Reset()
        {
            local.Clear();
            if(composeBufferAddresses)
            {
                local[RamBaseAddress] = RamBaseAddressReset;
            }
        }

        private bool Translate(long register, out long mapped)
        {
            if(!remap.TryGetValue(register, out mapped))
            {
                mapped = register;
            }
            return mapped != Local;
        }

        private void ForwardBufferAddress(long register)
        {
            var raw = local.TryGetValue(register, out var value) ? value : 0;
            var baseAddress = local[RamBaseAddress];
            Translate(register, out var mapped);
            machine.SystemBus.WriteDoubleWord(target + (ulong)mapped, (baseAddress & 0xFFFF0000) | (raw & 0x00FFFFFF));
        }

        private static uint Apply(long alias, uint current, uint value)
        {
            switch(alias)
            {
            case SetAlias:
                return current | value;
            case ClearAlias:
                return current & ~value;
            case ToggleAlias:
                return current ^ value;
            }
            return value;
        }

        private static bool IsBufferAddress(long register)
        {
            for(var n = 0; n < BufferCount; n++)
            {
                if(register == BufferAddress(n))
                {
                    return true;
                }
            }
            return false;
        }

        private static long BufferAddress(int n)
        {
            return 0x0C + 0x30 * n;
        }

        private static long ParseHex(string text)
        {
            text = text.Trim();
            if(text.StartsWith("0x", StringComparison.OrdinalIgnoreCase))
            {
                text = text.Substring(2);
            }
            return long.Parse(text, NumberStyles.HexNumber);
        }

        private readonly IMachine machine;
        private readonly ulong target;
        private readonly bool composeBufferAddresses;
        private readonly Dictionary<long, long> remap = new Dictionary<long, long>();
        private readonly Dictionary<long, uint> local = new Dictionary<long, uint>();

        private const long Local = -1;
        private const long BaseMask = 0xFFF;
        private const long AliasMask = 0x3000;
        private const long SetAlias = 0x1000;
        private const long ClearAlias = 0x2000;
        private const long ToggleAlias = 0x3000;
        private const int BufferCount = 4;
        private const long RamBaseAddress = 0xF0;
        private const uint RamBaseAddressReset = 0x20000000;
    }
}
