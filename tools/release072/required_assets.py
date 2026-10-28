"""Version 0.0.72: what the test zip must carry (the zip builder and tests/hero072_assets_tests.py share
this list, so a missing model or sound fails the gate before a zip is made)."""
import os

REPO = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..'))

# The converted models (tools/models072/convert_models_072.py -> build/h7out/Models): one scene each.
MODEL_FILES = [
    'Models/Batarang/EIRA/scene.glb',
    'Models/Batarang/CLOONEY/scene.glb',
    'Models/Batarang/JOKER/scene.gltf',
    'Models/Pistol/scene.glb',
    'Models/SawedOff/Frame/scene.glb',
    'Models/SawedOff/Barrels/scene.glb',
    'Models/Baton/Left/scene.glb',
    'Models/Baton/Right/scene.glb',
    'Models/Shield/scene.glb',
    'Models/GrappleGun/scene.glb',
    # Version 0.0.72 THOR HAMMER (#1 default, #5, #6, #3) and the PUNISHER VAN (snafuj; its cab and wheel BUILT).
    'Models/Hammer/BOOOOP/scene.glb',
    'Models/Hammer/DEVILSEYE/scene.glb',
    'Models/Hammer/LOVEANDTHUNDER/scene.glb',
    'Models/Hammer/FCCOOPER/scene.glb',
    'Models/Vehicle/PunisherVan/scene.glb',
    'Models/Vehicle/PunisherVan/Interior/scene.glb',
    'Models/Vehicle/PunisherVan/SteeringWheel/scene.glb',
    # Build 8 ROSTER page: every profile picture and suit preview (tools/roster/make_roster_glb.py; credits in license.txt).
    'Models/Roster/scene.glb',
    # Version 0.0.73 (fix 10) RETRACTABLE CLAWS: the JACKET AND JEANS suit's own claws (tools/claws073/extract_claws.py).
    'Models/Claws/Left/scene.glb',
    'Models/Claws/Right/scene.glb',
]
# Build 9 SUITS: the 23 suits the user sent (tools/suits/convert_suit.py; mesh_model::suitFolders' order).
SUIT_FOLDERS = ['Suit_batman_1', 'Suit_batman_2', 'Suit_batman_3', 'Suit_batman_5', 'Suit_ironMan_1', 'Suit_ironMan_2',
                'Suit_ironMan_3', 'Suit_ironMan_4', 'Suit_captainAmerica_1', 'Suit_captainAmerica_2', 'Suit_daredevil_1',
                'Suit_daredevil_3', 'Suit_hulk_2', 'Suit_wolverine_1', 'Suit_wolverine_3', 'Suit_wolverine_4', 'Suit_punisher_1',
                'Suit_punisher_2', 'Suit_thor_1', 'Suit_thor_2', 'Suit_greenLantern_2', 'Suit_greenLantern_3', 'Suit_superman_4']
MODEL_FILES += ['Models/Suits/%s/scene.glb' % f for f in SUIT_FOLDERS]
# Version 0.0.72: the CC BY credit beside each new model (the artist's own license.txt; the built parts' note).
LICENSE_FILES = [os.path.dirname(m) + '/license.txt' for m in MODEL_FILES[10:]]

# The player's own sounds, shipped unchanged (sha256 as sent) beside the mod, and the WAV the game embeds
# from each (tools/sfx/user_guns.py; CMake SPIDY_SFX_FILE_<name> or the name itself).
USER_SOUNDS = {
    'user-shotgun.mp3': ('a51791b5c1ccf45fb6ac2f401617f714d78900e5245593a5d0f6e12421533795', 'shotgun_blast', 'shotgun_blast_user'),
    'user-pistol.mp3': ('b1e432f8ddb3284ed080ce6cbe548f18722cfe4bb8d3c8376d4ee91a28561f49', 'pistol_shot', 'pistol_shot_user'),
    'user-pistol-reload.mp3': ('5907266fc5026ecdf8969e39c115ce09cfbba4db170ab123e186c7d7c7585ba4', 'pistol_reload', 'pistol_reload'),
    'user-lightning-1.mp3': ('d3465014365c783da44311f7ab0c41fcbcb69af733855f6ce50f0634a1ac94c9', 'thor_lightning_1', 'thor_lightning_1'),
    'user-lightning-2.mp3': ('b1e73c2f28d8c789437befea7ec0d0e6542f5f80b0e838cbded282bbbfe0f514', 'thor_lightning_2', 'thor_lightning_2'),
}
SOUND_FILES = ['Sounds/' + k for k in USER_SOUNDS]
DOC_FILES = ['READ-ME-FIRST.txt', 'SHA256SUMS.txt']
# Test build 5: the zip's one top folder holds the installer and the notes; the mod files sit under payload/
# (the installer copies them into the mod folder the player plays from; tools/run_game_vr.py reads the marker).
INSTALLER_FILES = ['INSTALL-TEST-BUILD.bat', 'INSTALL-TEST-BUILD.ps1']
TEST_MARKER = 'HERO-VR-TEST-BUILD.txt'


def top_folder(build_no):
    return 'HERO-VR-0.0.72-test%s' % build_no


def required_zip_files():
    """Every file the zip must carry, as named inside its top folder."""
    return (INSTALLER_FILES + ['READ-ME-FIRST.txt'] +
            ['payload/' + f for f in MODEL_FILES + LICENSE_FILES + SOUND_FILES + [TEST_MARKER, 'SHA256SUMS.txt']])


def source_mp3(name):
    return os.path.join(REPO, 'assets', 'sfx', 'source', name)
