"""Bounded test of native offscreen scene views; no OpenXR presentation yet."""
import argparse
import ctypes as c
import json
import math
import pathlib
import struct
import sys
import time
from capture_game_state import Game,find_game,open_process,close
from bridge_game import ROOT,prepare
from observe_game import call_remote,call_with_payload
from inspect_game import PE


def snapshot(game,address):
    for _ in range(8):
        raw=game.read(address,352);seq=game.read(address+16,8)
        if len(raw)!=352 or len(seq)!=8:return None
        if struct.unpack_from('<3I',raw)!=(0x53535042,4,352):raise RuntimeError('Stereo probe protocol mismatch')
        sequence=struct.unpack_from('<Q',raw,16)[0]
        if sequence&1 or sequence!=struct.unpack('<Q',seq)[0]:continue
        result=dict(zip(('frames','created','retired','primary'),struct.unpack_from('<4Q',raw,24)))
        result.update(sequence=sequence,state=struct.unpack_from('<I',raw,12)[0])
        result['thread'],result['error']=struct.unpack_from('<2I',raw,56)
        result['eyes']=[]
        for i in range(2):
            start=64+i*144
            eye=dict(zip(('view','texture','texture_object','updates','flags','width','height','occlusion'),
                struct.unpack_from('<4Q4I',raw,start)))
            eye['pose']=struct.unpack_from('<16f',raw,start+48)
            eye.update(zip(('readback','readback_bytes','readback_completed'),struct.unpack_from('<Q2I',raw,start+112)))
            eye.update(zip(('render_width','render_height','viewport_width','viewport_height'),struct.unpack_from('<4I',raw,start+128)))
            result['eyes'].append(eye)
        return result
    return None


def frame_snapshot(game,address):
    for _ in range(8):
        raw=game.read(address,368);seq=game.read(address+16,8)
        if len(raw)!=368 or len(seq)!=8:return None
        if struct.unpack_from('<3I',raw)!=(0x53455044,5,368):raise RuntimeError('Eye frame protocol mismatch')
        if struct.unpack_from('<Q',raw,16)[0]&1 or raw[16:24]!=seq:continue
        # history_moved/still: the eye's previous camera differed from, or equalled, the rendered one.
        # same/late_frame_poses: the rendered pose was placed in that frame, or in an earlier one.
        names=('accepted','latched','left_copies','right_copies','left_begins','right_begins','left_ends','right_ends',
               'left_copied','right_copied','left_begun','right_begun','left_ended','right_ended','left_job','right_job',
               'reclaimed','left_history_moved','right_history_moved','left_history_still','right_history_still',
               'left_same_frame_poses','right_same_frame_poses','left_late_frame_poses','right_late_frame_poses')
        # rope_from_eye: first point of the left hand's game rope, relative to the left eye being rendered.
        return dict(error=struct.unpack_from('<I',raw,12)[0],**dict(zip(names,struct.unpack_from('<25Q',raw,24))),
                    rope_frames=struct.unpack_from('<Q',raw,224)[0],rope_from_eye=struct.unpack_from('<3f',raw,232),
                    eye_camera=eye_camera(raw))
    return None


def eye_camera(raw):
    """Version 5 (Update 21): the per-eye camera audit (native_eye_frame.hpp auditEyeCamera). Per eye: job copies
    whose rendered camera (view +0x30) was the pose given SetPose that frame (as_asked), the frame before
    (as_last_asked: a frame late) or neither (elsewhere); whose previous-frame camera (view +0x560, the engine's
    motion/history matrices) was the eye's own pose of the frame before (previous_own) or not (previous_other);
    the largest misses in metres; and the rendered eyes' distance apart in metres (the IPD drawn)."""
    counts=struct.unpack_from('<11Q',raw,248)
    misses=struct.unpack_from('<4f',raw,336)
    sep,sep_min,sep_max=struct.unpack_from('<3f',raw,352)
    out={}
    for i,eye in enumerate(('left','right')):
        out[eye]=dict(as_asked=counts[0+i],as_last_asked=counts[2+i],elsewhere=counts[4+i],
                      previous_own=counts[6+i],previous_other=counts[8+i],
                      rendered_miss_m=round(misses[i],4),previous_miss_m=round(misses[2+i],4))
    out['separations']=counts[10]
    out['separation_m']=round(sep,4);out['separation_min_m']=round(sep_min,4);out['separation_max_m']=round(sep_max,4)
    return out


