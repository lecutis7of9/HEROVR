// HERO VR launcher - 0.0.74 test 3 safety checks: MOD CHECK (other mods' loaders in the game's folder) and the
// TWO-GPU CHECK's undo (Windows' per-app graphics preference, from the backup the VR session wrote).
using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Web.Script.Serialization;
using Microsoft.Win32;

namespace Sm2VrLauncher
{
    static class SafetyChecks
    {
        /// <summary>Windows' own library names that mods use as loaders (tools\mod_check.py LOADERS: keep the two the same).</summary>
        public static readonly string[] Loaders =
        {
            "dxgi.dll", "d3d12.dll", "d3d11.dll", "d3d10.dll", "d3d9.dll", "d3d8.dll", "ddraw.dll",
            "dinput8.dll", "dinput.dll", "version.dll", "winmm.dll", "winhttp.dll", "wininet.dll",
            "xinput1_3.dll", "xinput1_4.dll", "xinput9_1_0.dll", "dsound.dll", "opengl32.dll", "msacm32.dll",
            "msvfw32.dll"
        };

        /// <summary>The loader files beside the game's exe, as named there (empty when the folder cannot be read).</summary>
        public static List<string> ModLoaders(string gameExe)
        {
            var found = new List<string>();
            try
            {
                string dir = Path.GetDirectoryName(gameExe);
                if (string.IsNullOrEmpty(dir) || !Directory.Exists(dir)) return found;
                var names = Directory.GetFiles(dir).Select(Path.GetFileName).ToList();
                foreach (string loader in Loaders)
                {
                    string hit = names.FirstOrDefault(n => n.Equals(loader, StringComparison.OrdinalIgnoreCase));
                    if (hit != null) found.Add(hit);
                }
            }
            catch (Exception) { }
            return found;
        }

        const string GpuKey = @"Software\Microsoft\DirectX\UserGpuPreferences";

        /// <summary>Puts back Windows' graphics preferences the TWO-GPU CHECK changed (reports\gpu-preference-backup.json),
        /// where they are still what it set. Returns how many were put back; never throws.</summary>
        public static int RestoreGpuPreferences(string root)
        {
            int back = 0;
            try
            {
                string file = Path.Combine(root, "reports", "gpu-preference-backup.json");
                if (!File.Exists(file)) return 0;
                var backup = new JavaScriptSerializer().Deserialize<Dictionary<string, object>>(File.ReadAllText(file));
                if (backup == null) return 0;
                var left = new Dictionary<string, object>();
                using (var key = Registry.CurrentUser.CreateSubKey(GpuKey))
                {
                    foreach (var pair in backup)
                    {
                        var entry = pair.Value as IDictionary;
                        if (entry == null) continue;
                        string set = entry["set"] as string, old = entry["old"] as string;
                        try
                        {
                            if (key != null && (key.GetValue(pair.Key) as string) == set)
                            {
                                if (old == null) key.DeleteValue(pair.Key, false);
                                else key.SetValue(pair.Key, old, RegistryValueKind.String);
                                back++;
                            }
                        }
                        catch (Exception) { left[pair.Key] = pair.Value; }
                    }
                }
                if (left.Count == 0) File.Delete(file);
                else File.WriteAllText(file, new JavaScriptSerializer().Serialize(left));
            }
            catch (Exception) { }
            return back;
        }
    }
}
