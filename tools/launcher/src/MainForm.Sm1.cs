// HERO VR 0.0.75 SM1 (0.0.76 layer, hotfix 0.0.75.2) - the launcher's two games, clearly apart: SPIDER-MAN 2 (HERO VR, unchanged) and SPIDER-MAN 1
// (iilyn's Spider-Man VR mod, with Mulder's Heroes add-on). His mod (Spidy by Ilya Mezerowsky, GPL-3.0,
// https://github.com/IlyaMez/SpidyVR) is downloaded from his own GitHub releases and never changed; his launcher keeps
// all of his graphics, performance and anti-aliasing settings. Our add-on is separate files. Made by Agent Mulder.
using System;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.IO;
using System.Threading;
using System.Windows.Forms;

namespace Sm2VrLauncher
{
    partial class MainForm
    {
        const int SheetSm1 = 6;
        GamePicker gamePicker;
        StatusRow sm1RowGame, sm1RowVersion, sm1RowMod, sm1RowAddon;
        TextLabel sm1StateText;
        AccentButton sm1BtnPlay, sm1BtnFlat, sm1BtnLauncher, sm1BtnUpdate;
        ToggleRow sm1Addon, sm1Wrist, sm1Flight, sm1News;
        Sm1AddonWatcher sm1Watcher;
        readonly Sm1VrSession sm1Session = new Sm1VrSession();
        string sm1Game, sm1GameWhere = "not searched", sm1GameVersion, sm1Mod;
        Sm1Release sm1Latest;
        volatile bool sm1Busy;
        bool sm1Checked;
        TextLabel sm1SuitsText;
        AccentButton sm1BtnSuitsInstall, sm1BtnSuitsRemove, sm1BtnSuitsDump;
        volatile bool sm1SuitsBusy;
        bool sm1SuitsChecked;

        /// <summary>SPIDER-MAN 1 is the game picked at the top (saved; SPIDER-MAN 2 / HERO VR is the default).</summary>
        bool Sm1Selected { get { return !Sm1.Paused && settings != null && settings.Get(Sm1.KeyGame, LobbyGame.Sm2) == LobbyGame.Sm1; } }

        /// <summary>The game tag the server list shows (sm1 / sm2).</summary>
        string MpGame { get { return Sm1Selected ? LobbyGame.Sm1 : LobbyGame.Sm2; } }

        // ---- The picker: two big cards at the top --------------------------------------------------------

        void BuildGamePicker()
        {
            if (Sm1.Paused) return;   // 0.0.76 SM1 PAUSED: no SPIDER-MAN 2 / SPIDER-MAN 1 picker; the launcher is Spider-Man 2 only
            gamePicker = new GamePicker { Game = Sm1Selected ? LobbyGame.Sm1 : LobbyGame.Sm2, Black = hero };
            Controls.Add(gamePicker);
            gamePicker.BringToFront();
            Sm1LayoutPicker();
            if (hero) gamePicker.Visible = false;   // shown once the intro has played (Sm1HeroTick)
            tip.SetToolTip(gamePicker, "SPIDER-MAN 2: HERO VR.  SPIDER-MAN 1: " + Sm1.Title + " (his mod by " + Sm1.Author + "). Switches at once, no restart.");
            gamePicker.Picked += (s, e) => Sm1Pick(gamePicker.Game);
        }

        void Sm1LayoutPicker()
        {
            if (gamePicker == null) return;
            if (hero)
            {
                // Between the HERO VR logo and the window buttons, above the status line and the tiles.
                gamePicker.DesignH = 74;
                gamePicker.Bounds = new Rectangle(content.X + (int)Math.Round(400 * heroK), content.Y + (int)Math.Round(14 * heroK),
                    (int)Math.Round(740 * heroK), (int)Math.Round(74 * heroK));
            }
            else
            {
                gamePicker.DesignH = 108;
                gamePicker.Bounds = Theme.R(36, 80, 392, 108);
            }
        }

        /// <summary>New design: the picker shows after the intro and hides while a sheet is open.</summary>
        void Sm1HeroTick()
        {
            if (gamePicker == null || scene == null) return;
            bool show = !SheetOpen && (heroBroken || scene.T >= HeroScene.IntroEnd);
            if (gamePicker.Visible != show) { gamePicker.Visible = show; if (show) gamePicker.BringToFront(); }
        }

        void Sm1Pick(string game)
        {
            try
            {
                bool sm1 = game == LobbyGame.Sm1;
                settings.Set(Sm1.KeyGame, sm1 ? LobbyGame.Sm1 : LobbyGame.Sm2); settings.Save();
                if (gamePicker != null && gamePicker.Game != game) { gamePicker.Game = game; gamePicker.Invalidate(); }
                sessionLog.Line(sm1 ? "Spider-Man 1 picked: " + Sm1.Title + ". PLAY VR now starts his mod." : "Spider-Man 2 picked: HERO VR. PLAY VR starts HERO VR.", Theme.Info);
                if (sm1) { Sm1Refresh(); tabs.Select(SheetSm1); }
                else if (tabs.Selected == SheetSm1) tabs.Select(hero ? -1 : SheetMultiplayer);
                try { MpRefresh(); } catch (Exception ex) { Program.LogError(ex); }
                UpdatePlayState();
            }
            catch (Exception ex) { Program.LogError(ex); }
        }

        /// <summary>Status opens the Spider-Man 1 page while Spider-Man 1 is picked (its checks are there).</summary>
        bool Sm1Redirect()
        {
            if (!Sm1Selected || tabs.Selected != SheetStatus) return false;
            tabs.Select(SheetSm1);
            return true;
        }

        // ---- The Spider-Man 1 page ---------------------------------------------------------------------

