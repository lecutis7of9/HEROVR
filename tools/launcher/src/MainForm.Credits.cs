// SM2 VR Launcher - the Credits sheet (SM2 VR by Agent Mulder, Spidy by Ilya Mezerowsky), plus the preview data used for screenshots (SM2VR_LAUNCHER_PREVIEW=1).
using System;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.Windows.Forms;

namespace Sm2VrLauncher
{
    static class Credits
    {
        public const string SpidyUrl = "https://github.com/IlyaMez/SpidyVR";
        public const string KofiUrl = "https://ko-fi.com/ilyamezerowsky";
        public const string SjrlUrl = "https://github.com/sjrl2000";
        public const string IplusUrl = "https://github.com/iplusdvr";
        public const string AdrianUrl = "https://github.com/AdrianGzzEsc";
        public const string BlazeUrl = "https://www.nexusmods.com/bladeandsorcery/mods/12564";
        public const string GplUrl = "https://www.gnu.org/licenses/gpl-3.0.html";
        // Agent Mulder, creator of the SM2 VR port and this launcher.
        public const string TikTokUrl = "https://www.tiktok.com/@agentmulder?_r=1&_t=ZP-9Aq9mnbk5aJ";
        public const string DiscordUrl = "https://discord.gg/W2b9YjRXzf";
    }

    partial class MainForm
    {
        RichLine sourceLine;

