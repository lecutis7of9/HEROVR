#!/usr/bin/env python3
"""Version 0.0.72: the player's own gun sounds (sent by the player; sha256 below) are the Punisher's:
  assets/sfx/source/user-shotgun.mp3 -> assets/sfx/shotgun_blast_user.wav  (sfx::shotgunBlast, the sawed-off)
  assets/sfx/source/user-pistol.mp3  -> assets/sfx/pistol_shot_user.wav    (sfx::pistolShot, both pistols)
  assets/sfx/source/user-pistol-reload.mp3 -> assets/sfx/pistol_reload.wav  (sfx::pistolReload, once a reload)
and THOR's lightning (either, at random, once a strike: sfx::thorLightning1 / thorLightning2):
  assets/sfx/source/user-lightning-1.mp3 -> assets/sfx/thor_lightning_1.wav
  assets/sfx/source/user-lightning-2.mp3 -> assets/sfx/thor_lightning_2.wav
(CMakeLists SPIDY_SFX_FILE_shotgun_blast / _pistol_shot; pistol_reload is its own name.)

The game's sound player takes RIFF PCM only (sfx.cpp parseWav), so each MP3 is decoded once, as it is:
its own sample rate and channels, 16-bit, no trim, no gain, no filter, no effect (ffmpeg's plain decode;
the MP3's encoder delay is dropped by the decoder, as any player does). The MP3s are also shipped
unchanged in the test zip (Sounds/).

  python3 tools/sfx/user_guns.py          write the WAVs
  python3 tools/sfx/user_guns.py --check  decode again and compare (exit 1 on any difference)
Needs ffmpeg."""
import hashlib, os, subprocess, sys, tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
SFX = os.path.normpath(os.path.join(HERE, '..', '..', 'assets', 'sfx'))
SOUNDS = {  # source mp3 -> (sha256, the WAV the build embeds)
    'user-shotgun.mp3': ('a51791b5c1ccf45fb6ac2f401617f714d78900e5245593a5d0f6e12421533795', 'shotgun_blast_user.wav'),
    'user-pistol.mp3': ('b1e432f8ddb3284ed080ce6cbe548f18722cfe4bb8d3c8376d4ee91a28561f49', 'pistol_shot_user.wav'),
    'user-pistol-reload.mp3': ('5907266fc5026ecdf8969e39c115ce09cfbba4db170ab123e186c7d7c7585ba4', 'pistol_reload.wav'),
    'user-lightning-1.mp3': ('d3465014365c783da44311f7ab0c41fcbcb69af733855f6ce50f0634a1ac94c9', 'thor_lightning_1.wav'),
    'user-lightning-2.mp3': ('b1e73c2f28d8c789437befea7ec0d0e6542f5f80b0e838cbded282bbbfe0f514', 'thor_lightning_2.wav'),
}


def sha(path):
    return hashlib.sha256(open(path, 'rb').read()).hexdigest()


def source(name):
    return os.path.join(SFX, 'source', name)


def decode(src, dst):
    subprocess.run(['ffmpeg', '-v', 'error', '-y', '-i', src, '-map_metadata', '-1', '-fflags', '+bitexact',
                    '-flags:a', '+bitexact', '-c:a', 'pcm_s16le', '-f', 'wav', dst], check=True)


def main(check=False):
    bad = 0
    for name, (digest, wav) in SOUNDS.items():
        src, out = source(name), os.path.join(SFX, wav)
        if not os.path.exists(src) or sha(src) != digest:
            print('MISSING OR CHANGED: the player\'s sound ' + src)
            bad += 1
            continue
        if not check:
            decode(src, out)
            print(os.path.relpath(out), os.path.getsize(out), 'bytes')
            continue
        with tempfile.TemporaryDirectory(prefix='sm2072_sfx_') as d:
            p = os.path.join(d, 'x.wav')
            decode(src, p)
            same = os.path.exists(out) and open(p, 'rb').read() == open(out, 'rb').read()
        print(wav, 'same' if same else 'DIFFERENT: run tools/sfx/user_guns.py')
        bad += not same
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main('--check' in sys.argv))
