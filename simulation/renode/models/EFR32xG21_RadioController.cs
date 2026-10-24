// xG21 RAC with an interpreter for its microcoded radio sequencer.
//
// Renode only models the xG24/xG26 radio, whose sequencer is a Cortex-M0+. On xG21 the sequencer is a proprietary
// 16-bit engine (RAC SEQCMD/SEQSTATUS/R0-R7/STIMER/VECTADDR) that runs the microcode RAIL copies into SEQRAM. This
// peripheral takes the RAC address range from EFR32xG24_Radio:
//  - Offsets below 0x28 (the radio state machine, CMD, IF/IEN, FORCESTATE, RXENSRCEN) have the same layout on both
//    chips and are forwarded to the xG24 model.
//  - From 0x28 on the layouts diverge. `map` ("src=dst,src=-,...", claude/mg21seq/remap_tables.py) moves xG21 offsets
//    to the model's, or keeps them here ("-"), like the sequencer registers. Unlisted offsets are forwarded unchanged.
//  - The model raises one Seq*IRQ per radio state for the sequencer and only advances once the matching state flag is
//    cleared. Each rising edge runs that state's microcode vector, then clears the flag, which is what the M0+
//    firmware does on xG24.
// The instruction set was reverse engineered from the RAIL image (claude/mg21seq/, claude/mg21-rail-sequencer.md).
// Waits complete immediately and STIMER advances with executed instructions, so this is functional, not timed.

using System;
using System.Collections.Generic;
using System.Globalization;

using Antmicro.Renode.Core;
using Antmicro.Renode.Logging;
using Antmicro.Renode.Peripherals.Bus;
using Antmicro.Renode.Time;

namespace Antmicro.Renode.Peripherals.Wireless
{
    public class EFR32xG21_RadioController : IDoubleWordPeripheral, IGPIOReceiver, IKnownSize
    {
        public EFR32xG21_RadioController(IMachine machine, EFR32xG24_Radio radio, string map)
        {
            this.machine = machine;
            this.radio = radio;
            foreach(var entry in map.Split(new[] { ',' }, StringSplitOptions.RemoveEmptyEntries))
            {
                var parts = entry.Split('=');
                var source = long.Parse(parts[0].Trim().Substring(2), NumberStyles.HexNumber);
                remap[source] = parts[1] == "-" ? Local : long.Parse(parts[1].Trim().Substring(2), NumberStyles.HexNumber);
            }
            Reset();
        }

        public long Size => 0x4000;

        public ulong VectorRuns { get; private set; }

        public ulong ExecutedInstructions { get; private set; }

        public uint ReadDoubleWord(long offset)
        {
            EnsureModelSequencerInterrupts();
            var register = offset & BaseMask;
            if(Translate(register, out var mapped))
            {
                return radio.ReadDoubleWordFromRadioController((offset & AliasMask) | mapped);
            }
            return ReadLocal(register);
        }

        public void WriteDoubleWord(long offset, uint value)
        {
            EnsureModelSequencerInterrupts();
            var register = offset & BaseMask;
            if(Translate(register, out var mapped))
            {
                if(register == Command)
                {
                    this.Log(LogLevel.Debug, "CMD write 0x{0:X} (alias 0x{1:X}) by {2}, STATUS=0x{3:X}", value, offset & AliasMask,
                        runningState.HasValue ? $"sequencer state {runningState} pc=0x{pc:X}" : "host", radio.ReadDoubleWordFromRadioController(Status));
                    // After a PROTIMER-started TX, the TX vector latches TXEN to hold the transmission until TX done.
                    // The model already cleared TXEN when the frame started, and at TX done it would take the latched
                    // TXEN as a request for a second frame (TX2TX with an empty FIFO). So drop it.
                    if(runningState == TxState)
                    {
                        value &= ~CommandTxEnable;
                    }
                }
                radio.WriteDoubleWordToRadioController((offset & AliasMask) | mapped, value);
                return;
            }

            var current = ReadLocal(register);
            switch(offset & AliasMask)
            {
            case SetAlias:
                value = current | value;
                break;
            case ClearAlias:
                value = current & ~value;
                break;
            case ToggleAlias:
                value = current ^ value;
                break;
            }
            if(register == SequencerCommand)
            {
                this.Log(LogLevel.Debug, "SEQCMD 0x{0:X}", value);
                return;
            }
            local[register] = value;
        }

