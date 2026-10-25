# Publishing an update the launcher can install

The launcher's **CHECK FOR UPDATE** and **UPDATE NOW** read GitHub Releases. Nothing has been created or uploaded
yet. These are the steps for you to do.

## Once: the repository

1. Create a GitHub repository, for example `sm2-vr`. It can be public, or private if every player has access.
   Because the mod is GPLv3, the source has to be available to everyone you give the mod to. A public repo is
   the simplest way.
2. Tell the launcher where it is, using either:
   * UPDATES tab → GITHUB REPOSITORY → `yourname/sm2-vr` → SAVE, or
   * `launcher.ini` beside the exe: `UpdateRepo=yourname/sm2-vr`. This ships the default to players.

   Until it's set, the launcher shows the placeholder `YOUR-GITHUB-NAME/sm2-vr` and the update check explains
   what to do.

## Every release

### Version numbers

Releases use semantic versioning (`MAJOR.MINOR.PATCH`). The **first published release is Version 0.0.1**
(internal build 38.5), tag **`v0.0.1`**. Every later release starts from the last published one:

| Change | Bump | Example after 0.0.1 |
|---|---|---|
| fixes, tuning, small features (the usual release) | PATCH | `0.0.2`, `0.0.3`, … `0.0.10` |
| a bigger feature set or a settings-format change | MINOR, PATCH back to 0 | `0.1.0` |
| the first "complete" release, or changes that break old installs | MAJOR, the rest back to 0 | `1.0.0` |

Never reuse or lower a published number. Test builds can be pre-releases such as `0.0.2-beta.1`, which sort
below `0.0.2` (publish them as GitHub pre-releases so `releases/latest` skips them).

The mod folder states its version in either of two files. The launcher and both generators read them in this
order:

1. `VERSION`: one line, the bare semver: `0.0.1`
2. `PORT-VERSION`: `Version 0.0.1 (build 38.5)`. The semver is parsed out, and `(build NN.N)` is an
   optional note about the internal build (written to `version.json` as `internal_build`).
3. A legacy `PORT-VERSION` of `Update 38.5`, from installs made before 0.0.1. The launcher still reads it, and
   counts **every** legacy version as older than 0.0.1, so those installs are offered 0.0.1. The generators
   refuse to publish a legacy version.

The release tag is always `v` + the version (`v0.0.1`). The generators reject a `-Tag` that doesn't match, and a
zip whose own `VERSION` / `PORT-VERSION` names a different version. For every release, update `VERSION` (and the
`PORT-VERSION` line, if you keep it) first, then make the zips.

### 1. Make the zips (from the repo root)

The examples below are for the second release, 0.0.2. For the first one (0.0.1) use `0.0.1` in the names and the
tag `v0.0.1`. Nobody has an earlier published release to update from, so 0.0.1 needs the full zip; an update
zip for it would only fit the exact internal build it was made against.

```powershell
mkdir dist -Force
# a. The update zip: the files that changed, as today
python tools/make_update_zip.py changed-files.txt dist/sm2-vr-0.0.2.zip
# b. The full source zip: every tracked file. This is what REVERT downloads for older versions.
git ls-files > dist/all-files.txt
python tools/make_update_zip.py dist/all-files.txt dist/sm2-full-0.0.2.zip
```

`make_update_zip.py` dates every entry two days in the past, so ninja never sees files from the future.

* An update zip expands over an install, so it only works on top of the version before it.
* A full zip makes an install exactly that version. The launcher prefers it for reverting, and also for updating
  when there is no update zip.
* The prebuilt player zip from `tools\package.ps1` (`SM2-VR-<ver>-win64.zip`) has no sources, so it can't be
  built. List it as `package` (play-only).

Asset names must not contain spaces, because GitHub replaces them with dots.

### 2. Write version.json and the .sha256 files

```powershell
powershell -ExecutionPolicy Bypass -File tools\make-version-json.ps1 -Repo yourname/sm2-vr `
  -UpdateZip dist\sm2-vr-0.0.2.zip -SourceZip dist\sm2-full-0.0.2.zip -Out dist\version.json
