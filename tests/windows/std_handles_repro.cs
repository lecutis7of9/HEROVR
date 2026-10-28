// 0.0.71 hotfix gate (Windows only, run by tests/std_handles_tests.py on the CI release build): the launcher's own
// PsRunner, after a Stop VR, starting the session's Python. Compiled with the launcher's src\*.cs (/main: this class)
// and started like Explorer starts the launcher (no console, no standard handles). Writes "key: value" lines.
//   legacy-handles  the launcher's standard handles after 0.0.70's Stop (FreeConsole, AttachConsole, Ctrl+C, FreeConsole)
//   legacy-python   0.0.70's PsRunner start after that Stop, 0.0.70's Python call (stdin not given)    -> expected FAILED WinError 6
//   python-fix      the same launcher state, Python with std_handles.repair()                         -> STARTED
//   launcher-fix    build 12's PsRunner.Start (clears dead handles), 0.0.70's Python call               -> STARTED
//   stop-fix        build 12's RequestStop, then build 12's PsRunner start and 0.0.70's Python call    -> STARTED
//   stop-legacy     the same, started like 0.0.70 (information: the handles that start inherited)
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading;

namespace Sm2VrLauncher
{
    static class StdHandlesRepro
    {
        [DllImport("kernel32.dll", SetLastError = true)] static extern bool AttachConsole(uint pid);
        [DllImport("kernel32.dll", SetLastError = true)] static extern bool FreeConsole();
        [DllImport("kernel32.dll")] static extern bool SetConsoleCtrlHandler(IntPtr handler, bool add);
        [DllImport("kernel32.dll")] static extern bool GenerateConsoleCtrlEvent(uint ctrlEvent, uint groupId);
        [DllImport("kernel32.dll")] static extern IntPtr GetStdHandle(int which);

        static StringBuilder log = new StringBuilder();
        static void Say(string k, string v) { log.Append(k).Append(": ").Append(v.Replace("\r", " ").Replace("\n", " | ")).Append("\r\n"); }

        static string Handles()
        {
            var parts = new List<string>();
            foreach (int w in new[] { -10, -11, -12 }) { IntPtr h = GetStdHandle(w); parts.Add(h.ToInt64() + (StdHandles.Usable(h) ? "" : "(dead)")); }
            return string.Join(",", parts.ToArray());
        }

        static ProcessStartInfo Ps(string cmd)   // exactly 0.0.70's PsRunner.Start ProcessStartInfo
        {
            var psi = new ProcessStartInfo("powershell.exe", "-NoProfile -NonInteractive -ExecutionPolicy Bypass -OutputFormat Text -EncodedCommand " +
                Convert.ToBase64String(Encoding.Unicode.GetBytes(cmd)));
            psi.UseShellExecute = false; psi.CreateNoWindow = true;
            psi.RedirectStandardOutput = true; psi.RedirectStandardError = true;
            return psi;
        }

        static string Run(ProcessStartInfo psi)
        {
            using (var p = Process.Start(psi))
            {
                string o = p.StandardOutput.ReadToEnd(), e = p.StandardError.ReadToEnd();
                p.WaitForExit(60000);
                return (o + " " + e).Trim();
            }
        }

        static void LegacyStop()   // 0.0.70's PsRunner.RequestStop, on a running hidden PowerShell
        {
            using (var p = Process.Start(Ps("Start-Sleep -Seconds 30")))
            {
                Thread.Sleep(3000);
                FreeConsole();
                bool attached = AttachConsole((uint)p.Id);
                SetConsoleCtrlHandler(IntPtr.Zero, true);
                GenerateConsoleCtrlEvent(0, 0);
                Thread.Sleep(300);
                FreeConsole();
                Say("legacy-attached", attached.ToString());
                if (!p.WaitForExit(15000)) { try { p.Kill(); } catch { } Say("legacy-stop", "killed"); } else Say("legacy-stop", "exit " + p.ExitCode);
            }
            Thread.Sleep(2500); SetConsoleCtrlHandler(IntPtr.Zero, false);
        }

        static int Main(string[] args)
        {
            string outFile = args[0], tools = args[1], python = args[2], child = args[3];
            string call = "& " + PsRunner.Ps1Literal(python) + " " + PsRunner.Ps1Literal(child) + " " + PsRunner.Ps1Literal(tools) + " ";
            try
            {
                Say("start-handles", Handles());
                LegacyStop();
                Say("legacy-handles", Handles());
                Say("legacy-python", Run(Ps(call + "0")));
                Say("python-fix", Run(Ps(call + "1")));
                // build 12's PsRunner (the real class): Start clears what is dead first
                var lines = new StringBuilder();
                int code = PsRunner.RunAndWait(Path.GetTempPath(), WriteScript(call + "0"), null, (l, err) => { lock (lines) lines.Append(l).Append(' '); }, CancellationToken.None);
                Say("launcher-fix", lines.ToString().Trim() + " (exit " + code + ")");
                Say("after-fix-handles", Handles());
                // build 12's RequestStop on a real session, then 0.0.70's start: the handles must still be good
                var r = new PsRunner(); var done = new ManualResetEvent(false);
                r.Exited += c => done.Set();
                r.Start(Path.GetTempPath(), WriteScript("Start-Sleep -Seconds 30"), null);
                Thread.Sleep(3000);
                Say("stop-sent", r.RequestStop().ToString());
                Say("stop-exited", done.WaitOne(15000).ToString());
                if (r.Running) r.ForceStop();
                Thread.Sleep(2500);
                Say("stop-handles", Handles());
                lines = new StringBuilder();
                code = PsRunner.RunAndWait(Path.GetTempPath(), WriteScript(call + "0"), null, (l, err) => { lock (lines) lines.Append(l).Append(' '); }, CancellationToken.None);
                Say("stop-fix", lines.ToString().Trim() + " (exit " + code + ")");
                Say("stop-legacy", Run(Ps(call + "0")));
            }
            catch (Exception ex) { Say("harness-error", ex.ToString()); }
            File.WriteAllText(outFile, log.ToString());
            return 0;
        }

        static string WriteScript(string body)
        {
            string p = Path.Combine(Path.GetTempPath(), "std-handles-" + Guid.NewGuid().ToString("N") + ".ps1");
            File.WriteAllText(p, body + "\r\nexit $LASTEXITCODE\r\n");
            return p;
        }
    }
}