        void BuildSm1()
        {
            var p = pages[SheetSm1];
            var card = Add(p, new GlassPanel("Spider-Man 1"), 0, 0, 1108, 436);
            bool hs = Theme.HeroStyle;
            Add(card, new TextLabel(Sm1.Title, Theme.Display(19), hs ? Theme.Neon : Theme.Text) { Wrap = false }, hs ? 190 : 170, 14, hs ? 520 : 760, 28);
            Add(card, new TextLabel("His mod: Spidy by " + Sm1.Author + " (" + Sm1.License + ", " + Sm1.RepoUrl + "), downloaded from his GitHub releases and never changed; " +
                "his launcher keeps all of his graphics, performance and anti-aliasing settings. Mulder's Heroes add-on (heroes, wrist menu, flight) is separate files by Agent Mulder.", Theme.Ui(11.5f), Theme.Sub), 20, 46, 1068, 36);

            sm1RowGame = Add(card, new StatusRow("Game"), 20, 86, 620, 50);
            var browseGame = new AccentButton("Browse", ButtonKind.Secondary) { Glyph = "folder" };
            sm1RowGame.AddAction(browseGame);
            sm1RowVersion = Add(card, new StatusRow("Game version"), 20, 136, 620, 50);
            sm1RowMod = Add(card, new StatusRow("iilyn's mod"), 20, 186, 620, 50);
            var browseMod = new AccentButton("Mod folder", ButtonKind.Secondary) { Glyph = "folder" };
            sm1RowMod.AddAction(browseMod);
            sm1RowAddon = Add(card, new StatusRow("Mulder's Heroes add-on"), 20, 236, 620, 50);
            sm1StateText = Add(card, new TextLabel("", Theme.Ui(11.5f), Theme.Sub), 20, 288, 620, 32);

            sm1BtnPlay = Add(card, new AccentButton("Play Spider-Man 1", ButtonKind.Primary) { Glyph = "play", TextFont = Theme.Display(18) }, 20, 324, 270, 56);
            sm1BtnFlat = Add(card, new AccentButton("Flatscreen", ButtonKind.Secondary) { Glyph = "monitor" }, 300, 332, 150, 40);
            sm1BtnLauncher = Add(card, new AccentButton("Open his launcher", ButtonKind.Secondary) { Glyph = "folder" }, 460, 332, 180, 40);
            sm1BtnUpdate = Add(card, new AccentButton("Update his mod", ButtonKind.Ghost) { Glyph = "download", TextFont = Theme.UiBold(12) }, 20, 392, 150, 30);
            var gh = Add(card, new AccentButton("His GitHub", ButtonKind.Ghost) { TextFont = Theme.UiBold(12) }, 180, 392, 110, 30);
            var kofi = Add(card, new AccentButton("Ko-fi", ButtonKind.Ghost) { TextFont = Theme.UiBold(12) }, 300, 392, 80, 30);
            var disc = Add(card, new AccentButton("His Discord", ButtonKind.Ghost) { TextFont = Theme.UiBold(12) }, 390, 392, 110, 30);
            var log = Add(card, new AccentButton("Add-on log", ButtonKind.Ghost) { TextFont = Theme.UiBold(12) }, 510, 392, 130, 30);

            var addonHead = Add(card, new TextLabel(GlassPanel.Spaced("Mulder's Heroes add-on"), Theme.UiBold(11), hs ? Theme.Neon : Theme.Red) { Wrap = false }, 670, 90, 418, 18);
            sm1Addon = Add(card, new ToggleRow("Load Mulder's Heroes add-on", "Adds heroes, the wrist menu and flight on top of his mod, never changing it; takes effect the next time you press Play.",
                settings.GetBool(Sm1.KeyAddon, true)), 670, 112, 418, 58);
            sm1Wrist = Add(card, new ToggleRow("Wrist menu", "Touch your left wrist with your right hand and pull the right trigger to open or close the hero menu, as in SM2; takes effect the next time you press Play.",
                settings.GetBool(Sm1.KeyWrist, true)), 670, 170, 418, 58);
            sm1Flight = Add(card, new ToggleRow("Flight", "Iron Man, Superman, Thor and Green Lantern take off on a held grip and stay up after letting go; takes effect the next time you press Play.",
                settings.GetBool(Sm1.KeyFlight, true)), 670, 228, 418, 58);
            sm1News = Add(card, new ToggleRow("Tell me about his new releases", "Looks at his GitHub once when the launcher starts (nothing installs by itself); takes effect the next time the launcher starts.",
                settings.GetBool(Sm1.KeyNews, true)), 670, 286, 418, 58);
            // 0.0.76: which hero features Spider-Man 1 has (Sm1.Features, the same list as the add-on's own).
            // (0.0.76: shown as the add-on heading's tooltip and in the session log at Play, to make room for hero suits.)
            tip.SetToolTip(addonHead, Sm1.FeatureSummary() + "\n\n" + Sm1.FeatureDetails());

            // 0.0.76: hero suits - the launcher adds the heroes to Spider-Man 1 as suits through the game's own toc
            // (Overstrike-style, Sm1Suits.cs); only the game's asset_archive changes, never iilyn's files.
            var suitsHead = Add(card, new TextLabel(GlassPanel.Spaced("Hero suits in Spider-Man 1"), Theme.UiBold(11), hs ? Theme.Neon : Theme.Red) { Wrap = false }, 670, 348, 418, 18);
            sm1SuitsText = Add(card, new TextLabel("Hero suits: not checked yet. Each button takes effect the next time the game starts.", Theme.Ui(10f), Theme.Sub), 670, 366, 418, 30);
            sm1BtnSuitsInstall = Add(card, new AccentButton("Install suits", ButtonKind.Ghost) { Glyph = "build", TextFont = Theme.UiBold(12) }, 670, 398, 136, 30);
            sm1BtnSuitsRemove = Add(card, new AccentButton("Remove suits", ButtonKind.Ghost) { Glyph = "revert", TextFont = Theme.UiBold(12) }, 811, 398, 136, 30);
            sm1BtnSuitsDump = Add(card, new AccentButton("Game info", ButtonKind.Ghost) { Glyph = "download", TextFont = Theme.UiBold(12) }, 952, 398, 136, 30);
            tip.SetToolTip(suitsHead, "Install fits the ten heroes to Spider-Man 1's own suit and adds them to its Suits menu (pick them there; the wrist menu does not switch them yet); takes effect the next time the game starts. " +
                "Remove puts the game's files back exactly as they were; takes effect the next time the game starts. " +
                "Game info only reads the game and saves a report for Agent Mulder; it changes nothing. iilyn's mod files are never touched.");
            tip.SetToolTip(sm1BtnSuitsInstall, "Converts Models\\SM1Suits on this PC (a minute or two), backs up asset_archive\\toc, writes asset_archive\\HeroVR. Close the game first. Takes effect the next time the game starts.");
            tip.SetToolTip(sm1BtnSuitsRemove, "Puts back the toc saved before Install and deletes asset_archive\\HeroVR. Takes effect the next time the game starts.");
            tip.SetToolTip(sm1BtnSuitsDump, "Reads the game (nothing is written there) and saves herovr_sm1_dump.json in " + Sm1Suits.DumpFolder + ". Send that folder to Agent Mulder.");
            sm1BtnSuitsInstall.Click += (s, e) => Sm1SuitsRun("install");
            sm1BtnSuitsRemove.Click += (s, e) => Sm1SuitsRun("revert");
            sm1BtnSuitsDump.Click += (s, e) => Sm1SuitsRun("dump");

            sm1Addon.Changed += (s, e) => { settings.SetBool(Sm1.KeyAddon, sm1Addon.Checked); settings.Save(); sm1Wrist.Enabled = sm1Flight.Enabled = sm1Addon.Checked; Sm1Refresh(); UpdatePlayState(); };
            sm1Wrist.Changed += (s, e) => { settings.SetBool(Sm1.KeyWrist, sm1Wrist.Checked); settings.Save(); };
            sm1Flight.Changed += (s, e) => { settings.SetBool(Sm1.KeyFlight, sm1Flight.Checked); settings.Save(); };
            sm1News.Changed += (s, e) => { settings.SetBool(Sm1.KeyNews, sm1News.Checked); settings.Save(); };
            sm1Wrist.Enabled = sm1Flight.Enabled = sm1Addon.Checked;
            sm1Wrist.DisabledReason = sm1Flight.DisabledReason = "Needs Mulder's Heroes add-on (above); takes effect the next time you press Play.";

            browseGame.Click += (s, e) => Sm1BrowseGame();
            browseMod.Click += (s, e) => Sm1BrowseMod();
            sm1BtnPlay.Click += (s, e) => Sm1Play();
            sm1BtnFlat.Click += (s, e) => Sm1PlayFlat();
            sm1BtnLauncher.Click += (s, e) => Sm1OpenLauncher();
            sm1BtnUpdate.Click += (s, e) => Sm1UpdateMod();
            gh.Click += (s, e) => SystemInfo.OpenUrl(Sm1.ReleasesUrl);
            kofi.Click += (s, e) => SystemInfo.OpenUrl(Sm1.KofiUrl);
            disc.Click += (s, e) => SystemInfo.OpenUrl(Sm1.DiscordUrl);
            log.Click += (s, e) =>
            {
                try { Directory.CreateDirectory(Sm1.AddonDataFolder); SystemInfo.OpenFolder(Sm1.AddonDataFolder, null); } catch (Exception ex) { Program.LogError(ex); }
            };
            tip.SetToolTip(sm1BtnLauncher, "Opens Spidy Launcher.exe, his own launcher: change his settings there (PLAY VR here uses them), or press PLAY there instead.");
            tip.SetToolTip(gh, Sm1.ReleasesUrl);
            tip.SetToolTip(kofi, "Support iilyn: " + Sm1.KofiUrl);
            tip.SetToolTip(disc, "iilyn's Discord: " + Sm1.DiscordUrl);
            tip.SetToolTip(sm1BtnUpdate, "Downloads his latest release from his GitHub (checksum checked) and unpacks it unchanged.");
        }

