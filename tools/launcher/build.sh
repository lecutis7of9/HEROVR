#!/bin/sh
# Linux/macOS test build with Mono (mcs). The real build is build.ps1 on Windows (csc from .NET Framework 4.8).
# Compiles against the .NET Framework 4.8 REFERENCE assemblies (Mono's 4.8-api), not Mono's own runtime
# library, so an API that Windows doesn't have (e.g. string.Trim(char)) fails here at build time.
set -e
cd "$(dirname "$0")"
OUT=${1:-out/SM2VRLauncher.exe}
API=${NETFX_API:-/usr/lib/mono/4.8-api}
[ -f "$API/mscorlib.dll" ] || { echo "No .NET Framework 4.8 reference assemblies at $API (install mono-devel / referenceassemblies-pcl)"; exit 1; }
python3 tools/check-netfx.py src
mkdir -p "$(dirname "$OUT")"
mcs -nostdlib -noconfig -lib:"$API" -r:"$API/mscorlib.dll" -r:"$API/System.dll" -r:"$API/System.Core.dll" \
  -r:"$API/System.Drawing.dll" -r:"$API/System.Windows.Forms.dll" -r:"$API/System.IO.Compression.dll" \
  -r:"$API/System.IO.Compression.FileSystem.dll" -r:"$API/System.Web.Extensions.dll" -r:"$API/System.Management.dll" \
  -langversion:5 -target:winexe -optimize+ -platform:anycpu -out:"$OUT" \
  -win32icon:assets/sm2vr.ico -resource:assets/background.jpg,background.jpg \
  -resource:assets/hero/orb.bin,hero.orb.bin -resource:assets/hero/intro.wav,hero.intro.wav -resource:assets/hero/loop.wav,hero.loop.wav \
  -resource:assets/hero/Barlow-Regular.ttf,hero.Barlow-Regular.ttf -resource:assets/hero/Barlow-Medium.ttf,hero.Barlow-Medium.ttf src/*.cs
python3 tools/check-netfx.py src "$OUT"
echo "Built $OUT (against .NET Framework 4.8 reference assemblies)"
# Preview: SM2VR_LAUNCHER_PREVIEW=1 mono out/SM2VRLauncher.exe
