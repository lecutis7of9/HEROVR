# HERO VR 0.0.76 HARLEYS: the two bikes the player picked (Made by Agent Mulder). Plain data, read by convert.py
# (Blender) and by tests/hero076_harleys_tests.py (plain Python). The credit is CC BY 4.0's: title, author, link,
# licence, and that the model was changed (converted, rescaled, split, decimated).
PRESETS = {
    'cap': dict(
        folder='Vehicle/Motorcycle/Cap',
        hero='CAPTAIN AMERICA',
        title='Harley Davidson Motorcycle "EL OHV V-TWIN"',
        artist='matias_romero',
        artist_url='https://sketchfab.com/matias_romero',
        url='https://sketchfab.com/3d-models/harley-davidson-motorcycle-el-ohv-v-twin-9923aaa519334f48b8dd44fd792475ed',
        uid='9923aaa519334f48b8dd44fd792475ed',
        source_faces=431558,
        # a 1936-47 Knucklehead EL: about 2.2 m long, its springer fork about 29 degrees from vertical (CANDIDATE:
        # the converter measures the fork and uses this only when the fork's line cannot be read)
        # the model is posed leaning on its side stand (about 10 degrees): stood upright on its rear axle first
        # the model names its parts: the tank (its dash and filler caps), the tank shift, the crash bar and the neck
        # sit near the fork but are the frame's
        length=2.22, rake_deg=29.0, upright=True,
        static_objects=r'_(Gas Tank|Gear Shift|Engine Guard|Chassis)_'),
    'wolverine': dict(
        folder='Vehicle/Motorcycle/Wolverine',
        hero='WOLVERINE',
        title='2016 Harley-Davidson FXDLS Dyna Low Rider S',
        artist='everhard',
        artist_url='https://sketchfab.com/everhard',
        url='https://sketchfab.com/3d-models/2016-harley-davidson-fxdls-dyna-low-rider-s-ad65bcb9282a46459a9a42f787bb5cbd',
        uid='ad65bcb9282a46459a9a42f787bb5cbd',
        source_faces=405975,
        # Harley-Davidson's FXDLS spec sheet: 2,335 mm long. The steer axis is the model's own: its fork tubes stand
        # 27 degrees back (the spec's 30.5 is the frame's; measured from the model's fork silhouette), the axle .095 m
        # ahead of the axis. The Low Rider S's dual gauges sit on the tank console, so they stay with the frame.
        length=2.335, rake_deg=27.0, axis_offset=.095,
        static_boxes=[((-.07, 0.0, .95), (.07, .24, 1.12))]),
}
LICENCE = 'CC BY 4.0'
LICENCE_URL = 'http://creativecommons.org/licenses/by/4.0/'
# The game's caps (include/spidy/mesh_model.hpp bikeTriangles / bikeBarsTriangles): over them the game refuses the model.
BODY_TRIANGLES = 110000
BARS_TRIANGLES = 40000
TEXTURE_PX = 1024


def credit_text(p):
    return ('"%s" (%s) by %s (%s) is licensed under %s (%s).\n'
            'Changed for HERO VR: converted to glTF binary, rescaled to %.2f m long, put in the game\'s vehicle frame, '
            'its handlebars and front fork split out as the steer part, parts that cannot be seen removed and the rest '
            'decimated to the game\'s triangle budget, textures resized to %d px.\n'
            % (p['title'], p['url'], p['artist'], p['artist_url'], LICENCE, LICENCE_URL, p['length'], TEXTURE_PX))
