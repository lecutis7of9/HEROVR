#!/usr/bin/env python3
# SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3; SpidyVR by Ilya Mezerowsky.
# Made by Agent Mulder.
"""HERO VR MP Test Buddy (0.0.73 MP SOLO TEST): a second "player" for the solo multiplayer test.

Run it on the same PC as the game (or another PC on the LAN, see READ-ME-FIRST) while HERO VR runs with
MP SOLO TEST ON. It says HELLO to the game's UDP port 39470; once the game answers WELCOME, its puppet is
the remote player you see (in place of your loopback ghost): it walks a circle, waves, and changes suit.

Keys (in this window):  S next suit   H next hero (no suit)   W wave on/off   C circle on/off
                        P next props show (0.0.74 MP PROPS: what PLAYER 2 holds, wears and throws)   Q quit
Plain Python 3 (the HERO VR bundled one), no packages, nothing written anywhere; Ctrl+C quits too.
The packet is the same as src/mp_net.cpp's (version 1; tests/mp_buddy_tests.py checks them byte for byte; 0.0.74: the
POSE carries the flight lean, 124 bytes; 112-byte ones still read, upright; 0.0.74 MP PROPS: the 'P' PROPS tail
after LOOK - fake props, so the batarang, Thor's hammer, the shield and the batons are seen without a second player).
"""
import argparse
import math
import os
import select
import socket
import struct
import sys
import time
import zlib

MAGIC = b'HVMP'
VERSION = 1
POSE, HELLO, WELCOME, PING, PONG, BYE = 1, 2, 3, 4, 5, 6
ROLE_GAME, ROLE_ECHO, ROLE_BUDDY = 0, 1, 2
FLYING, SWINGING, GRAPPLE, FIRING, HAND_L, HAND_R = 1, 2, 4, 8, 16, 32
PAYLOAD = {POSE: 112, HELLO: 20, WELCOME: 20, PING: 4, PONG: 8, BYE: 0}
HEADER = struct.Struct('<4sBBBBIIHH')  # 20 bytes
POSE_FMT = struct.Struct('<BBBBI3ff3f4f3f4f3f4fBBH')  # 112 bytes (the 0.0.73 POSE; still read, upright)
LEAN_FMT = struct.Struct('<3f')  # 0.0.74 FLIGHT POSE: + the body's leaned up, 124 bytes in all
assert HEADER.size == 20 and POSE_FMT.size == 112 and LEAN_FMT.size == 12
# 0.0.73 MPSUIT: the LOOK tail after the lean ('L', flags 1 a look / 2 learned, primary, secondary, emblem as
# sRGB bytes, 0) = 136 bytes; test 6 builds read the first 112 bytes and ignore the rest, this build reads a pose without it.
LOOK_BYTES = 12
LOOK_AT = POSE_FMT.size + LEAN_FMT.size  # 124
# 0.0.74 MP PROPS: the 'P' tail after LOOK (src/mp_props.cpp: header 8, items 24, lines 14, effects 18; at most
# 8 / 5 / 4; positions cm i16, directions unit x 32767, spin 1/4096) - only written when there are props.
PROPS_AT = LOOK_AT + LOOK_BYTES  # 136
PROPS_HEAD = struct.Struct('<cBBBBBBB')
PROP_ITEM = struct.Struct('<BBBBh3h3h3h')
PROP_LINE = struct.Struct('<BB3h3h')
PROP_FX = struct.Struct('<HBB3h3hBB')
assert PROPS_HEAD.size == 8 and PROP_ITEM.size == 24 and PROP_LINE.size == 14 and PROP_FX.size == 18
ITEMS_MOST, LINES_MOST, FX_MOST = 8, 5, 4
# Kinds (mp_props.hpp PropKind; 1-11 hero007::Look)
PISTOL, SAWED_OFF, SAWED_OFF_OPEN, BATARANG, CLAWS, CLUB, BATON062, BATON_MODEL, SHIELD, SHIELD_SPIN, GRAPPLE_GUN = range(1, 12)
HAMMER, BATON, PELLET, RING, BUBBLE, FIST, CHARGE, PALM = range(20, 28)
F_SIDE, F_PALM, F_IN_HAND, F_SHEATHED, F_FLYING = 8, 16, 32, 64, 128
LINE_KINDS = ('heat', 'freeze', 'grapple', 'cable', 'bolt', 'lantern', 'chain', 'repulsor', 'chest', 'hook', 'rope')
FX_KINDS = ('flash', 'flash_shotgun', 'impact', 'smoke', 'scorch', 'splash', 'shock')
DEFAULT_PORT = 39470
NO_SUIT = 255

