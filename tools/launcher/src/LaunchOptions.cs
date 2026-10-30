// SM2 VR Launcher - the launch options shown as toggles/sliders, and the launch-game-vr.ps1 arguments they make.
using System;
using System.Collections.Generic;
using System.Globalization;
using System.Linq;

namespace Sm2VrLauncher
{
    enum OptKind { Switch, Invert, Int, Choice, Webs }

    class Opt
    {
        public string Param;        // the launch-game-vr.ps1 parameter (or the switch for Invert)
        public OptKind Kind;
        public string Section, Title, Sub;
        public string Default;      // the launcher's default ("1"/"0" for switches)
        public int Min, Max, Step;
        public string Suffix;
        public string[] Choices, Labels;
        public bool AlwaysPass;     // pass even when equal to the script's default (as in the user's own command line)
        public string Exclusive;    // a switch turned off when this one turns on

        public static Opt Sw(string section, string p, string title, string sub, bool def)
        { return new Opt { Section = section, Param = p, Kind = OptKind.Switch, Title = title, Sub = sub, Default = def ? "1" : "0" }; }
        public static Opt Inv(string section, string p, string title, string sub)
        { return new Opt { Section = section, Param = p, Kind = OptKind.Invert, Title = title, Sub = sub, Default = "1" }; }
        public static Opt Num(string section, string p, string title, string sub, int min, int max, int step, int def, string suffix)
        { return new Opt { Section = section, Param = p, Kind = OptKind.Int, Title = title, Sub = sub, Min = min, Max = max, Step = step, Default = def.ToString(CultureInfo.InvariantCulture), Suffix = suffix }; }
        public static Opt Pick(string section, string p, string title, string sub, string[] choices, string def)
        { return new Opt { Section = section, Param = p, Kind = OptKind.Choice, Title = title, Sub = sub, Choices = choices, Default = def }; }
    }