        void BuildCredits()
        {
            var p = pages[3];
            var card = Add(p, new GlassPanel("Credits"), 0, 0, 1108, 556);
            var area = Add(card, new ScrollArea(), 20, 50, 1068, 490);
            int y = 0;
            Func<string, int> head = t =>
            {
                var l = new TextLabel(GlassPanel.Spaced(t), Theme.UiBold(11), Theme.Red) { Wrap = false };
                l.Bounds = Theme.R(0, y + 6, 1040, 20); area.Controls.Add(l); y += 30; return 0;
            };
            Func<string, Font, Color, int> para = (t, ft, c) =>
            {
                var l = new TextLabel(t, ft, c); l.Bounds = Theme.R(0, y, 1040, 20); area.Controls.Add(l);
                l.FitHeight(); y += (int)Math.Ceiling(l.Height / Theme.Scale) + 4; return 0;
            };
            Func<RichLine> line = () =>
            {
                var r = new RichLine { Tip = tip }; r.Bounds = Theme.R(0, y, 1040, 24); area.Controls.Add(r); y += 26; return r;
            };
            var f = Theme.Ui(12.5f); var fb = Theme.UiBold(12.5f); var dot = "   \u00B7   ";

            head("Made by Agent Mulder");
            line().Add("Agent Mulder", Theme.UiBold(14), Theme.Text).Add("  - " + LauncherInfo.Title + " for Marvel's Spider-Man 2 and this launcher.", f, Theme.Silver);
            line().Link("TikTok @agentmulder", fb, Credits.TikTokUrl).Add(dot, f, Theme.Dim).Link("Discord server", fb, Credits.DiscordUrl);

            // GPLv3 section 5(d): the Appropriate Legal Notices stay one click away (NOTICE, CREDITS.md, LICENSE).
            head("Copyright notices");
            para(LauncherInfo.Title + " is free software under the GNU General Public License version 3, and it comes with NO WARRANTY. " +
                 "You may share and change it under that license. The copyright notices and credits are in NOTICE and CREDITS.md, and the license is in LICENSE.", f, Theme.Sub);
            line().Act("Open LICENSE", fb, () => OpenNotice("LICENSE")).Add(dot, f, Theme.Dim)
                  .Act("Open NOTICE", fb, () => OpenNotice("NOTICE")).Add(dot, f, Theme.Dim)
                  .Act("Open CREDITS", fb, () => OpenNotice("CREDITS.md"));
            sourceLine = line();
            RefreshCredits();

            // Version 0.0.7 (launcher build 11): the heroes' 3D models, credited as their licences ask (THIRD-PARTY-MODELS.md).
            head("3D models");
            para("Captain America's shield - \"Captain America Shield\" by Digital Goblin, CC BY 4.0 (remeshed, recoloured)\n" +
                 "Captain America suit (optional manual swap) - \"Captain America\" by iqbalmulyawan, CC BY 4.0 (re-oriented, rescaled)\n" +
                 "Daredevil's batons - \"Daredevil's baton\" by Yanez Designs (Anthony Yanez), CC BY 4.0 (rope removed, split in two)\n" +
                 "Punisher's pistols - \"9mm Pistol\" by TORI106, CC BY 4.0 (decimated, recoloured)\n" +
                 "Punisher's sawed-off - \"Sawed-off Shotgun\" by LiliumLetifer, CC BY 4.0 (split at the hinge, decimated)\n" +
                 "Batman's grapple gun - \"Batman's Grapple Gun\" by Falxx, CC BY 4.0 (decimated, recoloured)\n" +
                 "Batarang EIRA - \"Batarang\" by Eira, CC BY 4.0 (decimated, recoloured)\n" +
                 "Batarang CLOONEY - \"Batarang Clooney\" by Ludus101, CC BY 4.0 (decimated)\n" +
                 "Batarang JOKER - \"Batarang Joker\" by baton268, CC BY-NC-ND 4.0 (unmodified, non-commercial: HERO VR is free)\n" +
                 // Version 0.0.72: THOR HAMMER's four Mjolnirs and the PUNISHER VAN.
                 "Thor's hammer #1 - \"Marvel Thor's Hammer (Mjolnir)\" by Boooooop, CC BY 4.0 (decimated)\n" +
                 "Thor's hammer #5 - \"Mjolnir (Thor's hammer)\" by TheDevilsEye, CC BY 4.0\n" +
                 "Thor's hammer #6 - \"Mjolnir - Thor love and thunder\" by Jesus Osco, CC BY 4.0 (decimated)\n" +
                 "Thor's hammer #3 - \"Thor's Hammer: Mjolnir\" by fccooper, CC BY 4.0\n" +
                 "Punisher van - \"GMC Vandura1976 Van\" by snafuj, CC BY 4.0 (decimated, painted black; cab and wheel built by Agent Mulder)\n" +
                 // Version 0.0.72 (launcher build 13): the 23 worn suits and the ROSTER pictures.
                 "Worn suits (Models\\Suits) - by Light.k, H.E.C.U Models, Tigerar1, mikomagallona, Marco.41, YE YE, officialsupremefilms, Neut2000, CAPTAAINR, " +
                 "Shivam.Yadav, 9A Films / Nihar Arora, Nathang30, benjamin.borromeo, Mittergen, floh, Chechorams16, Stokissout and AssetGoat; CC BY 4.0, " +
                 "CC BY-NC 4.0 (floh), CC BY-NC-SA 4.0 (Nathang30) and the Sketchfab Standard licence (Tigerar1, Marco.41); non-commercial: HERO VR is free\n" +
                 // Version 0.0.73 (launcher build 14): WOLVERINE's claws.
                 "Wolverine's claws (Models\\Claws) - from \"Wolverine - Jacket - Jeans (Rigged and Textured)\" by Light.k, CC BY 4.0 (cut from the model)\n" +
                 "ROSTER pictures - thumbnails of the artists' models, each named in THIRD-PARTY-MODELS.md\n" +
                 "Links, licences and changes: THIRD-PARTY-MODELS.md in the mod folder.", f, Theme.Silver);
            line().Act("Open THIRD-PARTY-MODELS", fb, () => OpenNotice("THIRD-PARTY-MODELS.md"));

            head("Third-party software");
            para("OpenXR SDK - The Khronos Group Inc., Apache License 2.0\n" +
                 "JsonCpp - Baptiste Lepilleur and The JsonCpp Authors, public domain / MIT\n" +
                 "MinHook - Tsuda Kageyu (TsudaKageyu), BSD 2-Clause, with the Hacker Disassembler Engine (HDE) notices\n" +
                 "Dear ImGui - Omar Cornut, MIT (Spidy Launcher)\n" +
                 "Barlow, Comic Neue and Orbitron fonts - their project authors, SIL Open Font License 1.1\n" +
                 "bHaptics C/C++ SDK - bHaptics Inc., under the bHaptics SDK agreement (not GPL)\n" +
                 "Python 3.12.10 embeddable - Python Software Foundation, PSF License\n" +
                 "Full texts: THIRD_PARTY_NOTICES.md and docs\\licenses in the mod folder.", f, Theme.Silver);

            head("Copyright notices");
            para("This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 3. " +
                 "It is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. " +
                 "See the GNU General Public License for more details.", f, Theme.Sub);
            line().Link("GNU GPL version 3", f, Credits.GplUrl).Add(dot, f, Theme.Dim).Act("LICENSE in the mod folder", f, () => SystemInfo.OpenFolder(root, Path.Combine(root, "LICENSE")));
            para("Not affiliated with or endorsed by Insomniac Games, Sony Interactive Entertainment, Marvel or Valve. Spider-Man is a trademark of Marvel. You need your own copy of the game.", f, Theme.Dim);
            var end = new TextLabel("", f, Theme.Dim); end.Bounds = Theme.R(0, y + 10, 10, 10); area.Controls.Add(end);
        }