HEROES = ['SPIDER-MAN', 'IRON MAN', 'DAREDEVIL', 'HULK', 'BATMAN', 'WOLVERINE', 'SUPERMAN', 'PUNISHER',
          'CAPTAIN AMERICA', 'THOR', 'GREEN LANTERN']
HERO_KEYS = ['spiderMan', 'ironMan', 'daredevil', 'hulk', 'batman', 'wolverine', 'superman', 'punisher',
             'captainAmerica', 'thor', 'greenLantern']
# mesh_model::suitFolders (the same order: the index is the suit id; the hash finds it in any build).
SUITS = ['Suit_batman_1', 'Suit_batman_2', 'Suit_batman_3', 'Suit_batman_5', 'Suit_ironMan_1', 'Suit_ironMan_2',
         'Suit_ironMan_3', 'Suit_ironMan_4', 'Suit_captainAmerica_1', 'Suit_captainAmerica_2', 'Suit_daredevil_1',
         'Suit_daredevil_3', 'Suit_hulk_2', 'Suit_wolverine_1', 'Suit_wolverine_3', 'Suit_wolverine_4',
         'Suit_punisher_1', 'Suit_punisher_2', 'Suit_thor_1', 'Suit_thor_2', 'Suit_greenLantern_2',
         'Suit_greenLantern_3', 'Suit_superman_4']


def fnv1a(text):
    h = 2166136261
    for b in text.encode('ascii'):
        h = ((h ^ b) * 16777619) & 0xffffffff
    return h


def hero_of_suit(folder):
    key = folder.split('_')[1]
    return HERO_KEYS.index(key) if key in HERO_KEYS else 0


def clean_name(name):
    out = ''.join(c if 32 <= ord(c) < 127 else ' ' for c in name.upper())[:16]
    return out


def encode(ptype, role, seq, sent_ms, payload=b'', hold_ms=0):
    body = payload
    head = HEADER.pack(MAGIC, VERSION, ptype, role, 0, seq & 0xffffffff, sent_ms & 0xffffffff, hold_ms, len(body))
    data = head + body
    return data + struct.pack('<I', zlib.crc32(data) & 0xffffffff)


def quat_ok(q):
    n2 = sum(c * c for c in q)
    return all(math.isfinite(c) for c in q) and 0.25 < n2 < 2.25


def pose_payload(p):
    """p: dict(hero, suit, actions, suit_hash, root(3), yaw, head(pos3, quat4), hands[2](pos3, quat4), curl(2), eye,
    lean_up(3), optional: upright (0, 1, 0))."""
    def clamp01(x):
        return max(0.0, min(1.0, x))
    (hp, hq), (lp, lq), (rp, rq) = p['head'], p['hands'][0], p['hands'][1]
    look = p.get('look')  # None, or (learned, primary, secondary, emblem) with each colour 0xRRGGBB
    flags, cols = 0, (0, 0, 0)
    if look:
        flags, cols = 1 | (2 if look[0] else 0), tuple(c & 0xffffff for c in look[1:4])
    tail = b'L' + bytes([flags]) + b''.join(c.to_bytes(3, 'big') for c in cols) + b'\0'
    return POSE_FMT.pack(p['hero'], p['suit'], p['actions'] & 63, 0, p['suit_hash'], *p['root'], p['yaw'],
                         *hp, *hq, *lp, *lq, *rp, *rq, round(clamp01(p['curl'][0]) * 255),
                         round(clamp01(p['curl'][1]) * 255), round(max(0.0, min(3.0, p['eye'])) * 1000)) + \
        LEAN_FMT.pack(*safe_lean_up(p.get('lean_up', (0.0, 1.0, 0.0)))) + tail + props_tail(p.get('props'))


