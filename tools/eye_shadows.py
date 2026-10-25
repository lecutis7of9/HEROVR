"""SpidyShadowData (stereo_probe.cpp): what each eye's scene submit made of its key-light shadow cascades.

Version 1 (736 bytes) has the eyes' own cascades; version 2 (Update 25, 784 bytes) adds how far each eye's nearest
cascade sat from the game view's own. Its head_* fields were Update 25's HEAD SHADOWS, removed in Update 26 (SM2
never fits an eye's cascades inside its submit): they stay 0 and are no longer reported.
"""
import struct

MAGIC = 0x53534844
SIZES = {1: 736, 2: 784}


def parse(raw):
    """The snapshot from SpidyShadowData's bytes, or None if they are not it."""
    if len(raw) < 12:
        return None
    magic, version, size = struct.unpack_from('<3I', raw)
    if magic != MAGIC or SIZES.get(version) != size or len(raw) < size:
        return None
    passes, compared, covered = struct.unpack_from('<3Q', raw, 24)
    deltas = struct.unpack_from('<4f', raw, 48)
    eyes = []
    for offset in (64, 400):
        submits, taken, flags, thread = struct.unpack_from('<2Q2I', raw, offset)
        static, moving, may_take, second, lights, slot = struct.unpack_from('<4B2H', raw, offset+24)
        eyes.append(dict(submits=submits, taken=taken, flags=flags, cascades=[static, moving], may_take=may_take,
                         second=second, lights=lights, shadow_map=slot,
                         extent=[list(struct.unpack_from('<3f', raw, offset+32+k*12)) for k in range(4)],
                         position=[list(struct.unpack_from('<3f', raw, offset+80+k*64+48)) for k in range(4)]))
    result = dict(version=version, mode=struct.unpack_from('<I', raw, 12)[0], passes=passes, compared=compared,
                  covered=covered, pose_delta=deltas[0], extent_delta=deltas[1], pose_delta_most=deltas[2],
                  extent_delta_most=deltas[3], eyes=eyes)
    if version >= 2:
        head_fits, head_missed = struct.unpack_from('<2Q', raw, 736)
        head_fault, head_submits = struct.unpack_from('<2I', raw, 752)
        game_compared = struct.unpack_from('<Q', raw, 760)[0]
        game = struct.unpack_from('<4f', raw, 768)
        result.update(head_fits=head_fits, head_missed=head_missed, head_fault=head_fault,
                      head_submits=head_submits, game_compared=game_compared,
                      game_delta_m=[round(x, 4) for x in game[:2]], game_delta_most_m=[round(x, 4) for x in game[2:]])
    return result


def snapshot(game, address):
    """A consistent read (the sequence even and unchanged), or None."""
    try:
        for _ in range(8):
            head = game.read(address, 12)
            if len(head) < 12:
                return None
            size = struct.unpack_from('<I', head, 8)[0]
            if size not in SIZES.values():
                return None
            raw = game.read(address, size)
            if struct.unpack_from('<q', raw, 16)[0] % 2 == 0 and game.read(address+16, 8) == raw[16:24]:
                return parse(raw)
    except Exception:  # noqa: BLE001 - a diagnostic; the game may have closed
        return None
    return None


def line(s):
    """The console's word on the eyes' shadows vs the game view's, or None without those numbers."""
    if not s or s.get('version', 1) < 2 or not s.get('game_compared'):
        return None
    most = max(s['game_delta_most_m'])
    return (f"Eye shadows: each eye's nearest shadow cascade vs the game view's: {s['game_delta_m'][0]:.3f} / "
            f"{s['game_delta_m'][1]:.3f} m at the end, {most:.3f} m at most ({s['game_compared']} frames).")