    static class LaunchOptions
    {
        public static readonly List<Opt> All = new List<Opt>
        {
            // Picture
            new Opt { Section = "PICTURE", Param = "RenderScale", Kind = OptKind.Int, Title = "RENDER SCALE", Min = 50, Max = 200, Step = 5, Default = "100", Suffix = "%", AlwaysPass = true,
                      Sub = "Eye resolution vs. the headset's recommendation. Lower is faster; above 100 costs frame rate." },
            Opt.Num("PICTURE", "Supersample", "SUPERSAMPLE", "Render the eyes larger and filter down (replaces FXAA). Costs like render scale.", 100, 200, 5, 100, "%"),
            Opt.Pick("PICTURE", "EyeAA", "EYE ANTI-ALIASING", "Smooths each eye image on its way to the headset.", new[] { "off", "fxaa" }, "fxaa"),
            Opt.Pick("PICTURE", "MipBias", "TEXTURE SHARPNESS (MIP BIAS)", "0.5 is calmer (less roof flicker), 0 is sharper, 'game' keeps the game's.", new[] { "game", "0", "0.25", "0.5", "1" }, "0.5"),
            Opt.Pick("PICTURE", "Anisotropy", "TEXTURE FILTERING", "Anisotropic filtering for textures at an angle. 0 keeps the game's.", new[] { "0", "2", "4", "8", "16" }, "16"),
            Opt.Sw("PICTURE", "EngineTaa", "ENGINE TAA", "Runs the game's own TAA in each eye, as SpidyVR does (restart to change).", true),
            Opt.Sw("PICTURE", "KeepGameAa", "KEEP MY GAME AA", "Off: VR sets the game's AA to TAA with no DLSS, put back after; on keeps yours (restart).", false),
            Opt.Num("PICTURE", "BuildingReach", "BUILDING DETAIL DISTANCE", "How far buildings keep their full models, % of the game's distance.", 50, 400, 10, 150, "%"),
            Opt.Sw("PICTURE", "KeepGameSettings", "KEEP MY GAME PICTURE SETTINGS", "Keep your depth of field, motion blur, grain and vignette while VR runs.", false),
            // Desktop
            new Opt { Section = "DESKTOP WINDOW", Param = "SmallerDesktopView", Kind = OptKind.Switch, Title = "SMALLER DESKTOP WINDOW", Default = "1", Exclusive = "FullDesktopView",
                      Sub = "1280 x 720 game window on the monitor while VR runs: about 3-5% less GPU work." },
            new Opt { Section = "DESKTOP WINDOW", Param = "FullDesktopView", Kind = OptKind.Switch, Title = "FULL DESKTOP WINDOW", Default = "0", Exclusive = "SmallerDesktopView",
                      Sub = "Keep the game's own display settings on the monitor (costs GPU time)." },
            Opt.Num("DESKTOP WINDOW", "MonitorRate", "MONITOR MIRROR RATE", "How often the monitor copy updates. 0 = every frame (slower in the headset).", 0, 240, 5, 30, " fps"),
            new Opt { Section = "DESKTOP WINDOW", Param = "GameMonitorView", Kind = OptKind.Switch, Title = "GAME'S VIEW ON MONITOR", Default = "0", Exclusive = "StockMonitorView",
                      Sub = "Monitor shows the game's own view of your head (about a fifth fewer frames)." },
            new Opt { Section = "DESKTOP WINDOW", Param = "StockMonitorView", Kind = OptKind.Switch, Title = "STOCK CAMERA ON MONITOR", Default = "0", Exclusive = "GameMonitorView",
                      Sub = "Monitor shows the stock game camera (culled for it, not your head)." },
            // Webs & movement
            new Opt { Section = "WEBS & MOVEMENT", Param = "OverlayWebs", Kind = OptKind.Webs, Title = "SPIDY WEB STRANDS", Default = "1",
                      Sub = "Spidy's overlay web strands. Off uses the game's own web lines (-GameWebs)." },
            Opt.Sw("WEBS & MOVEMENT", "TriggerWebs", "WEBS ON TRIGGER", "Shoot webs with the trigger and reel in with the grip (default: grip webs).", false),
            Opt.Sw("WEBS & MOVEMENT", "ArcadeWebs", "ARCADE WEBS", "Webs pass through lamp posts, signs and cars; swings stay above the street.", false),
            // 0.0.62 REAL ANCHORS (default on): passes -RealAnchors (= -NoAirWebs). Older mods' scripts lack the
            // switch, so Supported() leaves it out and they keep their air swing.
            Opt.Sw("WEBS & MOVEMENT", "RealAnchors", "REAL ANCHORS", "Webs only stick to a real building or object within 100 m; off lets them hold in thin air.", true),
            Opt.Inv("WEBS & MOVEMENT", "NoWebGrab", "WEB GRAB", "Webs catch props and thugs: reel them in, yank, throw."),
            Opt.Num("WEBS & MOVEMENT", "SwingSpeed", "SWING SPEED CAP", "Top speed while swinging.", 1, 65, 1, 32, " m/s"),
            Opt.Num("WEBS & MOVEMENT", "Weight", "WEIGHT", "Gravity while webs fly you, % of real gravity.", 40, 300, 5, 80, "%"),
            Opt.Inv("WEBS & MOVEMENT", "NoStandOnWalls", "STAND ON WALLS", "Your view turns with the wall you stand on."),
            Opt.Inv("WEBS & MOVEMENT", "NoWallRun", "WALL RUNNING", "Run along walls (off: the game's own wall crawl takes you)."),
            Opt.Sw("WEBS & MOVEMENT", "Flips", "FLIPS (EXPERIMENTAL)", "In the air the left stick turns you over; tap A for one flip.", false),
            Opt.Num("WEBS & MOVEMENT", "FlipSpeed", "FLIP SPEED", "Degrees a second at full tilt of the left stick.", 90, 480, 10, 180, "\u00B0/s"),
            // Comfort
            Opt.Num("COMFORT", "SnapTurn", "SNAP TURN", "Angle of each snap turn. 0 = off.", 0, 90, 5, 30, "\u00B0"),
            Opt.Num("COMFORT", "SmoothTurn", "SMOOTH TURN", "Smooth turning speed, on by default; 0 snap turns instead, no restart.", 0, 360, 10, 120, "\u00B0/s"),
            Opt.Num("COMFORT", "Haptics", "VIBRATION", "Controller vibration strength.", 0, 100, 5, 100, "%"),
            Opt.Pick("COMFORT", "ScreenSize", "GAME SCREEN SIZE", "The screen for menus, cutscenes and flat mode.", new[] { "Small", "Medium", "Large" }, "Medium"),
            Opt.Pick("COMFORT", "ScreenDepth", "CUTSCENE 3D DEPTH", "Cutscene depth. Spider-Man 2 cutscenes stay flat for now: the HUD hook is not found yet.", new[] { "Off", "Low", "Medium", "High" }, "Medium"),
            Opt.Pick("COMFORT", "Hud", "VR HUD SIZE", "Size of the VR HUD panel.", new[] { "Off", "Small", "Medium", "Large" }, "Medium"),
            // Body & combat
            Opt.Inv("BODY & COMBAT", "NoBody", "SPIDER-MAN BODY", "Spider-Man's body follows your head and hands (off: gloves only)."),
            Opt.Inv("BODY & COMBAT", "NoPunch", "PUNCHING", "A fist that hits a thug fast enough knocks him back."),
            Opt.Inv("BODY & COMBAT", "NoWebShooter", "WEB SHOOTER", "A free hand's button shoots the game's web balls where it points."),
            Opt.Inv("BODY & COMBAT", "NoAimMarkers", "AIM MARKERS", "Show where each hand's web would land."),
            Opt.Inv("BODY & COMBAT", "NoPromptHelp", "PROMPT HELP", "Y sends whatever button the game's prompt asks for."),
            Opt.Inv("BODY & COMBAT", "NoCalibrationPrompt", "T-POSE PROMPT", "Ask for a T-pose at the first gameplay to size your body."),
            // Advanced
            Opt.Inv("ADVANCED", "NoEyeOcclusion", "EYE OCCLUSION", "Leave on. Off draws hidden things too (about half the frame rate)."),
            Opt.Sw("ADVANCED", "OwnShadows", "PER-EYE SHADOWS", "Each eye makes its own sun shadow maps (more processor work).", false),
            Opt.Sw("ADVANCED", "EyeLensDistances", "EYE LENS DISTANCES", "The eyes' own wide lenses for detail distances (detail loads nearer).", false),
            Opt.Sw("ADVANCED", "CaptureImages", "CAPTURE EYE IMAGES", "Save eye pictures into the report (for bug reports; slower).", false),
            // 0.0.74 test 3: the start's safety checks, each on by default (tools\mod_check.py, gpu_check.py, start_errors.py).
            Opt.Inv("SAFETY CHECKS", "NoModCheck", "MOD CHECK", "Offers to move other mods' loaders (winmm.dll...) aside for VR; no restart needed."),
            Opt.Inv("SAFETY CHECKS", "NoGpuCheck", "TWO-GPU CHECK", "Runs the game and SteamVR on the graphics card; no restart, SteamVR at its next start."),
            Opt.Inv("SAFETY CHECKS", "NoClearErrors", "CLEARER ERRORS", "Says why the game crashed or lost its graphics, with the fix; no restart needed."),
        };