def c_round(x):
    """C++ std::lround: halves away from zero."""
    return int(math.floor(abs(x) + 0.5)) * (1 if x >= 0 else -1)


def _cm(v):
    return tuple(max(-32767, min(32767, c_round(c * 100.0))) for c in v)


def _unit(v, fallback):
    n = math.sqrt(sum(c * c for c in v)) if all(math.isfinite(c) for c in v) else 0.0
    v = tuple(c / n for c in v) if n > 1e-4 else fallback
    return tuple(max(-32767, min(32767, c_round(c * 32767.0))) for c in v)


def _spin(s):
    if not math.isfinite(s):
        return 0
    if abs(s) > 7.99:
        s = math.remainder(s, 6.2831853)
    return max(-32767, min(32767, c_round(s * 4096.0)))


def props_tail(props):
    """props: dict(flags, items=[dict(id, kind, flags, variant, spin, at, forward, up)], lines=[dict(kind, fade, from,
    to)], fx=[dict(seq, kind, age, at, way, param)]) -> the 'P' tail bytes (b'' when empty)."""
    if not props:
        return b''
    items = list(props.get('items', ()))[:ITEMS_MOST]
    lines = list(props.get('lines', ()))[:LINES_MOST]
    fx = list(props.get('fx', ()))[:FX_MOST]
    if not (items or lines or fx):
        return b''
    out = PROPS_HEAD.pack(b'P', 1, len(items), len(lines), len(fx), props.get('flags', 0) & 255, 0, 0)
    for it in items:
        out += PROP_ITEM.pack(it['id'], it['kind'], it.get('flags', 0), it.get('variant', 0), _spin(it.get('spin', 0.0)),
                              *_cm(it['at']), *_unit(it.get('forward', (0, 0, -1)), (0.0, 0.0, -1.0)),
                              *_unit(it.get('up', (0, 1, 0)), (0.0, 1.0, 0.0)))
    for ln in lines:
        out += PROP_LINE.pack(ln['kind'], max(0, min(255, c_round(max(0.0, min(1.0, ln.get('fade', 1.0))) * 255.0))),
                              *_cm(ln['from']), *_cm(ln['to']))
    for f in fx:
        out += PROP_FX.pack(f.get('seq', 0) & 0xffff, f['kind'], max(0, min(255, c_round(f.get('age', 0.0) / 0.004))),
                            *_cm(f['at']), *_unit(f.get('way', (0, 0, -1)), (0.0, 0.0, -1.0)), f.get('param', 0) & 255, 0)
    return out


def decode_props(t):
    """The 'P' tail as a dict, or None (same checks as decodeProps)."""
    if len(t) < 8 or t[0] != ord('P') or t[1] != 1:
        return None
    ni, nl, nf, flags = t[2], t[3], t[4], t[5]
    if ni > ITEMS_MOST or nl > LINES_MOST or nf > FX_MOST or len(t) < 8 + 24 * ni + 14 * nl + 18 * nf:
        return None
    at, out = 8, dict(flags=flags, items=[], lines=[], fx=[])
    for _ in range(ni):
        v = PROP_ITEM.unpack_from(t, at)
        at += 24
        out['items'].append(dict(id=v[0], kind=v[1], flags=v[2], variant=v[3], spin=v[4] / 4096,
                                 at=tuple(c / 100 for c in v[5:8]), forward=tuple(c / 32767 for c in v[8:11]),
                                 up=tuple(c / 32767 for c in v[11:14])))
    for _ in range(nl):
        v = PROP_LINE.unpack_from(t, at)
        at += 14
        out['lines'].append(dict(kind=v[0], fade=v[1] / 255, **{'from': tuple(c / 100 for c in v[2:5])},
                                 to=tuple(c / 100 for c in v[5:8])))
    for _ in range(nf):
        v = PROP_FX.unpack_from(t, at)
        at += 18
        out['fx'].append(dict(seq=v[0], kind=v[1], age=v[2] * 0.004, at=tuple(c / 100 for c in v[3:6]),
                              way=tuple(c / 32767 for c in v[6:9]), param=v[9]))
    return out