        /// <summary>Reads the game, his mod and the add-on (disk only, quick) and fills the page's rows.</summary>
        void Sm1Refresh()
        {
            try
            {
                if (preview) { sm1Game = null; sm1GameWhere = "preview"; sm1Mod = null; sm1GameVersion = null; }
                else
                {
                    sm1Game = Sm1.GamePath(settings, out sm1GameWhere);
                    sm1GameVersion = Sm1.GameVersion(sm1Game);
                    sm1Mod = Sm1.ModFolder(settings);
                }
                if (sm1RowGame == null) return;
                if (sm1Game == null) sm1RowGame.Set(CheckState.Bad, "Not found: PLAY opens Steam's install page, or press Browse.");
                else sm1RowGame.Set(CheckState.Ok, sm1Game);
                if (sm1Game == null) sm1RowVersion.Set(CheckState.Info, "Needs the game first.");
                else if (sm1GameVersion == Sm1.SupportedVersion) sm1RowVersion.Set(CheckState.Ok, "v" + sm1GameVersion + ", the version iilyn's mod supports.");
                else sm1RowVersion.Set(CheckState.Warn, "v" + (sm1GameVersion ?? "?") + ": his mod is made for v" + Sm1.SupportedVersion + "; it may not start.");
                if (sm1Mod == null) sm1RowMod.Set(CheckState.Warn, "Not on this PC yet: PLAY gets it from his GitHub (about 17 MB).");
                else
                {
                    string v = Sm1.InstalledVersion(sm1Mod);
                    string news = sm1Latest != null && v.Length > 0 && Sm1VersionNewer(sm1Latest.Version, v) ? " (" + sm1Latest.Version + " is out)" : "";
                    sm1RowMod.Set(news.Length > 0 ? CheckState.Warn : CheckState.Ok, (v.Length > 0 ? "Spidy " + v + ", " : "Spidy, ") + sm1Mod + news);
                }
                bool want = settings.GetBool(Sm1.KeyAddon, true);
                if (!want) sm1RowAddon.Set(CheckState.Info, "Off: Spider-Man 1 starts with iilyn's mod alone.");
                else if (Sm1.AddonPresent(root) && Sm1.LayerPresent(root)) sm1RowAddon.Set(CheckState.Ok, "Ready: heroes, wrist menu and flight join his VR. " + Sm1LastAddonLine());
                else if (Sm1.AddonPresent(root)) sm1RowAddon.Set(CheckState.Warn, "Only the report part is here (no " + Sm1.LayerDll + "): no wrist menu, heroes or flight. Repair gets it.");
                else sm1RowAddon.Set(CheckState.Warn, "Not here yet: PLAY offers to get it; his mod also runs alone.");
                if (!sm1SuitsChecked && !preview && sm1Game != null) { sm1SuitsChecked = true; Sm1SuitsRun("status"); }
            }
            catch (Exception ex) { Program.LogError(ex); }
        }

