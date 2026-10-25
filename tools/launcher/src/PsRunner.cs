// SM2 VR Launcher - runs the mod's PowerShell scripts hidden, streams their output, and stops them gently.
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Management;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading;

namespace Sm2VrLauncher
{
    class PsRunner
    {
        public event Action<string, bool> Output;   // line, isError
        public event Action<int> Exited;            // exit code (-1 when it could not start)
        public string Name;
        Process proc;
        public bool Running { get { try { return proc != null && !proc.HasExited; } catch { return false; } } }
        public int Pid { get { try { return proc == null ? 0 : proc.Id; } catch { return 0; } } }
        public DateTime Started;

        public static string Ps1Literal(string s) { return "'" + s.Replace("'", "''") + "'"; }

        /// <summary>Starts `& 'script.ps1' args` in a hidden PowerShell. Arguments are PowerShell text (already quoted).</summary>
        public bool Start(string workDir, string script, IEnumerable<string> args)
        {
            string argText = args == null ? "" : string.Join(" ", args);
            string cmd =
                "[Console]::OutputEncoding = New-Object System.Text.UTF8Encoding $false; " +
                "$ProgressPreference = 'SilentlyContinue'; " +
                "try { & " + Ps1Literal(script) + " " + argText + "; if ($LASTEXITCODE) { exit $LASTEXITCODE } else { exit 0 } } " +
                "catch { [Console]::Error.WriteLine('ERROR: ' + $_.Exception.Message); exit 1 }";
            var psi = new ProcessStartInfo("powershell.exe",
                "-NoProfile -NonInteractive -ExecutionPolicy Bypass -EncodedCommand " + Convert.ToBase64String(Encoding.Unicode.GetBytes(cmd)));
            psi.WorkingDirectory = workDir;
            psi.UseShellExecute = false;
            psi.CreateNoWindow = true;
            psi.RedirectStandardOutput = true;
            psi.RedirectStandardError = true;
            psi.StandardOutputEncoding = new UTF8Encoding(false);
            psi.StandardErrorEncoding = new UTF8Encoding(false);
            psi.EnvironmentVariables["PYTHONUNBUFFERED"] = "1";
            psi.EnvironmentVariables["PYTHONIOENCODING"] = "utf-8";
            psi.EnvironmentVariables["SM2VR_LAUNCHER"] = "1";
            try
            {
                proc = new Process { StartInfo = psi, EnableRaisingEvents = true };
                proc.OutputDataReceived += (s, e) => { if (e.Data != null && Output != null) Output(e.Data, false); };
                proc.ErrorDataReceived += (s, e) => { if (e.Data != null && Output != null) Output(e.Data, true); };
                proc.Exited += (s, e) =>
                {
                    int code = -1;
                    try { proc.WaitForExit(); code = proc.ExitCode; } catch { }
                    if (Exited != null) Exited(code);
                };
                proc.Start();
                Started = DateTime.Now;
                proc.BeginOutputReadLine();
                proc.BeginErrorReadLine();
                return true;
            }
            catch (Exception ex)
            {
                if (Output != null) Output("Could not start PowerShell: " + ex.Message, true);
                if (Exited != null) Exited(-1);
                return false;
            }
        }

        /// <summary>Runs a script to the end on the calling (background) thread; returns its exit code.</summary>
        public static int RunAndWait(string workDir, string script, IEnumerable<string> args, Action<string, bool> output, CancellationToken cancel)
        {
            var r = new PsRunner();
            var done = new ManualResetEvent(false);
            int code = -1;
            r.Output += output;
            r.Exited += c => { code = c; done.Set(); };
            if (!r.Start(workDir, script, args)) return -1;
            while (!done.WaitOne(250))
            {
                if (cancel.IsCancellationRequested) { r.RequestStop(); done.WaitOne(60000); break; }
            }
            done.WaitOne(2000);   // let the last output lines arrive
            return code;
        }

        // ---- Stopping ------------------------------------------------------------------------------------
        // The VR session (run_game_vr.py) puts the game's settings back when it ends normally, so the launcher asks it
        // to end with Ctrl+C (exactly as the user pressing Ctrl+C in a PowerShell window), never by killing it.

        [DllImport("kernel32.dll", SetLastError = true)] static extern bool AttachConsole(uint pid);
        [DllImport("kernel32.dll", SetLastError = true)] static extern bool FreeConsole();
        [DllImport("kernel32.dll")] static extern bool SetConsoleCtrlHandler(IntPtr handler, bool add);
        [DllImport("kernel32.dll")] static extern bool GenerateConsoleCtrlEvent(uint ctrlEvent, uint groupId);
        static readonly object consoleLock = new object();

        /// <summary>Sends Ctrl+C to the script's console (PowerShell and the Python it started). True if it was sent.</summary>
        public bool RequestStop()
        {
            if (!Running) return false;
            lock (consoleLock)
            {
                try
                {
                    FreeConsole();
                    if (!AttachConsole((uint)proc.Id)) return false;
                    SetConsoleCtrlHandler(IntPtr.Zero, true);   // the launcher itself ignores it
                    GenerateConsoleCtrlEvent(0, 0);
                    Thread.Sleep(300);
                    FreeConsole();
                }
                catch { return false; }
                finally
                {
                    var t = new Thread(() => { Thread.Sleep(2000); SetConsoleCtrlHandler(IntPtr.Zero, false); });
                    t.IsBackground = true; t.Start();
                }
            }
            return true;
        }

        /// <summary>The processes this run started (any depth), except the game, which is never touched.</summary>
        public List<int> Descendants()
        {
            var result = new List<int>();
            if (proc == null) return result;
            try
            {
                var parent = new Dictionary<int, int>();
                var name = new Dictionary<int, string>();
                using (var s = new ManagementObjectSearcher("SELECT ProcessId, ParentProcessId, Name FROM Win32_Process"))
                    foreach (ManagementObject o in s.Get())
                    {
                        int id = Convert.ToInt32(o["ProcessId"]);
                        parent[id] = Convert.ToInt32(o["ParentProcessId"]);
                        name[id] = Convert.ToString(o["Name"]);
                    }
                var frontier = new Queue<int>();
                frontier.Enqueue(proc.Id);
                while (frontier.Count > 0)
                {
                    int p = frontier.Dequeue();
                    foreach (var kv in parent)
                        if (kv.Value == p && !result.Contains(kv.Key) && kv.Key != p)
                        {
                            if (!name[kv.Key].Equals(SystemInfo.GameExe, StringComparison.OrdinalIgnoreCase)) result.Add(kv.Key);
                            frontier.Enqueue(kv.Key);
                        }
                }
            }
            catch { }
            return result;
        }

        /// <summary>Last resort: ends PowerShell and the Python session (not the game). Game settings may stay as VR left them.</summary>
        public void ForceStop()
        {
            foreach (var id in Descendants())
            {
                try { using (var p = Process.GetProcessById(id)) p.Kill(); } catch { }
            }
            try { if (Running) proc.Kill(); } catch { }
        }
    }
}
