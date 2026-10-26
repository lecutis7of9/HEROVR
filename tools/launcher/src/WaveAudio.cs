// HERO VR launcher - the new design's sound, through winmm waveOut (P/Invoke, no SoundPlayer): the intro once, then
// the ambient loop, sample-exact and seamless, until the launcher closes or a game starts (then a short fade out).
// Four 50 ms buffers are refilled from a background thread. Any failure (no sound device, no winmm) is silent.
using System;
using System.Runtime.InteropServices;
using System.Threading;

namespace Sm2VrLauncher
{
    class WaveAudio : IDisposable
    {
        [StructLayout(LayoutKind.Sequential, Pack = 2)]
        struct WAVEFORMATEX { public ushort wFormatTag, nChannels; public uint nSamplesPerSec, nAvgBytesPerSec; public ushort nBlockAlign, wBitsPerSample, cbSize; }
        [StructLayout(LayoutKind.Sequential)]
        struct WAVEHDR { public IntPtr lpData; public uint dwBufferLength, dwBytesRecorded; public IntPtr dwUser; public uint dwFlags, dwLoops; public IntPtr lpNext, reserved; }

        [DllImport("winmm.dll")] static extern int waveOutOpen(out IntPtr hwo, uint device, ref WAVEFORMATEX fmt, IntPtr callback, IntPtr instance, uint flags);
        [DllImport("winmm.dll")] static extern int waveOutPrepareHeader(IntPtr hwo, IntPtr hdr, int size);
        [DllImport("winmm.dll")] static extern int waveOutUnprepareHeader(IntPtr hwo, IntPtr hdr, int size);
        [DllImport("winmm.dll")] static extern int waveOutWrite(IntPtr hwo, IntPtr hdr, int size);
        [DllImport("winmm.dll")] static extern int waveOutReset(IntPtr hwo);
        [DllImport("winmm.dll")] static extern int waveOutClose(IntPtr hwo);

        const uint WAVE_MAPPER = 0xFFFFFFFF, CALLBACK_EVENT = 0x50000;
        const int WHDR_DONE = 1, Buffers = 4;

        readonly Pcm intro, loop;
        readonly int frameSamples;    // samples per buffer (all channels)
        long pos;                     // position in the intro, then (past its end) in the loop
        volatile float target = 1f, step;
        float gain;
        volatile bool stopping, closed;
        Thread thread;
        IntPtr hwo;
        readonly AutoResetEvent done = new AutoResetEvent(false);
        public string Error;
        public const float Volume = 0.9f;

        /// <summary>skipIntro: start straight in the ambient loop (sound turned back on in Options).</summary>
        public WaveAudio(Pcm intro, Pcm loop, bool skipIntro)
        {
            this.intro = intro; this.loop = loop;
            frameSamples = intro.Rate / 20 * intro.Channels;
            if (skipIntro) pos = intro.Samples.Length;
        }

        /// <summary>Opens the sound device and starts playing with a fade in. False (and Error) when there is no sound.</summary>
        public bool Start(double fadeInSeconds)
        {
            try
            {
                var fmt = new WAVEFORMATEX
                {
                    wFormatTag = 1, nChannels = (ushort)intro.Channels, nSamplesPerSec = (uint)intro.Rate, wBitsPerSample = 16,
                    nBlockAlign = (ushort)(2 * intro.Channels), nAvgBytesPerSec = (uint)(intro.Rate * 2 * intro.Channels), cbSize = 0
                };
                int r = waveOutOpen(out hwo, WAVE_MAPPER, ref fmt, done.SafeWaitHandle.DangerousGetHandle(), IntPtr.Zero, CALLBACK_EVENT);
                if (r != 0) { Error = "waveOutOpen error " + r; hwo = IntPtr.Zero; return false; }
                gain = 0; target = 1f; step = Step(fadeInSeconds);
                thread = new Thread(Run) { IsBackground = true, Name = "launcher sound", Priority = ThreadPriority.AboveNormal };
                thread.Start();
                return true;
            }
            catch (Exception ex) { Error = ex.GetType().Name + ": " + ex.Message; hwo = IntPtr.Zero; return false; }
        }

        float Step(double seconds) { return (float)(1.0 / Math.Max(1, seconds * intro.Rate)); }

        /// <summary>Fades out over `ms` and closes the device (the thread ends by itself).</summary>
        public void FadeOut(int ms)
        {
            step = Step(ms / 1000.0); target = 0f; stopping = true;
            done.Set();
        }

        public bool Playing { get { return thread != null && thread.IsAlive && !stopping; } }

        /// <summary>The next sample: the intro once, then the loop wrapped forever (no gap at the seam).</summary>
        short Next()
        {
            if (pos < intro.Samples.Length) return intro.Samples[pos++];
            long lp = (pos - intro.Samples.Length) % loop.Samples.Length;
            pos++;
            if (pos - intro.Samples.Length >= loop.Samples.Length * 1000L) pos -= loop.Samples.Length * 999L;   // stays small
            return loop.Samples[lp];
        }

        /// <summary>Fills one buffer; true when it was all silence after a fade out.</summary>
        bool Fill(short[] buf)
        {
            bool silent = true;
            int ch = intro.Channels;
            for (int i = 0; i < buf.Length; i += ch)
            {
                if (gain < target) gain = Math.Min(target, gain + step);
                else if (gain > target) gain = Math.Max(target, gain - step);
                float gv = gain * Volume;
                for (int c = 0; c < ch; c++)
                {
                    short s = Next();
                    buf[i + c] = (short)(s * gv);
                }
                if (gv > 0) silent = false;
            }
            return silent;
        }