        public void OnGPIO(int number, bool value)
        {
            if(!value || pending.Contains(number))
            {
                return;
            }
            pending.Add(number);
            machine.ScheduleAction(TimeInterval.Empty, _ =>
            {
                pending.Remove(number);
                if(number == TxPoweringDownState || number == Tx2RxState || number == Tx2TxState)
                {
                    // xG21 RAIL learns about TX completion from FRC IF.TXRAWEVENT, with the result in SR0 bits 10-12
                    // (all clear = sent). The microcode only raises it for blocked TXs, so the hardware raises it when a
                    // frame ends. These states are only entered from TX. The xG24 model raises TXDONE instead.
                    machine.SystemBus.WriteDoubleWord(FrameControllerInterruptFlagsSet, FrameControllerTxRawEvent);
                }
                RunVector(number);
            });
        }

        public void Reset()
        {
            local.Clear();
            pending.Clear();
            for(var i = 0; i < r.Length; i++)
            {
                r[i] = 0;
            }
            modelInterruptsEnabled = false;
        }

        private bool Translate(long register, out long mapped)
        {
            if(!remap.TryGetValue(register, out mapped))
            {
                mapped = register;
            }
            return mapped != Local;
        }

        private uint ReadLocal(long register)
        {
            switch(register)
            {
            case SequencerStatus:
                return SequencerStatusDone;
            case SequencerTimer:
                return (uint)(ExecutedInstructions & 0xFFFF);
            }
            return local.TryGetValue(register, out var value) ? value : 0;
        }

        private void EnsureModelSequencerInterrupts()
        {
            if(modelInterruptsEnabled)
            {
                return;
            }
            modelInterruptsEnabled = true;
            radio.WriteDoubleWordToRadioController(ModelSequencerInterruptEnable, StateInterruptsMask);
        }

        private void RunVector(int state)
        {
            VectorRuns++;
            var vectorBase = (ReadLocal(VectorAddress) & 0xFFFF);
            pc = (uint)(vectorBase + 4 * state);
            stack.Clear();
            runningState = state;
            var steps = 0;
            try
            {
                while(Step())
                {
                    if(++steps > StepBudget)
                    {
                        this.Log(LogLevel.Warning, "State {0} vector did not finish within {1} instructions (pc=0x{2:X})", state, StepBudget, pc);
                        break;
                    }
                }
            }
            catch(InvalidOperationException e)
            {
                this.Log(LogLevel.Error, "State {0} vector: {1}", state, e.Message);
            }
            runningState = null;
            this.Log(LogLevel.Debug, "State {0} vector ran {1} instructions, ended at 0x{2:X}", state, steps, pc);
            radio.WriteDoubleWordToRadioController(ClearAlias | ModelSequencerInterruptFlags, 1u << (16 + state));
        }

        private ushort Fetch(uint address)
        {
            return (ushort)machine.SystemBus.ReadWord(SequencerRam + (address & 0xFFFF));
        }

        private uint Load(uint address)
        {
            return machine.SystemBus.ReadDoubleWord(address & ~3u);
        }

        private void Store(uint address, uint value)
        {
            machine.SystemBus.WriteDoubleWord(address & ~3u, value);
        }

        private uint Flags(uint value)
        {
            zero = value == 0;
            negative = (value & 0x80000000) != 0;
            return value;
        }

        private uint Add(uint a, uint b)
        {
            var total = (ulong)a + b;
            carry = total > 0xFFFFFFFF;
            return Flags((uint)total);
        }

        private bool Condition(int c)
        {
            switch(c)
            {
            case 0: return true;
            case 1: return zero;
            case 2: return !zero;
            case 3: return carry;
            case 4: return !carry;
            case 5: return negative;
            case 6: return !negative;
            }
            throw new InvalidOperationException($"unknown branch condition {c} at 0x{pc:X}");
        }

        private static int Sign8(int x)
        {
            return x >= 0x80 ? x - 0x100 : x;
        }