        /// <summary>Runs the hero suits tool (status, install, revert, dump) in the background; one at a time.</summary>
        void Sm1SuitsRun(string command)
        {
            if (preview || sm1SuitsBusy || sm1BtnSuitsInstall == null) return;
            if (command != "status" && Sm1.GameRunning())
            {
                sessionLog.Line("Hero suits: close Spider-Man first; its files cannot change while it runs.", Theme.Warn);
                return;
            }
            string why = Sm1Suits.Missing(root, sm1Game);
            if (why != null) { sm1SuitsText.Text = "Hero suits: " + why + "."; if (command != "status") sessionLog.Line("Hero suits: " + why + ".", Theme.Warn); return; }
            sm1SuitsBusy = true;
            sm1BtnSuitsInstall.Enabled = sm1BtnSuitsRemove.Enabled = sm1BtnSuitsDump.Enabled = false;
            string verb = command == "install" ? "Installing hero suits (a minute or two)..." : command == "revert" ? "Removing hero suits..." :
                          command == "dump" ? "Reading the game for the report (about two minutes)..." : "Checking hero suits...";
            sm1SuitsText.Text = verb;
            if (command != "status") sessionLog.Line("Hero suits: " + verb, Theme.Info);
            string game = sm1Game, r = root;
            var t = new Thread(() =>
            {
                Sm1Suits.Result res;
                try { res = Sm1Suits.Run(r, game, command, line => { if (command != "status") Ui(() => sessionLog.Line("  " + line, Theme.Sub)); }); }
                catch (Exception ex) { res = new Sm1Suits.Result { Error = ex.Message }; }
                Sm1Suits.Result st = null;
                if (command != "status") { try { st = Sm1Suits.Run(r, game, "status", null); } catch { } }
                Ui(() =>
                {
                    sm1SuitsBusy = false;
                    sm1BtnSuitsInstall.Enabled = sm1BtnSuitsRemove.Enabled = sm1BtnSuitsDump.Enabled = true;
                    if (command == "status") { sm1SuitsText.Text = Sm1Suits.Describe(res); return; }
                    if (st != null) sm1SuitsText.Text = Sm1Suits.Describe(st);
                    if (!res.Ok) { sessionLog.Line("Hero suits: " + command + " failed: " + res.Error + (command == "install" ? " A part-way install is undone by the tool." : "") + " Log: " + Sm1Suits.LogPath, Theme.Bad); return; }
                    if (command == "install") sessionLog.Line("Hero suits installed. Start Spider-Man 1 and pick them in its Suits menu (looks not yet checked in the game).", Theme.Good);
                    else if (command == "revert") sessionLog.Line("Hero suits removed; the game's toc is back as it was.", Theme.Good);
                    else
                    {
                        sessionLog.Line("Game info saved in " + Sm1Suits.DumpFolder + ". Send that folder to Agent Mulder.", Theme.Good);
                        try { SystemInfo.OpenFolder(Sm1Suits.DumpFolder, null); } catch { }
                    }
                });
            });
            t.IsBackground = true;
            t.Start();
        }

        static string Sm1LastAddonLine()
        {
            string l = Sm1.AddonLogLast();
            return l.Length == 0 ? "" : "Last: " + (l.Length > 50 ? l.Substring(0, 50) + "..." : l);
        }

        static bool Sm1VersionNewer(string latest, string have)
        {
            Version a, b;
            return Version.TryParse((latest ?? "").Trim(), out a) && Version.TryParse((have ?? "").Trim(), out b) && a > b;
        }

        /// <summary>UpdatePlayState while Spider-Man 1 is picked: the status line and the play buttons.</summary>
        void Sm1State()
        {
            bool running = !preview && Sm1.GameRunning();
            bool idle = job == Job.None && !sm1Busy;
            btnPlay.Enabled = idle;
            btnFlat.Enabled = idle && !running;
            if (sm1BtnPlay != null) { sm1BtnPlay.Enabled = idle; sm1BtnFlat.Enabled = idle && !running; sm1BtnUpdate.Enabled = idle; sm1BtnLauncher.Enabled = idle && sm1Mod != null; }
            string title, sub; Color c;
            if (sm1Busy) { title = "DOWNLOADING..."; c = Theme.Silver; sub = "Getting iilyn's mod from his GitHub (Spider-Man 1)."; }
            else if (sm1Game == null) { title = "SPIDER-MAN 1 NOT FOUND"; c = Theme.Warn; sub = "PLAY VR offers Steam's install page for " + Sm1.GameName + "."; }
            else if (sm1Mod == null) { title = "IILYN'S MOD NEEDED"; c = Theme.Warn; sub = "PLAY VR downloads his mod from his GitHub first (a minute; nothing to do by hand)."; }
            else if (running) { title = "GAME RUNNING"; c = Theme.Warn; sub = "Spider-Man 1 is running; iilyn's launcher controls VR."; }
            else if (sm1GameVersion != null && sm1GameVersion != Sm1.SupportedVersion) { title = "READY"; c = Theme.Warn; sub = Sm1.VersionWarning(sm1GameVersion); }
            else { title = "READY"; c = Theme.Good; sub = "Spider-Man 1: PLAY VR starts iilyn's VR directly with his launcher's settings. Mulder's Heroes add-on loads once his VR runs."; }
            SetState(title, c, sub);
            if (sm1StateText != null) { sm1StateText.Text = sub; sm1StateText.ForeColor = c == Theme.Good ? Theme.Sub : c; }
            HeroSync();
        }

        string sm1Tile3Title, sm1Tile3Sub, sm1Tile3Icon;