        void Run()
        {
            int hsize = Marshal.SizeOf(typeof(WAVEHDR));
            int flagsAt = (int)Marshal.OffsetOf(typeof(WAVEHDR), "dwFlags");
            var hdrs = new IntPtr[Buffers];
            var datas = new IntPtr[Buffers];
            var queued = new bool[Buffers];
            var buf = new short[frameSamples];
            int silentAfterStop = 0;
            try
            {
                for (int i = 0; i < Buffers; i++)
                {
                    datas[i] = Marshal.AllocHGlobal(buf.Length * 2);
                    hdrs[i] = Marshal.AllocHGlobal(hsize);
                    var h = new WAVEHDR { lpData = datas[i], dwBufferLength = (uint)(buf.Length * 2) };
                    Marshal.StructureToPtr(h, hdrs[i], false);
                    int r = waveOutPrepareHeader(hwo, hdrs[i], hsize);
                    if (r != 0) throw new InvalidOperationException("waveOutPrepareHeader error " + r);
                }
                while (true)
                {
                    for (int i = 0; i < Buffers; i++)
                    {
                        if (queued[i] && (Marshal.ReadInt32(hdrs[i], flagsAt) & WHDR_DONE) == 0) continue;
                        bool silent = Fill(buf);
                        if (stopping && silent) silentAfterStop++;
                        Marshal.Copy(buf, 0, datas[i], buf.Length);
                        int r = waveOutWrite(hwo, hdrs[i], hsize);
                        if (r != 0) throw new InvalidOperationException("waveOutWrite error " + r);
                        queued[i] = true;
                    }
                    // Once a whole round of silence is queued, everything audible has played: close.
                    if (stopping && silentAfterStop > Buffers) break;
                    done.WaitOne(100);
                }
                // Let the last (silent) buffers drain.
                for (int n = 0; n < 20; n++)
                {
                    bool all = true;
                    for (int i = 0; i < Buffers; i++) if (queued[i] && (Marshal.ReadInt32(hdrs[i], flagsAt) & WHDR_DONE) == 0) all = false;
                    if (all) break;
                    done.WaitOne(25);
                }
            }
            catch (Exception ex) { Error = ex.Message; }
            finally
            {
                try { waveOutReset(hwo); } catch { }
                for (int i = 0; i < Buffers; i++)
                {
                    try { if (hdrs[i] != IntPtr.Zero) waveOutUnprepareHeader(hwo, hdrs[i], hsize); } catch { }
                }
                try { waveOutClose(hwo); } catch { }
                for (int i = 0; i < Buffers; i++)
                {
                    if (hdrs[i] != IntPtr.Zero) Marshal.FreeHGlobal(hdrs[i]);
                    if (datas[i] != IntPtr.Zero) Marshal.FreeHGlobal(datas[i]);
                }
                closed = true;
            }
        }

        /// <summary>Fades out quickly and waits (at most `waitMs`) for the device to close.</summary>
        public void Dispose() { Close(250, 600); }
        public void Close(int fadeMs, int waitMs)
        {
            if (thread == null) return;
            FadeOut(fadeMs);
            try { thread.Join(waitMs); } catch { }
        }
        public bool Closed { get { return closed; } }
    }

    /// <summary>The first connected Xbox-style controller: D-pad up/down and A (B goes back), polled while the launcher is in front.</summary>
    static class Pad
    {
        [StructLayout(LayoutKind.Sequential)]
        struct XINPUT_STATE { public uint dwPacketNumber; public ushort wButtons; public byte bLeftTrigger, bRightTrigger; public short sThumbLX, sThumbLY, sThumbRX, sThumbRY; }
        [DllImport("xinput1_4.dll", EntryPoint = "XInputGetState")] static extern int Get14(int user, out XINPUT_STATE s);
        [DllImport("xinput9_1_0.dll", EntryPoint = "XInputGetState")] static extern int Get910(int user, out XINPUT_STATE s);
        public const int Up = 0x1, Down = 0x2, A = 0x1000, B = 0x2000;
        static int api;          // 0 untried, 1 xinput1_4, 2 xinput9_1_0, -1 none
        static int user = -1;
        static DateTime nextScan = DateTime.MinValue;

        /// <summary>The buttons held now (0 when there is no controller or no XInput).</summary>
        public static int Buttons()
        {
            if (api < 0) return 0;
            if (user < 0)
            {
                if (DateTime.UtcNow < nextScan) return 0;   // looking for a controller costs time: twice a second at most
                nextScan = DateTime.UtcNow.AddMilliseconds(500);
                for (int u = 0; u < 4; u++) { int b; if (Read(u, out b)) { user = u; return b; } if (api < 0) return 0; }
                return 0;
            }
            int bt;
            if (Read(user, out bt)) return bt;
            user = -1;
            return 0;
        }

        static bool Read(int u, out int buttons)
        {
            buttons = 0;
            XINPUT_STATE s;
            try
            {
                if (api == 0 || api == 1)
                {
                    try { int r = Get14(u, out s); api = 1; if (r != 0) return false; buttons = s.wButtons; return true; }
                    catch (DllNotFoundException) { api = 2; }
                    catch (EntryPointNotFoundException) { api = 2; }
                }
                int r2 = Get910(u, out s);
                if (r2 != 0) return false;
                buttons = s.wButtons; return true;
            }
            catch (Exception) { api = -1; return false; }
        }
    }
}