def safe_lean_up(up):
    """The same as mp_net's safeLeanUp: a broken or upside-down lean is upright, else unit."""
    if len(up) != 3 or not all(math.isfinite(c) for c in up):
        return (0.0, 1.0, 0.0)
    n = math.sqrt(sum(c * c for c in up))
    if not 0.5 < n < 2.5 or up[1] / n <= -0.18:
        return (0.0, 1.0, 0.0)
    return tuple(c / n for c in up)


def name_payload(name):
    return clean_name(name).encode('ascii').ljust(16, b'\0') + bytes([VERSION, 0, 0, 0])


class BadPacket(ValueError):
    pass


def decode(data):
    """A packet as a dict; BadPacket with the reason (same checks as the C++ side)."""
    if len(data) < 24:
        raise BadPacket('too short')
    if len(data) > 512:
        raise BadPacket('too long')
    magic, version, ptype, role, _, seq, sent, hold, length = HEADER.unpack_from(data)
    if magic != MAGIC:
        raise BadPacket('not a HERO VR packet')
    if version != VERSION:
        raise BadPacket('another protocol version')
    if ptype not in PAYLOAD:
        raise BadPacket('unknown type')
    if length < PAYLOAD[ptype] or 20 + length + 4 != len(data):
        raise BadPacket('wrong length')
    if struct.unpack_from('<I', data, 20 + length)[0] != zlib.crc32(data[:20 + length]) & 0xffffffff:
        raise BadPacket('bad checksum')
    if role > 2:
        raise BadPacket('bad value')
    out = dict(type=ptype, role=role, seq=seq, sent_ms=sent, hold_ms=hold)
    body = data[20:20 + length]
    if ptype == POSE:
        v = POSE_FMT.unpack_from(body)
        out.update(hero=v[0], suit=v[1], actions=v[2], suit_hash=v[4], root=v[5:8], yaw=v[8],
                   head=(v[9:12], v[12:16]), hands=((v[16:19], v[19:23]), (v[23:26], v[26:30])),
                   curl=(v[30] / 255, v[31] / 255), eye=v[32] / 1000,
                   lean_up=safe_lean_up(LEAN_FMT.unpack_from(body, POSE_FMT.size)) if length >= 124 else (0.0, 1.0, 0.0),
                   look=None, props=None)
        if length >= LOOK_AT + LOOK_BYTES and body[LOOK_AT] == ord('L') and body[LOOK_AT + 1] & 1:
            t = body[LOOK_AT:]
            out['look'] = (bool(t[1] & 2),) + tuple(int.from_bytes(t[2 + 3 * i:5 + 3 * i], 'big') for i in range(3))
        if length > PROPS_AT:  # 0.0.74 MP PROPS (a tail that does not parse: no props, the pose stands)
            out['props'] = decode_props(body[PROPS_AT:])
        nums = list(out['root']) + [out['yaw']] + list(v[9:30])
        if not all(math.isfinite(x) for x in nums) or out['hero'] >= 32:
            raise BadPacket('bad value')
        if not all(quat_ok(q) for q in (v[12:16], v[19:23], v[26:30])):
            raise BadPacket('bad value')
    elif ptype in (HELLO, WELCOME):
        out.update(name=body[:16].rstrip(b'\0').decode('ascii', 'replace'), protocol=body[16])
    elif ptype == PING:
        out.update(token=struct.unpack_from('<I', body)[0])
    elif ptype == PONG:
        out.update(token=struct.unpack_from('<I', body)[0], ping_sent_ms=struct.unpack_from('<I', body, 4)[0])
    return out