        /// <summary>HeroSync's tiles while Spider-Man 1 is picked.</summary>
        void Sm1HeroTiles(HeroTile[] t, ref bool nowGame)
        {
            if (sm1Tile3Title == null) { sm1Tile3Title = t[3].Title; sm1Tile3Sub = t[3].Sub; sm1Tile3Icon = t[3].Icon; }
            if (!Sm1Selected) { t[3].Title = sm1Tile3Title; t[3].Sub = sm1Tile3Sub; t[3].Icon = sm1Tile3Icon; return; }
            bool running = !preview && Sm1.GameRunning();
            t[0].Title = "PLAY VR"; t[0].Icon = "helmet";
            bool vrOn = sm1Session.Running;
            t[0].Title = vrOn ? "STOP VR" : "PLAY VR";
            t[0].Sub = sm1Busy ? "DOWNLOADING IILYN'S MOD..." : vrOn ? "SPIDER-MAN 1 VR IS RUNNING" : "SPIDER-MAN 1 \u00B7 IILYN'S MOD";
            t[0].Enabled = vrOn || (job == Job.None && !sm1Busy);
            t[1].Enabled = job == Job.None && !sm1Busy && !running;
            t[1].Sub = running ? "GAME IS RUNNING" : "SPIDER-MAN 1 ON MONITOR";
            t[3].Title = "SPIDER-MAN 1"; t[3].Sub = "IILYN'S MOD \u00B7 ADD-ON";
            if (running) nowGame = true;
        }

        // ---- Playing -----------------------------------------------------------------------------------

        bool Sm1Ask(string text)
        {
            if (preview) return false;
            using (var d = new AutosetupDialog("Spider-Man 1 setup", text)) return d.ShowDialog(this) == DialogResult.Yes;
        }

        /// <summary>PLAY VR with Spider-Man 1 picked: anything missing gets a pop-up (No / Yes, Autosetup), the
        /// checks run (game version, a VR runtime, not as administrator, game not already running), then his
        /// run_game_vr.py starts directly with his own settings (Sm1Session) and the add-on is loaded once his VR
        /// runs. While it runs, the same tile is STOP VR (our stop event).</summary>
        void Sm1Play()
        {
            try
            {
                if (sm1Session.Running)
                {
                    if (sm1Session.Stop()) sessionLog.Line("Spider-Man 1: stopping iilyn's VR session...", Theme.Info);
                    return;
                }
                if (!GuardIdle() || sm1Busy) return;
                Sm1Refresh();
                if (sm1Game == null)
                {
                    if (Sm1Ask(Sm1.GameMissingText(sm1GameWhere)))
                    {
                        sessionLog.Line("Spider-Man 1: opening Steam's install page for " + Sm1.GameName + ". Press PLAY VR again when it is installed.", Theme.Info);
                        SystemInfo.OpenUrl("steam://install/" + Sm1.SteamAppId);
                    }
                    return;
                }
                if (sm1Mod == null)
                {
                    if (Sm1Ask(Sm1.ModMissingText())) Sm1InstallMod(true);
                    return;
                }
                if (!Sm1Checks()) return;
                string staged = null;
                if (!settings.GetBool(Sm1.KeyAddon, true) && Environment.OSVersion.Platform == PlatformID.Win32NT) Sm1.RemoveLayers();
                if (settings.GetBool(Sm1.KeyAddon, true))
                {
                    if (!Sm1.AddonPresent(root))
                    {
                        if (Sm1Ask(Sm1.AddonMissingText()) && SystemInfo.IsPortRoot(root))
                        {
                            sessionLog.Line("Spider-Man 1: getting the HERO VR mod files for Mulder's Heroes add-on...", Theme.Info);
                            RepairMod(ok => Ui(() =>
                            {
                                if (ok && Sm1.AddonPresent(root)) { Sm1Play(); return; }
                                sessionLog.Line("Spider-Man 1: this HERO VR release does not carry Mulder's Heroes add-on yet; starting iilyn's mod alone.", Theme.Warn);
                                Sm1Start(null);
                            }), true);
                            return;
                        }
                        sessionLog.Line("Spider-Man 1: starting iilyn's mod without Mulder's Heroes add-on.", Theme.Sub);
                    }
                    else
                    {
                        try
                        {
                            staged = Sm1.StageAddon(root);
                            Sm1.WriteAddonIni(true, settings.GetBool(Sm1.KeyWrist, true), settings.GetBool(Sm1.KeyFlight, true));
                        }
                        catch (Exception ex) { staged = null; sessionLog.Line("Spider-Man 1: Mulder's Heroes add-on could not be prepared (" + ex.Message + "); starting iilyn's mod alone.", Theme.Warn); }
                        Sm1RegisterLayer();
                    }
                }
                Sm1Start(staged);
            }
            catch (Exception ex) { Program.LogError(ex); sessionLog.Line("Spider-Man 1: " + ex.Message, Theme.Bad); }
        }

        /// <summary>Stages the add-on's OpenXR layer and registers it for this Windows user until Spider-Man 1 closes.
        /// Any failure only leaves the layer out (his mod runs as he made it).</summary>
        void Sm1RegisterLayer()
        {
            if (Environment.OSVersion.Platform != PlatformID.Win32NT) return;
            try
            {
                string manifest = Sm1.StageLayer(root);
                if (manifest == null)
                {
                    sessionLog.Line("Spider-Man 1: " + Sm1.LayerDll + " is not in the HERO VR mod files: no wrist menu, heroes or flight this time (Repair gets it).", Theme.Warn);
                    return;
                }
                Sm1.RegisterLayer(manifest);
                Sm1.RemoveLayersAfterGame(line => Ui(() => sessionLog.Line(line, Theme.Sub)));
                sessionLog.Line("Spider-Man 1: the heroes' OpenXR layer is on for this game (removed when it closes). In VR: right hand on the left wrist and pull the right trigger for the menu.", Theme.Good);
                foreach (var f in Sm1.Features)
                    sessionLog.Line("  " + f[0] + ": " + (f[1] == "stub" ? "NOT YET - " : "built, not yet tried in the game - ") + f[2], f[1] == "stub" ? Theme.Warn : Theme.Sub);
            }
            catch (Exception ex)
            {
                Sm1.RemoveLayers();
                sessionLog.Line("Spider-Man 1: the heroes' OpenXR layer could not be set up (" + ex.Message + "); his mod runs alone.", Theme.Warn);
            }
        }