def eye_camera_line(cam):
    """The console's word on the eye camera audit (eye_camera), or None before any eye was copied."""
    if not cam:
        return None
    l, r = cam['left'], cam['right']
    copies = sum(l[k] + r[k] for k in ('as_asked', 'as_last_asked', 'elsewhere'))
    if not copies:
        return None
    late = l['as_last_asked'] + r['as_last_asked']
    other = l['elsewhere'] + r['elsewhere']
    own = l['previous_own'] + r['previous_own']
    prev = own + l['previous_other'] + r['previous_other']
    apart = (f"eyes {cam['separation_m']*1000:.1f} mm apart ({cam['separation_min_m']*1000:.1f}-"
             f"{cam['separation_max_m']*1000:.1f})" if cam['separations'] else 'eyes apart: not measured')
    return (f"Eye cameras: {copies - late - other} of {copies} eye renders from the pose given that frame, "
            f"{late} a frame late, {other} from elsewhere (largest miss "
            f"{max(l['rendered_miss_m'], r['rendered_miss_m'])*100:.1f} cm); previous-frame camera the eye's own "
            f"in {own} of {prev}; {apart}.")


RENDER_RINGS = {0: 'off', 1: 'not_created', 2: 'game_ring', 3: 'spidy_ring'}


def render_memory_snapshot(game, address):
    """The game's per-frame render memory (native_render_memory::Data), in megabytes.

    Every view's draw lists and render commands for a frame come from one ring, and two consecutive
    frames must fit in it. `overflow_frames` counts frames in which a request did not fit; the game
    leaves out a view's work in such a frame. The game's own ring is 128 MB; three scene views
    need Spidy's larger one (`ring` == 'spidy_ring'), which the game only gets when the module is
    loaded while it starts (tools/vr_launcher.py).
    """
    for _ in range(8):
        raw = game.read(address, 64)
        seq = game.read(address+16, 8)
        if len(raw) != 64 or len(seq) != 8:
            return None
        if struct.unpack_from('<3I', raw) != (0x53524d44, 1, 64):
            raise RuntimeError('Render memory protocol mismatch')
        if struct.unpack_from('<Q', raw, 16)[0] & 1 or raw[16:24] != seq:
            continue
        status, = struct.unpack_from('<I', raw, 12)
        frames, overflow = struct.unpack_from('<2Q', raw, 24)
        game_ring, ring, last, worst, pair, error = struct.unpack_from('<6I', raw, 40)
        return dict(ring=RENDER_RINGS.get(status, status), frames=frames, overflow_frames=overflow,
                    game_ring_mb=game_ring/1024, ring_mb=ring/1024, last_frame_mb=last/1024,
                    worst_frame_mb=worst/1024, worst_two_frames_mb=pair/1024, error=error)
    return None


MIP_BIAS_STATUS = {0: 'off', 1: 'waiting_for_device', 2: 'watching', 3: 'late', 4: 'unavailable'}


def encode_mip_bias(value, anisotropy=0):
    """run_game_vr's --mip-bias and --anisotropy for SpidyMipBiasStart (spidy/mip_bias.hpp encode): bias
    'game' only reports, else -2.0..+1.0; anisotropy 0 keeps the game's, else 1..16 for the samplers the
    game biases negatively."""
    anisotropy = int(anisotropy or 0)
    if not 0 <= anisotropy <= 16:
        raise ValueError('anisotropy must be 0..16')
    if value in (None, 'game'):
        return anisotropy << 16
    hundredths = round(float(value) * 100)
    if not -200 <= hundredths <= 100:
        raise ValueError('mip bias must be game or -2..1')
    return (1000 + hundredths) | anisotropy << 16