        private bool Step()
        {
            ExecutedInstructions++;
            var h = Fetch(pc);
            var op = h >> 12;
            var m = (h >> 11) & 1;
            var rr = (h >> 8) & 7;
            var x = h & 0xFF;
            var hi = x >> 4;
            var lo = x & 0xF;
            var next = pc + 2;

            if(op == 0xC && x == 0 && m == 0)
            {
                r[rr] = Fetch(pc + 2) | (uint)Fetch(pc + 4) << 16;
                next = pc + 6;
            }
            else if(op == 0xC && x == 0)
            {
                r[rr] = ReadLocal(WaitSnapshot);
            }
            else if(h == 0x9806)
            {
                stack.Push(pc + 4);
                next = Fetch(pc + 2);
            }
            else if(h == 0x9802)
            {
                next = Fetch(pc + 2);
            }
            else if(h == 0xA000)
            {
                if(stack.Count == 0)
                {
                    return false;
                }
                // The dispatcher pushes handler addresses with flags in the top bits
                next = stack.Pop() & 0xFFFF;
            }
            else if(op == 0xF && m == 1)
            {
                if(x != 0)
                {
                    radio.WriteDoubleWordToRadioController(SetAlias | InterruptFlags, (uint)x << 16);
                }
                return false;
            }
            else if((h & 0xF80F) == 0x8800)
            {
                stack.Push(r[hi & 7]);
            }
            else if(op == 9 && m == 0 && x == 0)
            {
                r[rr] = stack.Pop();
            }
            else if(op == 7 && m == 0)
            {
                if(Condition(rr))
                {
                    next = (uint)(pc + Sign8(x));
                }
            }
            else if(op == 7)
            {
                var kind = x >> 5;
                var bit = x & 0x1F;
                if(kind == 4)
                {
                    zero = ((r[rr] >> bit) & 1) == 0;
                }
                else if(kind <= 3)
                {
                    var address = kind <= 1 ? r[rr] : RadioControllerBase + 4 * r[rr];
                    var value = Load(address);
                    Store(address, (kind & 1) != 0 ? value | (1u << bit) : value & ~(1u << bit));
                }
                else
                {
                    throw new InvalidOperationException($"unknown bit operation 0x{h:X4} at 0x{pc:X}");
                }
            }
            else if(op == 1 && lo == 4)
            {
                r[rr] = Load(r[hi & 7]);
            }
            else if(op == 1 && lo == 8)
            {
                Store(r[rr], r[hi & 7]);
            }
            else if(op == 1 && lo == 1 && m == 1)
            {
                Store(RadioControllerBase + StorageRegister0 + 4 * (uint)(rr & 3), r[hi & 7]);
            }
            else if(op == 1 && lo == 1 && hi < 8)
            {
                r[rr] = r[hi];
            }
            else if(op == 1 && lo == 1)
            {
                r[rr] = Load(RadioControllerBase + StorageRegister0 + 4 * (uint)(hi & 3));
            }
            else if(op == 2 && m == 0)
            {
                r[rr] = Add(r[rr], (uint)Sign8(x));
            }
            else if(op == 2 && lo == 1)
            {
                r[rr] = Add(r[rr], r[hi & 7]);
            }
            else if(op == 2 && lo == 0)
            {
                Add(r[rr], r[hi & 7]);
            }
            else if(op == 3 && m == 0)
            {
                r[rr] = Flags(r[rr] & (uint)x);
            }
            else if(op == 3 && lo == 1)
            {
                r[rr] = Flags(r[rr] & r[hi & 7]);
            }
            else if(op == 3 && lo == 0)
            {
                Flags(r[rr] & r[hi & 7]);
            }
            else if(op == 4 && m == 1)
            {
                // Also walks pointers between registers of one peripheral, e.g. BUFC IF 0xE0 ^ 0x80 -> BUF1 CMD 0x60
                r[rr] = Flags(r[rr] ^ (uint)x);
            }
            else if(op == 4 && lo == 1)
            {
                r[rr] = Flags(r[rr] | r[hi & 7]);
            }
            else if(op == 4 && lo == 0)
            {
                r[rr] = Flags(r[hi & 7]);
            }
            else if(op == 5 && m == 0 && lo == 0)
            {
                Flags(r[rr] - r[hi & 7]);
            }
            else if(op == 5 && m == 0)
            {
                r[rr] = Flags(r[rr] ^ r[hi & 7]);
            }
            else if(op == 5 && lo == 1)
            {
                r[rr] = Flags(~r[rr]);
            }
            else if(op == 5 && lo == 3)
            {
                r[rr] = Flags((uint)-(int)r[rr]);
            }
            else if(op == 6)
            {
                // x[7:6] type (0 ROR, 1 ASR, 2 LSR, LSL when m=1); x[5]: amount from register x[2:0]
                var amount = (int)(((x & 0x20) != 0 ? r[x & 7] : (uint)x) & 0x1F);
                var kind = x >> 6;
                var value = r[rr];
                if(m == 1 && kind == 2)
                {
                    value <<= amount;
                }
                else if(m == 0 && kind == 0)
                {
                    value = amount == 0 ? value : (value >> amount) | (value << (32 - amount));
                }
                else if(m == 0 && kind == 1)
                {
                    value = (uint)((int)value >> amount);
                }
                else if(m == 0 && kind == 2)
                {
                    value >>= amount;
                }
                else
                {
                    throw new InvalidOperationException($"unknown shift 0x{h:X4} at 0x{pc:X}");
                }
                r[rr] = Flags(value);
            }
            else if(op == 0xB && m == 0)
            {
                r[rr] = Load(RadioControllerBase + 4 * (uint)x);
            }
            else if(op == 0xB)
            {
                r[rr] = (uint)x;
            }
            else if(op == 0xA && m == 1)
            {
                Store(RadioControllerBase + 4 * (uint)x, r[rr]);
            }
            else if(op == 0xD && m == 1)
            {
                Flags(r[rr] & (uint)x);
            }
            else if((op == 0xE || op == 0) && m == 1)
            {
                // Waits: events and delays complete immediately
            }
            else
            {
                throw new InvalidOperationException($"unknown instruction 0x{h:X4} at 0x{pc:X}");
            }

            pc = next;
            return true;
        }