def quat_about(axis, angle):
    x, y, z = axis
    n = math.sqrt(x * x + y * y + z * z) or 1.0
    s = math.sin(angle / 2) / n
    return (x * s, y * s, z * s, math.cos(angle / 2))


def quat_mul(a, b):
    ax, ay, az, aw = a
    bx, by, bz, bw = b
    return (aw * bx + ax * bw + ay * bz - az * by, aw * by - ax * bz + ay * bw + az * bx,
            aw * bz + ax * by - ay * bx + az * bw, aw * bw - ax * bx - ay * by - az * bz)


class Puppet:
    """The buddy's body: walks a circle, waves its right hand; suit / hero switched by keys."""

    def __init__(self):
        self.suit = -1          # -1: no suit (the hero's own look)
        self.hero = 0
        self.wave = True
        self.circle = True
        self.radius = 0.8
        self.angle = 0.0
        self.t = 0.0
        self.show = 0           # 0.0.74 MP PROPS: PROP_SHOWS[show]

    def next_suit(self):
        self.suit = self.suit + 1 if self.suit + 1 < len(SUITS) else -1
        if self.suit >= 0:
            self.hero = hero_of_suit(SUITS[self.suit])

    def next_hero(self):
        self.suit = -1
        self.hero = (self.hero + 1) % len(HEROES)

    def next_show(self):
        self.show = (self.show + 1) % len(PROP_SHOWS)

    def label(self):
        return SUITS[self.suit] if self.suit >= 0 else HEROES[self.hero] + ' (own look)'

    def step(self, dt):
        self.t += dt
        if self.circle:
            self.angle = (self.angle + dt * 0.6) % (2 * math.pi)  # about one lap in 10 s

    def state(self):
        t = self.t
        # The circle about the spot the game puts the buddy on; the body faces along the walk.
        cx, cz = math.sin(self.angle) * self.radius, -math.cos(self.angle) * self.radius + self.radius
        walking = self.circle
        yaw = (-self.angle - math.pi / 2) if walking else 0.0
        yaw = math.atan2(math.sin(yaw), math.cos(yaw))
        bob = 0.025 * math.sin(t * 7.0) if walking else 0.0
        head = ((0.0, 1.65 + bob, 0.0), quat_about((0, 1, 0), 0.25 * math.sin(t * 0.7)))
        swing = 0.25 * math.sin(t * 3.5) if walking else 0.0
        left = ((-0.28, 0.95, -swing * 0.5), quat_about((1, 0, 0), -0.4 + swing))
        if self.wave:
            w = math.sin(t * 6.0)
            right = ((0.38 + 0.08 * w, 1.78, -0.05), quat_mul(quat_about((0, 0, 1), -0.6 * w), quat_about((1, 0, 0), 1.4)))
        else:
            right = ((0.28, 0.95, swing * 0.5), quat_about((1, 0, 0), -0.4 - swing))
        actions = HAND_L | HAND_R | (SWINGING if self.circle and int(t) % 10 == 9 else 0)
        suit = self.suit if self.suit >= 0 else NO_SUIT
        return dict(hero=self.hero, suit=suit, actions=actions,
                    suit_hash=fnv1a(SUITS[self.suit]) if self.suit >= 0 else 0,
                    root=(cx, 0.0, cz), yaw=yaw, head=head, hands=(left, right),
                    curl=(0.6, 0.0 if self.wave else 0.4), eye=1.65,
                    props=fake_props(PROP_SHOWS[self.show], t, left[0], right[0]))


# 0.0.74 MP PROPS: what the puppet shows (P cycles; --props picks the first).
PROP_SHOWS = ['none', 'batarang', 'hammer', 'batons', 'shield', 'guns', 'claws', 'all']


