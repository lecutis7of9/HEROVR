// SM2 VR Launcher - runs the mod's PowerShell scripts hidden, streams their output, and stops them gently.
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Management;
using System.Runtime.InteropServices;
using System.Text;
using System.Text.RegularExpressions;
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
                "-NoProfile -NonInteractive -ExecutionPolicy Bypass -OutputFormat Text -EncodedCommand " + Convert.ToBase64String(Encoding.Unicode.GetBytes(cmd)));
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
            // 0.0.62.2: this launcher shows a session's start failure itself (launch-game-vr.ps1 shows it for older ones).
            psi.EnvironmentVariables["SM2VR_LAUNCHER_BUILD"] = LauncherInfo.Build.ToString();
            // 0.0.70.1 hotfix: PowerShell (and the Python session it starts) gets this launcher's standard input as is;
            // a dead one (left by an earlier Stop VR, here or in the launcher that updated us) was "[WinError 6] The
            // handle is invalid" at "finding the headset". A dead handle is cleared first, exactly as a launcher
            // started from Explorer has none.
            StdHandles.ClearDead();
            try
            {
                proc = new Process { StartInfo = psi, EnableRaisingEvents = true };
                // -OutputFormat Text keeps the streams plain; any CLIXML that still arrives ("#< CLIXML <Objs ...>",
                // Windows PowerShell's serialized Write-Host and error records) is turned back into its text lines.
                var outText = new Clixml(); var errText = new Clixml();
                proc.OutputDataReceived += (s, e) => { if (e.Data != null && Output != null) foreach (var l in outText.Feed(e.Data)) Output(l, false); };
                proc.ErrorDataReceived += (s, e) => { if (e.Data != null && Output != null) foreach (var l in errText.Feed(e.Data)) Output(l, true); };
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
                // 0.0.70.1 hotfix: AttachConsole sets this launcher's standard handles to that console's and FreeConsole
                // closes them, leaving the handles dead for every later PLAY VR (WinError 6). They are put back after.
                var saved = StdHandles.Save();
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
                    StdHandles.Restore(saved);
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

    /// <summary>0.0.70.1 hotfix: this process's standard handles (stdin, stdout, stderr), which every program it starts
    /// with UseShellExecute=false gets for the streams it does not redirect. Windows only; elsewhere nothing.</summary>
    static class StdHandles
    {
        const int StdInput = -10, StdOutput = -11, StdError = -12;
        static readonly int[] All = { StdInput, StdOutput, StdError };
        [DllImport("kernel32.dll", SetLastError = true)] static extern IntPtr GetStdHandle(int which);
        [DllImport("kernel32.dll", SetLastError = true)] static extern bool SetStdHandle(int which, IntPtr handle);
        [DllImport("kernel32.dll")] static extern IntPtr GetCurrentProcess();
        [DllImport("kernel32.dll", SetLastError = true)] static extern bool DuplicateHandle(IntPtr srcProcess, IntPtr src, IntPtr dstProcess, out IntPtr dst, uint access, bool inherit, uint options);
        [DllImport("kernel32.dll", SetLastError = true)] static extern bool CloseHandle(IntPtr handle);

        static bool OnWindows { get { return Environment.OSVersion.Platform == PlatformID.Win32NT && Type.GetType("Mono.Runtime") == null; } }

        /// <summary>True when a child could be given <paramref name="h"/>: no handle (the child gets none, as from Explorer)
        /// or one that duplicates, as Python's subprocess and CreateProcess do with it.</summary>
        public static bool Usable(IntPtr h)
        {
            if (h == IntPtr.Zero) return true;
            if (h == new IntPtr(-1)) return false;
            IntPtr me = GetCurrentProcess(), copy;
            if (!DuplicateHandle(me, h, me, out copy, 0, false, 2 /* DUPLICATE_SAME_ACCESS */)) return false;
            CloseHandle(copy);
            return true;
        }

        public static IntPtr[] Save()
        {
            if (!OnWindows) return null;
            try { var r = new IntPtr[All.Length]; for (int i = 0; i < All.Length; i++) r[i] = GetStdHandle(All[i]); return r; }
            catch { return null; }
        }

        public static void Restore(IntPtr[] saved)
        {
            if (saved == null) return;
            try { for (int i = 0; i < All.Length; i++) SetStdHandle(All[i], saved[i]); ClearDead(); } catch { }
        }

        /// <summary>Clears (sets to none) any standard handle that is set but no longer valid. The number cleared.</summary>
        public static int ClearDead()
        {
            if (!OnWindows) return 0;
            int n = 0;
            try { foreach (int which in All) if (!Usable(GetStdHandle(which)) && SetStdHandle(which, IntPtr.Zero)) n++; }
            catch { }
            return n;
        }
    }

    /// <summary>Turns PowerShell's CLIXML ("#< CLIXML" then &lt;Objs&gt;...&lt;/Objs&gt;) back into the lines it carries;
    /// any other line passes unchanged. One instance per stream.</summary>
    sealed class Clixml
    {
        StringBuilder buf;
        static readonly char[] Ws = { ' ', '\t', '\r', '\n' };
        static readonly Regex Escape = new Regex("_x([0-9A-Fa-f]{4})_");

        public List<string> Feed(string line)
        {
            var result = new List<string>();
            string t = line.TrimStart(Ws);
            if (buf == null)
            {
                if (t.StartsWith("#< CLIXML"))
                {
                    t = t.Substring(9).TrimStart(Ws);
                    if (t.Length == 0) return result;
                }
                if (!t.StartsWith("<Objs")) { result.Add(line); return result; }
                buf = new StringBuilder();
            }
            buf.Append(t).Append('\n');
            if (t.IndexOf("</Objs>", StringComparison.Ordinal) < 0)
            {
                if (buf.Length > 4 * 1024 * 1024) { result.AddRange(Decode(buf.ToString())); buf = null; }
                return result;
            }
            string xml = buf.ToString();
            buf = null;
            result.AddRange(Decode(xml));
            return result;
        }

        static readonly Regex Progress = new Regex("<Obj S=\"progress\".*?</Obj>", RegexOptions.Singleline);
        static readonly Regex Texts = new Regex("<S S=\"[^\"]*\">(.*?)</S>|<ToString>(.*?)</ToString>", RegexOptions.Singleline);

        /// <summary>The text of each record (stream strings and the ToString of Write-Host/information records);
        /// progress records are dropped. Regex, not an XML parser: no extra reference assembly.</summary>
        public static List<string> Decode(string xml)
        {
            var lines = new List<string>();
            string body = Progress.Replace(xml, "");
            foreach (Match m in Texts.Matches(body))
            {
                string v = m.Groups[1].Success ? m.Groups[1].Value : m.Groups[2].Value;
                v = Unescape(System.Net.WebUtility.HtmlDecode(v));
                foreach (var l in v.Replace("\r\n", "\n").Split(new[] { '\n' }))
                    if (l.Trim().Length > 0) lines.Add(l.TrimEnd(Ws));
            }
            if (lines.Count == 0 && body.IndexOf("<Obj", StringComparison.Ordinal) >= 0) return Plain(body);
            return lines;
        }

        static string Unescape(string s)
        {
            return Escape.Replace(s, m => ((char)Convert.ToInt32(m.Groups[1].Value, 16)).ToString());
        }

        /// <summary>Unreadable XML: the text between its tags, one line each.</summary>
        static List<string> Plain(string xml)
        {
            var lines = new List<string>();
            foreach (Match m in Regex.Matches(xml, ">([^<]+)<"))
            {
                string v = Unescape(System.Net.WebUtility.HtmlDecode(m.Groups[1].Value));
                foreach (var l in v.Replace("\r\n", "\n").Split(new[] { '\n' }))
                    if (l.Trim().Length > 0) lines.Add(l.TrimEnd(Ws));
            }
            return lines;
        }
    }
}