        private readonly IMachine machine;
        private readonly EFR32xG24_Radio radio;
        private readonly Dictionary<long, long> remap = new Dictionary<long, long>();
        private readonly Dictionary<long, uint> local = new Dictionary<long, uint>();
        private readonly HashSet<int> pending = new HashSet<int>();
        private readonly uint[] r = new uint[8];
        private readonly Stack<uint> stack = new Stack<uint>();
        private uint pc;
        private bool zero;
        private bool negative;
        private bool carry;
        private bool modelInterruptsEnabled;
        private int? runningState;

        private const uint RadioControllerBase = 0xB8020000;
        private const uint SequencerRam = 0xB0000000;
        private const int StepBudget = 200000;

        private const long BaseMask = 0xFFF;
        private const long AliasMask = 0x3000;
        private const long SetAlias = 0x1000;
        private const long ClearAlias = 0x2000;
        private const long ToggleAlias = 0x3000;

        // xG21 RAC
        private const long Status = 0x0C;
        private const long Command = 0x10;
        private const uint CommandTxEnable = 0x1;
        private const int TxState = 9;
        private const int TxPoweringDownState = 10;
        private const int Tx2RxState = 11;
        private const int Tx2TxState = 12;
        private const uint FrameControllerInterruptFlagsSet = 0xB8005068;
        private const uint FrameControllerTxRawEvent = 1u << 15;
        private const long InterruptFlags = 0x1C;
        private const long SequencerStatus = 0x28;
        private const long SequencerCommand = 0x2C;
        private const long WaitSnapshot = 0x58;
        private const long SequencerTimer = 0x5C;
        private const long VectorAddress = 0x64;
        private const uint StorageRegister0 = 0x70;
        private const uint SequencerStatusDone = 0x10;

        // xG24 model, reached through the forwarded range
        private const long ModelSequencerInterruptFlags = 0x28;
        private const long ModelSequencerInterruptEnable = 0x2C;
        private const uint StateInterruptsMask = 0x3FFF0000;
        private const long Local = -1;
    }
}