def fake_props(show, t, left, right):
    """Fake props in the puppet's yawed root frame (the frame of its head and hands; src/mp_prop_look.hpp ids: 1/2
    held left/right, 3/4 flying, 5-8 worn, 9 the hammer, 10/11 batons). 'all' = shield left, hammer right, a
    batarang and a pistol worn."""
    if show == 'none':
        return None
    items, flags = [], 0
    ahead, up = (0.0, 0.0, -1.0), (0.0, 1.0, 0.0)
    held = F_IN_HAND
    if show == 'batarang':
        items.append(dict(id=2, kind=BATARANG, flags=held | F_SIDE, spin=0.0, at=right, forward=ahead, up=up))
    if show in ('batarang', 'all'):
        items.append(dict(id=5, kind=BATARANG, flags=1, at=(0.12, 1.0, 0.08), forward=(1, 0, 0), up=up))
    if show in ('hammer', 'all'):
        items.append(dict(id=9, kind=HAMMER, flags=held, spin=0.0, at=right, forward=(0, 1, 0), up=(0, 0, -1)))
    if show == 'batons':
        flags |= 2  # the club look (no model needed)
        for i, h in enumerate((left, right)):
            items.append(dict(id=10 + i, kind=BATON, flags=held | (F_SIDE if i else 0), spin=0.57, at=h,
                              forward=(0.0, 0.5, -0.866)))
    if show in ('shield', 'all'):
        items.append(dict(id=1, kind=SHIELD, flags=held, at=left, forward=ahead, up=up))
    if show == 'guns':
        items.append(dict(id=2, kind=PISTOL, flags=held | F_SIDE, at=right, forward=ahead, up=up))
    if show in ('guns', 'all'):
        items.append(dict(id=6, kind=PISTOL, flags=2, at=(-0.2, 0.95, 0.0), forward=(0, -1, 0), up=(1, 0, 0)))
    if show == 'claws':
        flags |= 1
        for i, h in enumerate((left, right)):
            items.append(dict(id=1 + i, kind=CLAWS, flags=held | (F_SIDE if i else 0), spin=1.0, at=h, forward=ahead, up=up))
    return dict(flags=flags, items=items[:ITEMS_MOST])


class Keys:
    """One key at a time, without Enter on Windows (msvcrt); a line of letters elsewhere."""

    def __init__(self, enabled):
        self.enabled = enabled and sys.stdin is not None
        try:
            import msvcrt  # noqa: F401
            self.msvcrt = msvcrt
        except ImportError:
            self.msvcrt = None

    def poll(self):
        if not self.enabled:
            return ''
        if self.msvcrt:
            keys = ''
            while self.msvcrt.kbhit():
                c = self.msvcrt.getwch()
                if c in ('\x00', '\xe0'):
                    self.msvcrt.getwch()
                    continue
                keys += c
            return keys.upper()
        try:
            if select.select([sys.stdin], [], [], 0)[0]:
                line = sys.stdin.readline()
                if line == '':
                    self.enabled = False
                return line.strip().upper()
        except (OSError, ValueError):
            self.enabled = False
        return ''


def now_ms(start):
    return int((time.monotonic() - start) * 1000) & 0xffffffff


