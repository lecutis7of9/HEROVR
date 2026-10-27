"""Version 0.0.7 CAPTAIN AMERICA: the shield's flight and chain, drawn from the same rules the
mod runs (hero_modes::Baton, hero_modes.hpp: 22 m/s out homing on the aim pick, a hit ricochets off him
for 0.25 s, then the nearest enemy within 5 m not hit yet, at most 2 bounces = 3 enemies; then home at
18 m/s chasing the hand; 4 s at most). Top view plus an edge-on inset (the flat spin).

  cap_flight_diagram.py <out.png>"""
import sys
import numpy as np
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt

SPEED, BACK, HIT, CATCH, RICO, RANGE, CHAINS = 22., 18., .45, .3, .25, 5., 2


def simulate_mod(hand, enemies, dt=1 / 90):
    """Exactly the mod's order: on a hit, chain at once to the next (no ricochet pause); the last hit
    ricochets off him 0.25 s and then flies home."""
    pos, path, hits = hand.copy(), [hand.copy()], []
    target, state, t, bounce, vel = 0, 'flying', 0., 0., np.zeros(2)
    while t < 4:
        t += dt
        if state == 'flying':
            to = enemies[target] - pos
            d = np.linalg.norm(to)
            if d <= HIT + SPEED * dt:
                pos = enemies[target].copy()
                hits.append(target)
                cand = [(np.linalg.norm(e - pos), k) for k, e in enumerate(enemies) if k not in hits]
                cand = [c for c in cand if c[0] <= RANGE]
                if cand and len(hits) <= CHAINS:
                    target = min(cand)[1]
                else:
                    state, bounce, vel = 'ricochet', 0., -to / d * 6.
            else:
                pos = pos + to / d * SPEED * dt
        elif state == 'ricochet':
            bounce += dt
            pos = pos + vel * dt
            if bounce >= RICO:
                state = 'returning'
        if state == 'returning':
            to = hand - pos
            d = np.linalg.norm(to)
            if d <= CATCH + BACK * dt:
                path.append(hand.copy())
                return np.array(path), hits, t
            pos = pos + to / d * BACK * dt
        path.append(pos.copy())
    return np.array(path), hits, t


def main(out):
    hand = np.array([-.3, 0.])
    enemies = np.array([[-3.5, 8.], [.2, 10.6], [3.6, 8.6], [8.5, 13.]])
    path, hits, secs = simulate_mod(hand, enemies)
    fig = plt.figure(figsize=(15, 8.2), facecolor='#1e1e24')
    ax = fig.add_axes([.03, .07, .52, .86], facecolor='#24242c')
    ax.set_aspect('equal')
    ax.plot(path[:, 0], path[:, 1], color='#7fc8ff', lw=2.2, label='shield path (simulated with the mod\'s Baton rules)')
    for k in range(0, len(path) - 6, 6):
        a, b = path[k], path[k + 6]
        if np.linalg.norm(b - a) > .05:
            ax.annotate('', b, a, arrowprops=dict(arrowstyle='->', color='#7fc8ff', lw=1.2))
    ax.plot(*hand, 'o', ms=13, color='#e0c070')
    ax.text(hand[0] + .4, hand[1] - .7, 'YOU (the shield arm\'s hand):\nthrow = trigger, caught back here', color='#e0c070', fontsize=9)
    for k, e in enumerate(enemies):
        hit = k in hits
        ax.plot(*e, 's', ms=14, color='#e05050' if hit else '#777')
        ax.text(e[0] + .45, e[1] + .3, ('HIT %d' % (hits.index(k) + 1)) if hit else 'not hit (over 5 m from\nthe last one hit)',
                color='#ff8080' if hit else '#aaa', fontsize=9)
        if hit:
            ax.add_patch(plt.Circle(e, RANGE, fill=False, ls=':', color='#e05050', alpha=.35))
    ax.text(-8.5, 15.6, 'TOP VIEW (metres)', color='#9fd7ff', fontsize=11, weight='bold')
    notes = ['1  THROW: the trigger with the shield on the arm. A fast flat',
             '   sweep (the backhand) sends it along the sweep; otherwise',
             '   the aim pick (35 deg cone, 25 m) picks the enemy. 22 m/s.',
             '2  EACH HIT: a blow on sm2_damage (SHIELD DAMAGE, default',
             '   35 HP), the ricochet sound, a buzz in the throwing hand.',
             '3  CHAIN (SHIELD CHAIN ON): straight on to the nearest enemy',
             '   within 5 m not hit yet; 2 bounces, 3 enemies at most.',
             '4  RETURN: off the last one (0.25 s) and home at 18 m/s,',
             '   chasing the hand; the catch sound and a buzz.',
             '   Never lost: back on the arm after 4 s whatever happens.',
             '   One shield: no second throw while it is out.']
    for k, n in enumerate(notes):
        fig.text(.575, .9 - k * .03, n, color='#e8e8e8', fontsize=9, family='monospace')
    fig.text(.575, .55, 'this throw: %d enemies hit, back in %.2f s' % (len(hits), secs), color='#ffd25a', fontsize=10)
    ax.set_xlim(-9, 12)
    ax.set_ylim(-2, 16.5)
    ax.tick_params(colors='#999')
    for s in ax.spines.values():
        s.set_color('#555')
    ax.legend(loc='lower right', facecolor='#2a2a33', labelcolor='#ddd', fontsize=8.5)
    # edge-on: the flat spin
    bx = fig.add_axes([.575, .07, .4, .4], facecolor='#24242c')
    th = np.linspace(0, 2 * np.pi, 200)
    for k, x0 in enumerate([0, 2.2, 4.4]):
        bx.fill(x0 + .61 / 2 * np.cos(th), .035 * np.sin(th), color=['#c33', '#ddd', '#36c'][k], alpha=.95)
        bx.plot(x0 + .42 * np.cos(th), .09 * np.sin(th) + .16, color='#7fc8ff', lw=1)
        bx.annotate('', (x0 + .1, .16 - .088), (x0 - .1, .16 - .088), arrowprops=dict(arrowstyle='->', color='#7fc8ff', lw=1.4))
    bx.annotate('', (6.2, -.12), (-.8, -.12), arrowprops=dict(arrowstyle='->', color='#ffd25a', lw=1.6))
    bx.text(5.4, -.2, 'flight', color='#ffd25a', fontsize=8.5)
    bx.text(-.8, -.62, 'EDGE-ON: it flies FLAT, face up, spinning about its face normal\n(drawn at 8 turns a second; the real throw: about 10, ~25 m/s:\ngyroscopic, so it stays flat - SciFunLab / WIRED 2016)',
            color='#e8e8e8', fontsize=8.3)
    bx.set_xlim(-1, 6.5)
    bx.set_ylim(-.7, .45)
    bx.axis('off')
    fig.suptitle('CAPTAIN AMERICA: THE SHIELD\'S FLIGHT AND CHAIN (default SHIELD CHAIN ON, SHIELD DAMAGE 35 HP)', color='#9fd7ff', fontsize=12, x=.04, ha='left', y=.985)
    fig.savefig(out, dpi=100, facecolor=fig.get_facecolor())


if __name__ == '__main__':
    main(sys.argv[1])