        // LICENSE and CREDITS.md are in the install's top folder, NOTICE beside the exe (tools\\launcher).
        void OpenNotice(string name)
        {
            foreach (var dir in new[] { AppDomain.CurrentDomain.BaseDirectory, root ?? "" })
            {
                try
                {
                    var p = Path.Combine(dir, name);
                    if (File.Exists(p)) { Process.Start("notepad.exe", "\"" + p + "\""); return; }
                }
                catch { }
            }
            SystemInfo.OpenUrl("https://www.gnu.org/licenses/gpl-3.0.html");
        }

        void RefreshCredits()
        {
            if (sourceLine == null) return;
            sourceLine.Pieces.Clear();
            var f = Theme.Ui(12.5f);
            sourceLine.Add("Source code: ", Theme.UiBold(12.5f), Theme.Text);
            if (settings.RepoConfigured) sourceLine.Link(settings.RepoUrl.Replace("https://", ""), f, settings.RepoUrl);
            else sourceLine.Add("github.com/" + settings.UpdateRepo + " (set the repository in Updates)", f, Theme.Sub);
            sourceLine.Add("   \u00B7   ", f, Theme.Dim).Link("upstream source", f, Credits.SpidyUrl);
            sourceLine.Invalidate();
        }

        // ---- Preview (screenshots on a machine without the game) ----------------------------------------------