def run(args, out=print):
    try:
        host = socket.gethostbyname(args.host)
    except OSError as e:
        out(f'Cannot find {args.host}: {e}')
        return 2
    game = (host, args.port)
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setblocking(False)
    try:
        sock.bind(('0.0.0.0' if host not in ('127.0.0.1',) else '127.0.0.1', 0))
    except OSError as e:
        out(f'Cannot open a UDP socket: {e}')
        return 2
    start = time.monotonic()
    puppet = Puppet()
    if args.suit:
        while puppet.suit + 1 < len(SUITS) and (puppet.suit < 0 or SUITS[puppet.suit] != args.suit):
            puppet.next_suit()
    keys = Keys(not args.no_keys)
    seq = 0
    connected = False
    last_hello = -10.0
    last_heard = 0.0
    last_print = 0.0
    next_send = 0.0
    ping_ms = None
    received = refused = game_poses = 0
    name = clean_name(args.name)
    out(f'HERO VR MP Test Buddy: saying hello to the game at {game[0]}:{game[1]} as {name!r}.')
    if getattr(args, 'props', 'none') in PROP_SHOWS:
        puppet.show = PROP_SHOWS.index(args.props)
    out('Keys: S next suit, H next hero, W wave, C circle, P props, Q quit (Enter after the letter outside Windows).')
    last = time.monotonic()
    code = 0
    try:
        while True:
            t = time.monotonic()
            dt, last = t - last, t
            if args.seconds and t - start >= args.seconds:
                break
            for k in keys.poll():
                if k == 'Q':
                    raise KeyboardInterrupt
                if k == 'S':
                    puppet.next_suit()
                    out(f'Suit: {puppet.label()}')
                elif k == 'H':
                    puppet.next_hero()
                    out(f'Hero: {puppet.label()}')
                elif k == 'W':
                    puppet.wave = not puppet.wave
                    out('Wave ' + ('on' if puppet.wave else 'off'))
                elif k == 'P':
                    puppet.next_show()
                    out(f'Props: {PROP_SHOWS[puppet.show]}')
                elif k == 'C':
                    puppet.circle = not puppet.circle
                    out('Circle ' + ('on' if puppet.circle else 'off'))
            if args.switch_every and puppet.t > 0 and int(puppet.t / args.switch_every) != int((puppet.t - dt) / args.switch_every):
                puppet.next_suit()
                out(f'Suit: {puppet.label()}')
            every = getattr(args, 'props_every', 0)
            if every and puppet.t > 0 and int(puppet.t / every) != int((puppet.t - dt) / every):
                puppet.next_show()
                out(f'Props: {PROP_SHOWS[puppet.show]}')
            # Receive.
            while True:
                try:
                    data, addr = sock.recvfrom(2048)
                except (BlockingIOError, InterruptedError):
                    break
                except ConnectionResetError:  # Windows: the game's port is closed (the game is not running)
                    break
                except OSError:
                    break
                try:
                    p = decode(data)
                except BadPacket as e:
                    refused += 1
                    if refused <= 5:
                        out(f'Refused a {len(data)}-byte packet: {e}')
                    continue
                if addr[0] != game[0] or addr[1] != game[1]:
                    continue
                received += 1
                last_heard = t
                if p['type'] == WELCOME and not connected:
                    connected = True
                    out(f"Connected: the game ({p['name'] or 'HERO VR'}) welcomed us. You should now see the buddy "
                        f"in front of you as PLAYER 2 (TEST BUDDY).")
                elif p['type'] == PING:
                    seq += 1
                    sock.sendto(encode(PONG, ROLE_BUDDY, seq, now_ms(start), struct.pack('<II', p['token'], p['sent_ms'])), game)
                elif p['type'] == PONG:
                    ping_ms = (now_ms(start) - p['ping_sent_ms']) & 0xffffffff
                elif p['type'] == POSE:
                    game_poses += 1
                elif p['type'] == BYE:
                    out('The game said bye (MP SOLO TEST turned off or the game closed). Waiting for it again.')
                    connected = False
            if connected and t - last_heard > 3.0:
                out('No word from the game for 3 s - saying hello again.')
                connected = False
            # Hello until welcomed; the puppet at 30 Hz once in.
            if not connected and t - last_hello >= 0.5:
                last_hello = t
                seq += 1
                try:
                    sock.sendto(encode(HELLO, ROLE_BUDDY, seq, now_ms(start), name_payload(name)), game)
                except OSError:
                    pass
            puppet.step(dt)
            if connected and t >= next_send:
                next_send = max(next_send + 1 / 30, t - 0.1)
                seq += 1
                try:
                    sock.sendto(encode(POSE, ROLE_BUDDY, seq, now_ms(start), pose_payload(puppet.state())), game)
                except OSError:
                    pass
                if int(t) % 2 == 0 and t - last_print >= 1.0:
                    seq += 1
                    try:
                        sock.sendto(encode(PING, ROLE_BUDDY, seq, now_ms(start), struct.pack('<I', seq)), game)
                    except OSError:
                        pass
            if t - last_print >= 1.0:
                last_print = t
                state = 'CONNECTED' if connected else 'waiting for the game'
                ping = f'{ping_ms} ms' if ping_ms is not None else '-'
                out(f'[{t - start:5.0f}s] {state}; ping {ping}; suit {puppet.label()}; '
                    f'packets in {received}, game poses {game_poses}, refused {refused}')
            time.sleep(0.004)
    except KeyboardInterrupt:
        out('Quitting.')
    finally:
        if connected:
            seq += 1
            try:
                sock.sendto(encode(BYE, ROLE_BUDDY, seq, now_ms(start)), game)
            except OSError:
                pass
        sock.close()
    out(f'Done: connected={connected}, packets in {received}, game poses {game_poses}, refused {refused}.')
    return code