def mip_bias_snapshot(game, address):
    """The game's texture sampler mip bias as its D3D12 device saw it (spidy::mip_bias::Data).

    `most_negative` is the most negative bias the game gave any sampler; `changed_*` count the
    ones that got `bias` instead. A sampler with a bias of 0 or more is never changed.
    'late' means the module started after the game made its device, so nothing is seen.
    """
    raw = game.read(address, 152)
    if len(raw) != 152:
        return None
    if struct.unpack_from('<3I', raw) != (0x534d4244, 2, 152):
        raise RuntimeError('Mip bias protocol mismatch')
    status, change = struct.unpack_from('<2I', raw, 12)
    bias, most_negative = struct.unpack_from('<2f', raw, 20)
    error, = struct.unpack_from('<I', raw, 28)
    names = ('samplers', 'negative_samplers', 'changed_samplers', 'root_signatures', 'static_samplers',
             'negative_static_samplers', 'changed_root_signatures', 'embedded_root_signatures',
             'negative_embedded_samplers', 'failed_root_signatures', 'devices')
    counts = dict(zip(names, struct.unpack_from('<11Q', raw, 32)))
    # Version 2: the negatively biased (material) samplers' filtering, and how many got `anisotropy`.
    anisotropy, most_anisotropy = struct.unpack_from('<2I', raw, 120)
    filtering = dict(zip(('anisotropic_samplers', 'trilinear_samplers', 'raised_samplers'),
                         struct.unpack_from('<3Q', raw, 128)))
    return dict(status=MIP_BIAS_STATUS.get(status, status), bias=round(bias, 2) if change else 'game',
                most_negative=round(most_negative, 3), error=error, **counts,
                anisotropy=anisotropy or 'game', most_anisotropy=most_anisotropy, **filtering)


