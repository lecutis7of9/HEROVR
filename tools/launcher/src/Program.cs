// SM2 VR Launcher - entry point. One window per user; crashes are written to launcher-error.log.
using System;
using System.IO;
using System.Threading;
using System.Windows.Forms;

namespace Sm2VrLauncher
{
    static class Program
    {
        [STAThread]
        static int Main(string[] args)
        {
            Application.SetUnhandledExceptionMode(UnhandledExceptionMode.CatchException);
            Application.ThreadException += (s, e) => Crash(e.Exception, true);
            AppDomain.CurrentDomain.UnhandledException += (s, e) => Crash(e.ExceptionObject as Exception, false);
            bool first;
            using (var mutex = new Mutex(true, @"Local\SM2VR-Launcher-" + Environment.UserName, out first))
            {
                if (!first && !SystemInfo.Preview)
                {
                    MessageBox.Show("The " + LauncherInfo.Title + " launcher is already open.", LauncherInfo.Title, MessageBoxButtons.OK, MessageBoxIcon.Information);
                    return 1;
                }
                Application.EnableVisualStyles();
                Application.SetCompatibleTextRenderingDefault(false);
                var form = new MainForm();
                string shot = Environment.GetEnvironmentVariable("SM2VR_LAUNCHER_SCREENSHOT");
                if (!string.IsNullOrEmpty(shot)) Screenshot.Arm(form, shot);
                Application.Run(form);
                GC.KeepAlive(mutex);
            }
            return 0;
        }

        static void Crash(Exception ex, bool canContinue)
        {
            string text = DateTime.Now.ToString("o") + "\r\n" + (ex == null ? "unknown error" : ex.ToString()) + "\r\n\r\n";
            string log = Path.Combine(Settings.Folder, "launcher-error.log");
            try { Directory.CreateDirectory(Settings.Folder); File.AppendAllText(log, text); } catch { }
            try
            {
                MessageBox.Show("Something went wrong in the launcher:\n\n" + (ex == null ? "unknown error" : ex.Message) +
                    "\n\nDetails were saved to " + log + ". Your mod files were not changed by this error" +
                    (canContinue ? "; the launcher keeps running." : "."), LauncherInfo.Title, MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
            catch { }
        }
    }

    /// <summary>For the README preview: saves the window to a PNG after it has painted, then closes it.</summary>
    static class Screenshot
    {
        public static void Arm(Form f, string path)
        {
            var t = new System.Windows.Forms.Timer { Interval = 2500 };
            t.Tick += (s, e) =>
            {
                t.Stop();
                if (path == "wait") { var c = new System.Windows.Forms.Timer { Interval = 6000 }; c.Tick += (a, b) => f.Close(); c.Start(); return; }
                using (var bmp = new System.Drawing.Bitmap(f.ClientSize.Width, f.ClientSize.Height))
                {
                    f.DrawToBitmap(bmp, new System.Drawing.Rectangle(System.Drawing.Point.Empty, f.ClientSize));
                    bmp.Save(path, System.Drawing.Imaging.ImageFormat.Png);
                }
                f.Close();
            };
            f.Shown += (s, e) => t.Start();
        }
    }
}