def selftest():
    """Prints this side's encoding of the fixed packets (tests/mp_buddy_tests.py compares them with C++)."""
    pose = dict(hero=0, suit=3, actions=SWINGING | HAND_R, suit_hash=fnv1a('advanced_suit_2'),
                root=(0.5, 0.0, -0.25), yaw=0.5, head=((0, 1.6, 0), (0, 0, 0, 1)),
                hands=(((-0.25, 1.0, -0.25), (0, 0, 0, 1)), ((0.25, 1.75, -0.25), (0, 0.5, 0, 0.8660254))),
                curl=(1.0, 0.0), eye=1.65)
    print('pose', encode(POSE, ROLE_BUDDY, 42, 1000, pose_payload(pose)).hex())
    golden_props = dict(flags=2, items=[
        dict(id=9, kind=HAMMER, flags=F_FLYING, variant=2, spin=1.25, at=(0.25, 1.5, -2.0), forward=(0, 1, 0), up=(0, 0, -1)),
        dict(id=2, kind=BATARANG, flags=F_IN_HAND | F_SIDE, variant=3, spin=-0.5, at=(0.25, 1.75, -0.5), forward=(0, 0, -1),
             up=(1, 0, 0))])
    print('pose_props', encode(POSE, ROLE_BUDDY, 42, 1000, pose_payload(dict(pose, props=golden_props))).hex())
    print('hello', encode(HELLO, ROLE_BUDDY, 1, 7, name_payload('TEST BUDDY')).hex())
    print('bye', encode(BYE, ROLE_BUDDY, 3, 0).hex())
    print('fnv advanced_suit_2 %08x' % fnv1a('advanced_suit_2'))
    return 0


def main(argv=None):
    ap = argparse.ArgumentParser(description='HERO VR MP Test Buddy: a second player for MP SOLO TEST.')
    ap.add_argument('--host', default='127.0.0.1', help='the PC running the game (default: this one)')
    ap.add_argument('--port', type=int, default=DEFAULT_PORT)
    ap.add_argument('--name', default='TEST BUDDY')
    ap.add_argument('--suit', default='', help='start in this suit folder (e.g. Suit_thor_1)')
    ap.add_argument('--seconds', type=float, default=0, help='quit after this long (0: until Q)')
    ap.add_argument('--switch-every', type=float, default=0, help='change suit every N seconds by itself')
    ap.add_argument('--props', default='all', choices=PROP_SHOWS, help='0.0.74 MP PROPS: what the buddy holds (P cycles)')
    ap.add_argument('--props-every', type=float, default=0, help='change the props show every N seconds by itself')
    ap.add_argument('--no-keys', action='store_true')
    ap.add_argument('--selftest', action='store_true')
    args = ap.parse_args(argv)
    if args.selftest:
        return selftest()
    return run(args, out=lambda s: print(s, flush=True))


if __name__ == '__main__':
    sys.exit(main())