# or: python tools/make_version_json.py --repo yourname/sm2-vr --update-zip dist/sm2-vr-0.0.2.zip --source-zip dist/sm2-full-0.0.2.zip --out dist/version.json
```

This reads the version (`VERSION`, else `PORT-VERSION`) and writes `dist\version.json` plus `dist\<zip>.sha256` for each zip.
The options are:

* `-Version` (`--version`): a semver such as `0.0.2`, to override the files
* `-BuildNote` (`--build-note`): the internal build note, default the `(build NN.N)` in `PORT-VERSION`
* `-Tag`: must be `v<version>`, which is also the default
* `-Date`, default today (UTC)
* `-PackageZip`
* `-MinGameVersion`, default `2.810.0.0`
* `-InstallSubfolder`, default `sm2-port`
* `-ChangelogUrl` and `-SourceUrl`, which default to the release page and the repo

Example output: [docs/version.example.json](docs/version.example.json).

| Field | Meaning |
|---|---|
| `schema` | 1. Launchers refuse a higher schema they don't know. |
| `name`, `version`, `version_number`, `tag`, `release_date` | e.g. `SM2 VR`, `Version 0.0.1`, `0.0.1`, `v0.0.1`, `2026-10-25`. Launchers compare `version_number` as semver. |
| `internal_build` | optional, informational: `38.5` for 0.0.1 |
| `min_game_version` | `2.810.0.0`. The launcher won't install on another game version. |
| `install_subfolder` | the folder name a fresh install should use (`sm2-port`) |
| `files[]` | `{kind, name, url, sha256, size}` where kind is `update`, `source` or `package`. URLs are https release-asset URLs. |
| `build` | `{script: tools/build.ps1, args: [-Observer]}`, run from the install folder after expanding |
| `launch` | `{script: tools/launch-game-vr.ps1, args: [-RenderScale, 100, -OverlayWebs, -EngineTaa, -SmallerDesktopView]}` |
| `changelog_url`, `source_url`, `license` (`GPL-3.0`), `credits[]` | for the launcher's UI and for other launchers |

### 3. Create the release and upload

On the website: Releases → Draft a new release → tag `v0.0.2`, title `Version 0.0.2`, notes → attach
`sm2-vr-0.0.2.zip`, `sm2-full-0.0.2.zip`, both `.sha256` files and `version.json` → Publish.
Or use the GitHub CLI:

```powershell
gh release create v0.0.2 --title "Version 0.0.2" --notes-file notes.md `
  dist\sm2-vr-0.0.2.zip dist\sm2-full-0.0.2.zip dist\sm2-vr-0.0.2.zip.sha256 dist\sm2-full-0.0.2.zip.sha256 dist\version.json
```

Don't mark it as a pre-release. `releases/latest` skips pre-releases and drafts.

### 4. Check

* `https://github.com/yourname/sm2-vr/releases/latest/download/version.json` opens the new manifest.
* Launcher → UPDATES → CHECK FOR UPDATE says *Version 0.0.2, released …: newer than your 0.0.1*.
* On a test copy (`launcher.ini` with `Portable=1` and its own install folder), UPDATE NOW downloads, checks the
  sha256, backs up, expands, builds and shows *Version 0.0.2*.

## How the launcher reads it

1. `GET https://github.com/<repo>/releases/latest/download/version.json`. This is a plain download with no API
   rate limit.
2. If there is no manifest, it uses the GitHub API (`/repos/<repo>/releases/latest`) and that release's
   `version.json` asset. If that doesn't exist either, it uses `sm2-vr-*.zip` (or the older `sm2-update-*.zip`) / `sm2-full-*.zip`, with
   sha256 taken from a `<zip>.sha256` or `SHA256SUMS` asset or a hash in the release notes.
3. REVERT → "Download an older release" lists `/repos/<repo>/releases` and uses each release's `version.json`.

A release with no sha256 anywhere can still be installed, but only after a warning that it can't be verified.

## If a release is bad

Delete or edit the release, or publish a fixed one with the next number (`0.0.3` after a bad `0.0.2`). Players who already updated can press
REVERT. Their previous version is in `C:\sm2-port-backup\` (the last 3 are kept), with its settings files.
