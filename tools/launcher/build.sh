#!/bin/sh
# Linux/macOS test build with Mono (mcs). The real build is build.ps1 on Windows.
set -e
cd "$(dirname "$0")"
mkdir -p out
mcs -langversion:5 -target:winexe -optimize+ -out:out/SM2VRLauncher.exe \
  -win32icon:assets/sm2vr.ico \
  -r:System.Windows.Forms -r:System.Drawing -r:System.IO.Compression -r:System.IO.Compression.FileSystem \
  -r:System.Web.Extensions -r:System.Management \
  -resource:assets/background.jpg,background.jpg src/*.cs
echo "Built out/SM2VRLauncher.exe"
# Preview: SM2VR_LAUNCHER_PREVIEW=1 mono out/SM2VRLauncher.exe