def motion_command(main,serial,seconds,travel=0.):
    # Exercise the same paired world-pose command used by tracked eyes. This is
    # a deterministic diagnostic motion, not a substitute for headset tracking.
    # `travel` slides both eyes along the camera's right axis, in metres.
    angle=math.radians(10)*math.sin(seconds*2);co,si=math.cos(angle),math.sin(angle)
    eyes=[]
    for i in range(2):
        pose=list(main)
        for j in range(3):
            pose[j]=co*main[j]+si*main[8+j]
            pose[8+j]=-si*main[j]+co*main[8+j]
            pose[12+j]=main[12+j]+travel*main[j]+(-.032 if i==0 else .032)*pose[j]
        eyes.extend(pose+[-math.pi/4,math.pi/4,-math.pi/4,math.pi/4])
    # Command v2 (native_eye_frame.hpp): no player anchor, so the pose is used as sent.
    return struct.pack('<4IQ2I40f3fI',0x53455043,2,208,1,serial,250,0,*eyes,0,0,0,0)


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--create-views',action='store_true')
    p.add_argument('--native-readback',action='store_true',help='Use native asynchronous texture readback (fresh views required)')
    p.add_argument('--display-pass',action='store_true',help='Run the native display conversion into each eye output target')
    p.add_argument('--motion-test',action='store_true',help='Exercise leased world poses and trace their native render jobs')
    p.add_argument('--seconds',type=float,default=4)
    p.add_argument('--size',type=int,default=512)
    p.add_argument('--output',type=pathlib.Path,default=ROOT/'reports/stereo-probe.json')
    a=p.parse_args()
    if not .5<=a.seconds<=10 or not 64<=a.size<=1024:p.error('Use .5..10 seconds and 64..1024 pixels')
    if a.native_readback and (not a.create_views or a.size&(a.size-1)):p.error('Native readback requires --create-views and a power-of-two size')
    if a.display_pass and not a.create_views:p.error('Display pass requires --create-views')
    game=Game(find_game());process=None;exports=None;stopped=False;samples=[];objects=[]
    try:
        pe=PE(pathlib.Path(game.path).read_bytes());rvas=(0x18a0bb0,0x189bd30,0x186cc00,0x1846c20,0x19223e0,0x189e310,0x189e3a0)
        code={r:pe.bytes(r,16) for r in rvas}
        if any(game.read(game.base+r,16)!=v for r,v in code.items()):raise RuntimeError('View lifecycle already patched')
        process=open_process(0x0400|0x0010|0x0020|0x0008|0x0002,False,game.pid)
        if not process:raise c.WinError(c.get_last_error())
        exports,digest=prepare(game.pid,process,ROOT/'build/windows-ninja/spidy_stereo_probe.dll',
            ROOT/'reports/stereo-modules',('SpidyStart','SpidyStop','SpidyStereoData','SpidySetEyes','SpidyStereoFrames'))
        flags=int(a.create_views)|(2 if a.native_readback else 0)|(4 if a.display_pass else 0)
        config=struct.pack('<4IQ4I',0x53534346,1,40,game.pid,game.base,int(a.seconds*1000),flags,a.size,a.size)
        result=call_with_payload(process,exports['SpidyStart'],config)
        if result:raise RuntimeError(f'Stereo probe start: {result}')
        print('Native offscreen view probe active.',flush=True)
        begin=time.monotonic();last=None;serial=0;main_pose=None;frame_samples=[]
        while (elapsed:=time.monotonic()-begin)<a.seconds+1:
            sample=snapshot(game,exports['SpidyStereoData'])
            if sample and sample['sequence']!=last:
                sample['seconds']=elapsed;samples.append(sample);last=sample['sequence']
                if a.motion_test and elapsed<a.seconds-.3 and sample['primary'] and not sample['retired']:
                    if main_pose is None:main_pose=struct.unpack('<16f',game.read(sample['primary'],64))
                    serial+=1
                    status=call_with_payload(process,exports['SpidySetEyes'],motion_command(main_pose,serial,elapsed))
                    if status and status!=4003:raise RuntimeError(f'Eye pose command rejected: {status}')
                f=frame_snapshot(game,exports['SpidyStereoFrames'])
                if f:frame_samples.append(dict(seconds=elapsed,**f))
                if not objects and sample['created'] and any(not(e['flags']&7) for e in sample['eyes']):
                    for eye in sample['eyes']:
                        obj=eye['texture_object']
                        if obj:
                            raw=game.read(obj,512)
                            vt=struct.unpack_from('<Q',raw)[0] if len(raw)>=8 else 0
                            objects.append(dict(object=hex(obj),bytes=raw.hex(),vtable=hex(vt),
                                vtable_bytes=game.read(vt,128).hex()))
            time.sleep(.025)
        status=call_remote(process,exports['SpidyStop']);stopped=status==0
        restored=all(game.read(game.base+r,16)==v for r,v in code.items())
        final=snapshot(game,exports['SpidyStereoData'])
        final_frames=frame_snapshot(game,exports['SpidyStereoFrames'])
        if a.native_readback and stopped and final:
            from capture_stereo import write_png
            for i,e in enumerate(final['eyes']):
                if not e['readback_completed'] or not e['readback']:
                    continue
                expected=e['width']*e['height']*4
                if e['readback_bytes']!=expected or expected>4*1024*1024:
                    raise RuntimeError('Unexpected native readback dimensions')
                # Views are excluded and the engine has had one second to drain
                # their jobs. Reject a buffer still changing during the read.
                pixels=game.read(e['readback'],expected)
                time.sleep(.05)
                if len(pixels)!=expected or pixels!=game.read(e['readback'],expected):
                    raise RuntimeError('Native readback is not stable after retirement')
                path=a.output.with_name(a.output.stem+f'-{i}.png')
                write_png(path,e['width'],e['height'],pixels)
                e['image']=str(path.resolve());e['unique_rgba']=len(set(struct.iter_unpack('<I',pixels)))
        result=dict(pid=game.pid,module_base=hex(game.base),dll_sha256=digest,create_views=a.create_views,
            stopped=stopped,entries_restored=restored,display_pass=a.display_pass,native_readback=a.native_readback,
            samples=samples,final=final,objects=objects,native_stereo_verified=False)
        result.update(motion_test=a.motion_test,frame_samples=frame_samples,final_frames=final_frames)
        a.output.parent.mkdir(parents=True,exist_ok=True)
        a.output.write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
        print(json.dumps(dict(stopped=stopped,restored=restored,final=final),indent=2))
        return 0 if final and final['frames'] and stopped and restored else 1
    finally:
        if process and exports and not stopped:call_remote(process,exports['SpidyStop'])
        if process:close(process)
        game.close()


if __name__=='__main__':
    try:sys.exit(main())
    except (OSError,ValueError,RuntimeError) as e:print(f'Stereo probe failed: {e}',file=sys.stderr);sys.exit(2)