        void FillPreview()
        {
            root = @"C:\sm2-port";
            gamePath = @"C:\Program Files (x86)\Steam\steamapps\common\Marvel's Spider-Man 2\Spider-Man2.exe";
            UpdateVersionUi();
            upInstalled.Set(CheckState.Ok, "Version 0.0.6 (build 39.0)  \u00B7  C:\\sm2-port");
            rowGame.Set(CheckState.Ok, gamePath + "  (Steam)");
            rowGameVer.Set(CheckState.Ok, "v2.810.0.0, the version this mod supports. Mod folder: C:\\sm2-port");
            rowVr.Set(CheckState.Ok, "Virtual Desktop streamer running  \u00B7  OpenXR runtime: SteamVR");
            rowBuild.Set(CheckState.Ok, "Version 0.0.6, built " + DateTime.Now.AddMinutes(-12).ToString("d MMM yyyy HH:mm") + ".");
            step1.Set(CheckState.Ok, "VR is running.");
            step2.Set(CheckState.Ok, "Built and up to date.");
            btnStop.Visible = false;
            SetState("READY", Theme.Good, "Everything checks out. Put the headset on after the game opens.");
            optionsSummary.Text = "Launch options: " + LaunchOptions.Summary(OptValue);
            sessionLog.Line(LauncherInfo.Title + " Launcher started.", Theme.Dim);
            sessionLog.Line("Found Spider-Man2.exe v2.810.0.0 in Steam library D:\\SteamLibrary.", Theme.Silver);
            sessionLog.Line("Virtual Desktop streamer running; OpenXR runtime: SteamVR.", Theme.Good);
            sessionLog.Line("VR mod Version 0.0.6 is built and up to date.", Theme.Good);
            sessionLog.Line("Press PLAY VR when the headset is connected.", Theme.Info);
            latest = new Manifest { Name = "SM2 VR", Version = "Version 0.0.6", VersionNumber = "0.0.6", Tag = "v0.0.6", ReleaseDate = "2026-10-25", MinGameVersion = "2.810.0.0" };
            upLatest.Set(CheckState.Ok, "Version 0.0.6, released 2026-10-25: you are up to date.");
            updatePill.Visible = false;
            upDownloads.Set(CheckState.Ok, "sm2-vr-0.0.6-slim.zip (Version 0.0.6, " + DateTime.Now.AddHours(-3).ToString("d MMM HH:mm") + "): already installed.");
            downloadZip = null;
            backupsNote.Text = "Keeps the last 3 versions in C:\\sm2-port-backup. Now: Version 0.0.4, 0.0.3 and 0.0.2. Your settings (live.txt, next-launch.txt, gestures) are kept across updates.";
            buildLog.Line("> Checking for updates (version.json from YOUR-GITHUB-NAME/sm2-vr)", Theme.Info);
            buildLog.Line("Version 0.0.6 is the latest release: up to date.", Theme.Good);
            buildLog.Line("> tools\\build.ps1 -Observer", Theme.Info);
            buildLog.Line("[ 98/112] Building CXX object CMakeFiles/spidy_stereo_probe.dir/src/game_xr.cpp.obj", Theme.Silver);
            buildLog.Line("[112/112] Linking CXX shared library spidy_stereo_probe.dll", Theme.Silver);
            buildLog.Line("Build finished.", Theme.Good);
            SetProgress("Build finished.", 100);
            scriptParams = null;
            string tab = Environment.GetEnvironmentVariable("SM2VR_LAUNCHER_TAB");
            int t; if (int.TryParse(tab, out t) && t >= 0 && t < pages.Length) tabs.Select(t);
            SetUpdateButtons();
            btnUpdateNow.Enabled = false;
            Invalidate(true);
            if (Environment.GetEnvironmentVariable("SM2VR_LAUNCHER_DIALOG") == "revert")
            {
                var dt = new Timer { Interval = 1500 };
                dt.Tick += (s, e) => { dt.Stop(); using (var d = new RevertDialog(root, settings, true)) d.ShowDialog(this); };
                dt.Start();
            }
            string dlg = Environment.GetEnvironmentVariable("SM2VR_LAUNCHER_DIALOG");
            if (dlg == "offer" || dlg == "fallback" || dlg == "switch")
            {
                var dt = new Timer { Interval = 1500 };
                dt.Tick += (s, e) => { dt.Stop(); PreviewDesignDialog(dlg); };
                dt.Start();
            }
            if (hero) latest = null;   // the preview's fake install is "up to date": the tile says CHECK FOR NEW VERSION
            HeroSync();
            if (dlg == "crash" || dlg == "bug")
            {
                var dt = new Timer { Interval = 1500 };
                dt.Tick += (s, e) =>
                {
                    dt.Stop();
                    if (dlg == "bug") { using (var d = new BugDialog(root)) d.ShowDialog(this); return; }
                    var c = new CrashFinding { Kind = "The game closed with a crash report from Windows.",
                        Body = Telemetry.Header(root, true) + "\nWhat was found: Windows error report AppCrash_Spider-Man2.exe (preview)\n\nGame fps: average 41.9, low 33.0\nHero: Iron Man" };
                    using (var d = new CrashDialog(c, root)) d.ShowDialog(this);
                };
                dt.Start();
            }
        }
    }
}
