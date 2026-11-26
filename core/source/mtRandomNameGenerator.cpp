#include "Platform.h"
#include "string.h"
#include "util_math.h"
#include <mtRandomNameGenerator.h>

// Short word lists (the original had two lists of about a thousand words each, 15 KB of flash).
static const char* const kWords1[] = { "dusty", "tape", "warm", "slow", "lost", "quiet", "paper", "soft",
                                       "night", "amber", "hollow", "faded", "grain", "loose", "velvet", "static",
                                       "pale", "rusty", "silver", "sleepy", "golden", "woolly", "distant", "crooked" };
static const char* const kWords2[] = { "loop", "reel", "hiss", "drift", "echo", "tide", "moth", "spool",
                                       "bloom", "fog", "ember", "wire", "rain", "dust", "kite", "river",
                                       "attic", "cellar", "lantern", "orchard", "comet", "pebble", "ribbon", "harbor" };

mtRandomNameGenerator randomNameGenerator;

const char* mtRandomNameGenerator::getRandomName(Timer* timer)
{
    utilMathRandom::randomSeed(timer->micros());
    const uint16_t rand1 = utilMathRandom::random(sizeof(kWords1) / sizeof(kWords1[0]) - 1);
    const uint16_t rand2 = utilMathRandom::random(sizeof(kWords2) / sizeof(kWords2[0]) - 1);
    snprintf(randomName, sizeof(randomName), "%s %s", kWords1[rand1], kWords2[rand2]);
    return randomName;
}
