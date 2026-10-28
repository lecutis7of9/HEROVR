#pragma once
// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3; SpidyVR by Ilya Mezerowsky.
// Made by Agent Mulder.
//
// Version 0.0.73 test 2 BUTTON GUIDE (fix 23): each hero's own controls, shown on his ROSTER page
// (the CONTROLS tab beside SUITS). Every line is a control the mod already has (controls_guide.hpp,
// repulsor.hpp, hero072.hpp, hero007 and 0.0.73's universal grapple, extras385's FLY MODE poses);
// nothing here changes a control. The lines are short enough for one row each (the tests check).
#include "spidy/hero_modes.hpp"

#include <cstdint>

namespace spidy::hero_controls {
struct Line {
    const char* name;
    const char* how;
};
inline constexpr uint32_t linesMost = 9, nameMost = 14, howMost = 44;
struct Guide {
    uint32_t count;
    Line lines[linesMost];
};
// Every flyer's FLY MODE poses (extras385::fly::help).
#define SPIDY_FLY_LINES                                                                                                \
    {"FLY", "both hands forward; one hand up rises"}, {"HOVER / STOP", "arms down hover; a T-pose brakes"},          \
    {                                                                                                                  \
        "LAND", "click the right stick (R3)"                                                                           \
    }
inline constexpr Guide guides[hero_modes::heroes] = {
    // SPIDER-MAN
    {9,
     {{"WEB SWING", "grip + trigger, swing your arm"},
      {"ZIP", "right stick up, let go"},
      {"WEB BALL", "tap the trigger"},
      {"WEB GRABBER", "aim, hold the trigger, yank back"},
      {"SONIC BURST", "hands forward, close + both triggers"},
      {"WALL CRAWL", "grab the wall, pull hand over hand"},
      {"PUNCH", "punch (the game's X)"},
      {"DODGE / USE", "B dodges, Y interacts"},
      {"HOLOGRAM", "other hand to this wrist + trigger"}}},
    // IRON MAN
    {7,
     {{"REPULSOR", "pull a trigger: a blast from that palm"},
      {"CHEST BEAM", "pull both triggers together and hold"},
      SPIDY_FLY_LINES,
      {"SUPERSONIC", "both hands out, left stick forward"},
      {"PUNCH", "punch (the game's X)"}}},
    // DAREDEVIL
    {7,
     {{"BATONS", "reach over a shoulder and grip"},
      {"THROW", "tap the trigger; it chains and comes back"},
      {"GRAPPLE", "hold the trigger: a line from the baton"},
      {"STRIKE", "swing a baton or punch"},
      {"PARRY", "cross both batons in front of your face"},
      {"FAN", "twirl the batons fast (douses a fire)"},
      {"SONAR", "always on: foes glow in the dark"}}},
    // HULK
    {5,
     {{"BIG JUMP", "hold A on the ground, let go"},
      {"SMASH", "punch: normal foes go down in one"},
      {"UPPERCUT", "punch while the fist moves up fast"},
      {"CLAP", "clap your hands (douses a fire)"},
      {"RAGE", "take hits in a fight: harder punches"}}},
    // BATMAN
    {8,
     {{"BATARANG", "hold a trigger, let go to throw"},
      {"SMOKE", "grip at the front of the belt, throw"},
      {"GRAPPLE GUN", "grip away from the pouches, trigger"},
      {"CAPE GLIDE", "in the air, both arms out to the sides"},
      {"GLIDE TURN", "dip an arm; tuck in dives, raise climbs"},
      {"TAKEDOWN", "perched above a foe: press Y"},
      {"PUNCH", "punch (the game's X)"},
      {"DODGE / USE", "B dodges, Y interacts"}}},
    // WOLVERINE
    {5,
     {{"CLAWS", "grip: the claws slide out (SNIKT)"},
      {"SLASH", "swing with the claws out"},
      {"CLAW CLIMB", "claws out on a wall (CLAW CLIMB on)"},
      {"HEALING", "no hits for a few seconds: health back"},
      {"DODGE / USE", "B dodges, Y interacts"}}},
    // SUPERMAN
    {7,
     {{"HEAT VISION", "both triggers: beams along your gaze"},
      {"COLD BREATH", "a hand at the mouth + both triggers"},
      SPIDY_FLY_LINES,
      {"SUPERSONIC", "both hands out, left stick forward"},
      {"PUNCH", "punch: super strength"}}},
    // PUNISHER
    {8,
     {{"PISTOLS", "grip at a hip to draw, trigger fires"},
      {"SHOTGUN", "reach behind, grip, trigger fires"},
      {"RELOAD", "flick the gun, then touch the hip"},
      {"HOOK", "grip at the chest draws it, trigger fires"},
      {"HOLSTER", "let go of the grip at the holster"},
      {"VAN", "tap X (PUNISHER VAN on)"},
      {"PUNCH", "punch (the game's X)"},
      {"DODGE / USE", "B dodges, Y interacts"}}},
    // CAPTAIN AMERICA
    {8,
     {{"SHIELD", "reach over the shield shoulder, grip"},
      {"THROW", "backhand flick + the trigger"},
      {"BASH", "swing with the shield on the arm"},
      {"PUT AWAY", "reach back and let go of the grip"},
      {"SLAM", "drive the shield arm down fast"},
      {"GRAPPLE GUN", "other hand: grip, then the trigger"},
      {"SUPER SPRINT", "left stick pushed, pump your arms"},
      {"DODGE / USE", "B dodges, Y interacts"}}},
    // THOR
    {9,
     {{"HAMMER", "reach over a shoulder and grip"},
      {"THROW", "let go of the grip mid-swing"},
      {"RECALL", "grip again: it flies back to the hand"},
      {"LIGHTNING", "right trigger: a lightning blast"},
      {"STORM", "hold the hammer up overhead for 1 s"},
      {"SMASH", "down swing calls a bolt; up launches"},
      SPIDY_FLY_LINES}},
    // GREEN LANTERN
    {9,
     {{"TAKE OFF", "both arms straight up overhead"},
      {"FLY", "the hands steer (FLY MODE's poses)"},
      {"RING BEAM", "right trigger"},
      {"CHAIN", "left trigger: the chain construct"},
      {"FIST", "right grip + a fast punch"},
      {"BUBBLE", "both grips, pull the hands apart"},
      {"HOSE", "both grips, hands together out front"},
      {"CHARGE", "the ring charge is on the left wrist"},
      {"LAND", "click the right stick (R3)"}}},
};
#undef SPIDY_FLY_LINES
inline const Guide& guideOf(uint32_t hero) { return guides[hero < hero_modes::heroes ? hero : 0]; }
} // namespace spidy::hero_controls