        /// <summary>The checks before a direct start; each failure is said in the log and a pop-up.</summary>
        bool Sm1Checks()
        {
            if (preview) return true;
            if (Environment.OSVersion.Platform != PlatformID.Win32NT) { sessionLog.Line("Spider-Man 1: VR runs on Windows only.", Theme.Bad); return false; }
            if (Sm1.GameRunning())
            {
                sessionLog.Line("Spider-Man 1 is already running: close it first, then press PLAY VR (iilyn's VR starts the game itself).", Theme.Warn);
                return false;
            }
            string runtime;
            bool vrRuntime = Sm1Session.VrRuntimePresent(out runtime);
            var blockers = Sm1Session.Blockers(sm1Mod, sm1Game, sm1GameVersion, vrRuntime, Sm1Session.RunningAsAdministrator());
            if (blockers.Count == 0)
            {
                sessionLog.Line("Spider-Man 1: checks passed (game " + sm1GameVersion + ", VR runtime " + Path.GetFileName(runtime) + ", not administrator).", Theme.Sub);
                return true;
            }
            foreach (var b in blockers) sessionLog.Line("Spider-Man 1: " + b, Theme.Bad);
            MessageBox.Show(this, "Spider-Man 1 VR cannot start yet:\n\n" + string.Join("\n\n", blockers.ToArray()), LauncherInfo.Title, MessageBoxButtons.OK, MessageBoxIcon.Warning);
            return false;
        }

        /// <summary>Starts his run_game_vr.py with his launcher.ini's settings (read only); his lines go to the log
        /// and to %APPDATA%\HERO VR SM1\reports. False (said in the log) when it could not start.</summary>
        bool Sm1StartDirect()
        {
            if (preview) { sessionLog.Line("Preview: would start " + Sm1Session.Script + " directly.", Theme.Dim); return true; }
            try
            {
                string note;
                var options = Sm1Session.Load(out note);
                if (note != null) sessionLog.Line("Spider-Man 1: " + note + ".", Theme.Sub);
                sm1Session.Start(sm1Mod, options, false,
                    line => Ui(() => sessionLog.Line("SM1 VR: " + line, Theme.Sub)),
                    code => Ui(() =>
                    {
                        sessionLog.Line("Spider-Man 1: iilyn's VR session ended (exit " + code + "). Its report: " + sm1Session.Report, code == 0 ? Theme.Info : Theme.Warn);
                        Sm1Refresh();
                    }));
                sessionLog.Line("Spider-Man 1: iilyn's VR is starting with his launcher's settings (render scale " + options.RenderScale + "%, upscale " + options.Upscale + "%). Log: " + sm1Session.LogPath, Theme.Good);
                Sm1Refresh();
                return true;
            }
            catch (Exception ex)
            {
                Program.LogError(ex);
                sessionLog.Line("Spider-Man 1: iilyn's VR could not start directly (" + ex.Message + "). Use OPEN IILYN'S LAUNCHER and press PLAY there.", Theme.Bad);
                return false;
            }
        }

        void Sm1Start(string addon)
        {
            if (!Sm1StartDirect()) { Sm1.RemoveLayers(); return; }
            HeroGameStarting();
            if (addon == null) return;
            if (Environment.OSVersion.Platform != PlatformID.Win32NT) { sessionLog.Line("Spider-Man 1: Mulder's Heroes add-on loads on Windows only.", Theme.Sub); return; }
            if (sm1Watcher != null) sm1Watcher.Stop();
            sm1Watcher = new Sm1AddonWatcher(addon, (line, good) => Ui(() =>
            {
                sessionLog.Line("Spider-Man 1: " + line, good ? Theme.Good : Theme.Warn);
                Sm1Refresh();
            }));
            sm1Watcher.Start();
            sessionLog.Line("Spider-Man 1: Mulder's Heroes add-on waits for iilyn's VR to start, then loads.", Theme.Sub);
        }

        /// <summary>Opens Spidy Launcher.exe (his launcher, his settings). True when it is open.</summary>
        bool Sm1OpenLauncher()
        {
            try
            {
                if (sm1Mod == null) Sm1Refresh();
                if (sm1Mod == null) { MessageBox.Show(this, "iilyn's mod is not on this PC yet: press PLAY VR (Spider-Man 1) and Yes, Autosetup.", LauncherInfo.Title); return false; }
                if (Sm1.LauncherRunning()) { sessionLog.Line("Spider-Man 1: iilyn's launcher is already open; press PLAY there.", Theme.Info); return true; }
                var psi = new System.Diagnostics.ProcessStartInfo(Path.Combine(sm1Mod, Sm1.LauncherExe)) { WorkingDirectory = sm1Mod, UseShellExecute = true };
                if (preview) { sessionLog.Line("Preview: would open " + psi.FileName, Theme.Dim); return true; }
                System.Diagnostics.Process.Start(psi);
                sessionLog.Line("Spider-Man 1: iilyn's launcher is open. Press PLAY in his launcher; his graphics, performance and AA settings are all his.", Theme.Good);
                return true;
            }
            catch (Exception ex)
            {
                Program.LogError(ex);
                sessionLog.Line("Spider-Man 1: could not open iilyn's launcher: " + ex.Message, Theme.Bad);
                MessageBox.Show(this, "Could not open iilyn's launcher (" + ex.Message + ").\n\nOpen the Spider-Man 1 page and press Open his launcher, or start Spidy Launcher.exe from " + (sm1Mod ?? "his folder") + ".", LauncherInfo.Title);
                return false;
            }
        }

        void Sm1PlayFlat()
        {
            try
            {
                if (!GuardIdle() || sm1Busy) return;
                Sm1Refresh();
                if (sm1Game == null)
                {
                    if (Sm1Ask(Sm1.GameMissingText(sm1GameWhere))) SystemInfo.OpenUrl("steam://install/" + Sm1.SteamAppId);
                    return;
                }
                if (Sm1.GameRunning()) { sessionLog.Line("Spider-Man 1 is already running.", Theme.Info); return; }
                sessionLog.Line("Spider-Man 1: starting the game on the monitor (no VR) through Steam.", Theme.Info);
                SystemInfo.OpenUrl("steam://rungameid/" + Sm1.SteamAppId);
                HeroGameStarting();
            }
            catch (Exception ex) { Program.LogError(ex); sessionLog.Line("Spider-Man 1: " + ex.Message, Theme.Bad); }
        }

        void Sm1UpdateMod()
        {
            if (!GuardIdle() || sm1Busy) return;
            Sm1InstallMod(false);
        }