        public static Opt Find(string p) { return All.FirstOrDefault(o => o.Param.Equals(p, StringComparison.OrdinalIgnoreCase)); }

        /// <summary>Which ps1 parameter(s) an option needs to work.</summary>
        public static bool Supported(Opt o, Dictionary<string, string> scriptParams)
        {
            if (scriptParams == null || scriptParams.Count == 0) return true;   // unknown script: trust it
            if (o.Kind == OptKind.Webs) return scriptParams.ContainsKey("GameWebs") || scriptParams.ContainsKey("OverlayWebs");
            return scriptParams.ContainsKey(o.Param);
        }

        static string Quote(string v)
        {
            double d;
            if (double.TryParse(v, NumberStyles.Float, CultureInfo.InvariantCulture, out d) || System.Text.RegularExpressions.Regex.IsMatch(v, @"^[A-Za-z][\w.]*$"))
                return v;
            return "'" + v.Replace("'", "''") + "'";
        }

        /// <summary>The arguments for launch-game-vr.ps1, as PowerShell text (for -EncodedCommand), from the stored values.</summary>
        public static List<string> Arguments(Func<Opt, string> value, Dictionary<string, string> scriptParams)
        {
            var args = new List<string>();
            foreach (var o in All)
            {
                if (!Supported(o, scriptParams)) continue;
                string v = value(o);
                switch (o.Kind)
                {
                    case OptKind.Switch:
                        if (v == "1")
                        {
                            if (o.Exclusive != null && value(Find(o.Exclusive)) == "1" && o.Param == "SmallerDesktopView") break; // Full wins
                            args.Add("-" + o.Param);
                        }
                        break;
                    case OptKind.Invert:
                        if (v == "0") args.Add("-" + o.Param);
                        break;
                    case OptKind.Webs:
                        if (v == "1") { if (scriptParams == null || scriptParams.ContainsKey("OverlayWebs")) args.Add("-OverlayWebs"); }
                        else args.Add("-GameWebs");
                        break;
                    default:
                        string scriptDefault = null;
                        if (scriptParams != null) scriptParams.TryGetValue(o.Param, out scriptDefault);
                        bool same = scriptDefault != null && string.Equals(scriptDefault, v, StringComparison.OrdinalIgnoreCase);
                        if (o.AlwaysPass || !same) { args.Add("-" + o.Param); args.Add(Quote(v)); }
                        // 0.0.73 fix 26: the script reads -SmoothTurn 0 alone as an older launcher's default (smooth), so
                        // a player's own SMOOTH TURN off also says -SnapTurning.
                        if (o.Param == "SmoothTurn" && v == "0" && (scriptParams == null || scriptParams.ContainsKey("SnapTurning")))
                        {
                            if (same) { args.Add("-" + o.Param); args.Add(Quote(v)); }
                            args.Add("-SnapTurning");
                        }
                        break;
                }
            }
            return args;
        }

        /// <summary>A short human summary of the options that differ from the mod's defaults, for the PLAY page.</summary>
        public static string Summary(Func<Opt, string> value)
        {
            var parts = new List<string>();
            parts.Add("Render " + value(Find("RenderScale")) + "%");
            if (value(Find("OverlayWebs")) == "1") parts.Add("overlay webs"); else parts.Add("game webs");
            if (value(Find("EngineTaa")) == "1") parts.Add("engine TAA");
            if (value(Find("FullDesktopView")) == "1") parts.Add("full desktop");
            else if (value(Find("SmallerDesktopView")) == "1") parts.Add("1280x720 desktop");
            if (value(Find("TriggerWebs")) == "1") parts.Add("trigger webs");
            if (value(Find("SmoothTurn")) != "0") parts.Add("smooth turn");
            return string.Join(" \u00B7 ", parts);
        }
    }
}