        /// <summary>Downloads his latest release (his GitHub, checksum checked) and unpacks it unchanged, off the UI thread.</summary>
        void Sm1InstallMod(bool thenPlay)
        {
            if (sm1Busy || preview) return;
            if (Sm1.LauncherRunning() || Sm1.GameRunning())
            {
                MessageBox.Show(this, "Close iilyn's launcher and Spider-Man first: his files cannot be replaced while they run.", LauncherInfo.Title, MessageBoxButtons.OK, MessageBoxIcon.Information);
                return;
            }
            sm1Busy = true;
            UpdatePlayState();
            var t = new Thread(() =>
            {
                string error = null, dir = null;
                Sm1Release r = null;
                try
                {
                    r = Sm1.LatestRelease();
                    dir = Sm1.DownloadAndInstall(r, line => Ui(() => sessionLog.Line("Spider-Man 1: " + line, Theme.Sub)),
                        pct => Ui(() => { if (sm1StateText != null) sm1StateText.Text = "Downloading iilyn's mod from his GitHub: " + (pct < 0 ? "..." : pct + "%"); }));
                }
                catch (Exception ex) { error = ex.Message; }
                Ui(() =>
                {
                    sm1Busy = false;
                    if (error != null)
                    {
                        sessionLog.Line("Spider-Man 1: iilyn's mod could not be installed: " + error, Theme.Bad);
                        MessageBox.Show(this, "iilyn's mod could not be installed: " + error + "\n\nNothing was changed. Try again, or get it by hand from " + Sm1.ReleasesUrl + " and pick its folder (Spider-Man 1 page > Mod folder).",
                            LauncherInfo.Title, MessageBoxButtons.OK, MessageBoxIcon.Warning);
                    }
                    else
                    {
                        sm1Latest = r;
                        settings.Set(Sm1.KeyModFolder, ""); settings.Save();   // the installed copy is the one used from now on
                        sessionLog.Line("Spider-Man 1: iilyn's mod " + r.Version + " is installed, unchanged, in " + dir + ".", Theme.Good);
                    }
                    Sm1Refresh();
                    UpdatePlayState();
                    if (error == null && thenPlay) Sm1Play();
                });
            }) { IsBackground = true, Name = "SM1 download" };
            t.Start();
        }

        void Sm1BrowseGame()
        {
            using (var d = new OpenFileDialog { Title = "Pick Spider-Man.exe (" + Sm1.GameName + ")", Filter = "Spider-Man 1|Spider-Man.exe|Programs|*.exe", FileName = Sm1.GameExe })
            {
                try { if (sm1Game != null) d.InitialDirectory = Path.GetDirectoryName(sm1Game); } catch { }
                if (d.ShowDialog(this) != DialogResult.OK) return;
                if (!Sm1.IsGameExe(d.FileName)) { MessageBox.Show(this, "That is not Spider-Man.exe. iilyn's mod runs on " + Sm1.GameName + " only.", LauncherInfo.Title); return; }
                settings.Set(Sm1.KeyGamePath, d.FileName); settings.Save();
                Sm1Refresh(); UpdatePlayState();
            }
        }

        void Sm1BrowseMod()
        {
            using (var d = new FolderBrowserDialog { Description = "Pick iilyn's mod folder (the one with Spidy Launcher.exe and tools\\run_game_vr.py)" })
            {
                if (sm1Mod != null) d.SelectedPath = sm1Mod;
                if (d.ShowDialog(this) != DialogResult.OK) return;
                if (SystemInfo.IsPortRoot(d.SelectedPath)) { MessageBox.Show(this, "That is HERO VR's folder (Spider-Man 2). Pick iilyn's own Spidy folder, or press PLAY VR and Yes, Autosetup to get it.", LauncherInfo.Title); return; }
                if (!Sm1.IsModFolder(d.SelectedPath)) { MessageBox.Show(this, d.SelectedPath + " has no Spidy Launcher.exe and tools\\run_game_vr.py. Pick the unpacked Spidy-x.y.z folder.", LauncherInfo.Title); return; }
                settings.Set(Sm1.KeyModFolder, d.SelectedPath); settings.Save();
                Sm1Refresh(); UpdatePlayState();
            }
        }

        // ---- Start and close ---------------------------------------------------------------------------

        /// <summary>At start with Spider-Man 1 picked: one pop-up for what is missing (unless the player said No before),
        /// and his newest release looked up (no download) when release news is on.</summary>
        void Sm1Startup()
        {
            if (preview || sm1Checked) return;
            sm1Checked = true;
            if (!Sm1.Paused && settings.GetBool(Sm1.KeyNews, true))   // 0.0.76 SM1 PAUSED: no Spider-Man 1 news while hidden
            {
                var t = new Thread(() =>
                {
                    try
                    {
                        var r = Sm1.LatestRelease();
                        Ui(() =>
                        {
                            sm1Latest = r;
                            string have = sm1Mod == null ? "" : Sm1.InstalledVersion(sm1Mod);
                            if (have.Length > 0 && Sm1VersionNewer(r.Version, have)) sessionLog.Line("Spider-Man 1: iilyn's mod " + r.Version + " is out (you have " + have + "). Spider-Man 1 page > Update his mod.", Theme.Info);
                            Sm1Refresh();
                        });
                    }
                    catch { }
                }) { IsBackground = true, Name = "SM1 release news" };
                t.Start();
            }
            // A layer entry left by a launcher that closed while Spider-Man 1 ran: removed now the game is not running.
            if (Environment.OSVersion.Platform == PlatformID.Win32NT && !Sm1.GameRunning() && Sm1.RemoveLayers() > 0)
                sessionLog.Line("Spider-Man 1: removed a leftover entry of the heroes' OpenXR layer.", Theme.Sub);
            if (!Sm1Selected || settings.GetBool(Sm1.KeyDeclined, false)) return;
            Sm1Refresh();
            string text = sm1Game == null ? Sm1.GameMissingText(sm1GameWhere) : sm1Mod == null ? Sm1.ModMissingText() : null;
            if (text == null) return;
            if (!Sm1Ask(text))
            {
                settings.SetBool(Sm1.KeyDeclined, true); settings.Save();
                sessionLog.Line("Spider-Man 1 setup skipped. PLAY VR (Spider-Man 1) offers it again.", Theme.Sub);
                return;
            }
            if (sm1Game == null) SystemInfo.OpenUrl("steam://install/" + Sm1.SteamAppId);
            else Sm1InstallMod(false);
        }

        void Sm1Closing()
        {
            try { if (sm1Watcher != null) sm1Watcher.Stop(); } catch { }
            // The game not running: no one needs the layer entry. Running: the next launcher start removes it.
            try { if (Environment.OSVersion.Platform == PlatformID.Win32NT && !Sm1.GameRunning()) Sm1.RemoveLayers(); } catch { }
        }
    }

    /// <summary>The two games, side by side and big: SPIDER-MAN 2 (HERO VR) and SPIDER-MAN 1 (iilyn's mod with
    /// Mulder's Heroes add-on). A press picks one (Picked); the line under them says it switches at once.</summary>
    class GamePicker : Skin
    {
        public string Game = LobbyGame.Sm2;
        public bool Black;            // the new design: plain black behind (its orb is no backdrop)
        public float DesignH = 108;   // Height at scale 1, for the text sizes
        public event EventHandler Picked;
        int hover = -1;
        public const string Hint = "Pick a game: switches at once, no restart needed.";
        static readonly string[] Games = { LobbyGame.Sm2, LobbyGame.Sm1 };
        static readonly string[] Titles = { "SPIDER-MAN 2", "SPIDER-MAN 1" };
        static readonly string[] Subs = { "HERO VR", Sm1.Title };

        public GamePicker() { Cursor = Cursors.Hand; TabStop = true; }

        float U { get { return Height / Math.Max(1f, DesignH * Theme.Scale); } }

        RectangleF Card(int i)
        {
            float hintH = Height * 0.18f, gap = Width * 0.025f, w = (Width - gap) / 2f;
            return new RectangleF(i * (w + gap) + 1, 1, w - 2, Height - hintH - 3);
        }

        int Hit(Point p) { for (int i = 0; i < 2; i++) if (Card(i).Contains(p)) return i; return -1; }

        protected override void OnPaintBackground(PaintEventArgs e)
        {
            if (Black) { using (var b = new SolidBrush(Color.Black)) e.Graphics.FillRectangle(b, ClientRectangle); return; }
            base.OnPaintBackground(e);
        }

        protected override void Render(Graphics g)
        {
            Color accent = Theme.HeroStyle ? Theme.Neon : Theme.Red;
            float u = U;
            for (int i = 0; i < 2; i++)
            {
                var r = Card(i);
                bool sel = Games[i] == Game, hot = hover == i;
                using (var path = Theme.HeroStyle ? Theme.Slanted(r, r.Height * 0.18f) : Theme.Round(r, Theme.S(10)))
                {
                    using (var b = new SolidBrush(sel ? Color.FromArgb(70, accent) : Color.FromArgb(hot ? 34 : 20, 255, 255, 255))) g.FillPath(b, path);
                    using (var pen = new Pen(sel ? accent : Color.FromArgb(hot ? 160 : 80, 220, 225, 230), sel ? Math.Max(2f, 2f * u * Theme.Scale) : 1f)) g.DrawPath(pen, path);
                }
                float pad = r.Height * (Theme.HeroStyle ? 0.22f : 0.12f);
                var tf = Theme.F("display", (Theme.HeroStyle ? 22f : 19f) * u, FontStyle.Bold);
                var sf = Theme.F("ui", (Theme.HeroStyle ? 11f : 10.5f) * u, FontStyle.Regular);
                using (var b = new SolidBrush(sel ? Color.White : Theme.Silver)) g.DrawString(Titles[i], tf, b, r.X + pad, r.Y + r.Height * 0.08f);
                float th = g.MeasureString("S", tf).Height;
                using (var b = new SolidBrush(sel ? (Theme.HeroStyle ? accent : Color.FromArgb(235, 255, 255, 255)) : Theme.Sub))
                using (var fmt = new StringFormat { Trimming = StringTrimming.EllipsisWord })
                    g.DrawString(Subs[i], sf, b, new RectangleF(r.X + pad, r.Y + r.Height * 0.08f + th * 0.95f, r.Width - pad * 2 - r.Height * 0.25f, r.Bottom - (r.Y + r.Height * 0.08f + th * 0.95f) - 2), fmt);
                if (sel)
                {
                    float d = r.Height * 0.24f, cx = r.Right - pad * 0.6f - d, cy = r.Y + r.Height * 0.12f;
                    using (var b = new SolidBrush(accent)) g.FillEllipse(b, cx, cy, d, d);
                    using (var pen = new Pen(Color.Black, Math.Max(1.5f, d * 0.14f)) { StartCap = LineCap.Round, EndCap = LineCap.Round })
                        g.DrawLines(pen, new[] { new PointF(cx + d * .28f, cy + d * .52f), new PointF(cx + d * .45f, cy + d * .68f), new PointF(cx + d * .74f, cy + d * .34f) });
                }
            }
            using (var b = new SolidBrush(Theme.Sub))
                g.DrawString(Hint, Theme.F("ui", (Theme.HeroStyle ? 12f : 10f) * u, FontStyle.Regular), b, 2, Height * 0.83f);
        }

        void Pick(int i)
        {
            if (i < 0 || i > 1) return;
            Game = Games[i];
            Invalidate();
            if (Picked != null) Picked(this, EventArgs.Empty);
        }

        protected override void OnMouseMove(MouseEventArgs e) { base.OnMouseMove(e); int h = Hit(e.Location); if (h != hover) { hover = h; Invalidate(); } }
        protected override void OnMouseLeave(EventArgs e) { base.OnMouseLeave(e); if (hover != -1) { hover = -1; Invalidate(); } }
        protected override void OnMouseUp(MouseEventArgs e) { base.OnMouseUp(e); if (e.Button == MouseButtons.Left) Pick(Hit(e.Location)); }
        protected override bool IsInputKey(Keys k) { return k == Keys.Left || k == Keys.Right || base.IsInputKey(k); }
        protected override void OnKeyDown(KeyEventArgs e)
        {
            base.OnKeyDown(e);
            if (e.KeyCode == Keys.Left) Pick(0);
            else if (e.KeyCode == Keys.Right) Pick(1);
        }
    }
}
