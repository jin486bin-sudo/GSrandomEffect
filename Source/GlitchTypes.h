#pragma once
#include <cstdint>

enum class GlitchType {
    Stutter = 0,  // loop a short portion of the beat
    Slice,        // rearrange beat slices randomly
    Bitcrush,     // bit depth + sample rate reduction
    Stretch,      // slow-motion playback from capture
    Reverse,      // reverse playback of captured beat
    Gate,         // rhythmic chop / mute pattern
    TapeStop,     // gradual pitch-drop slowdown
    Scratch,      // back-and-forth pendulum playback
    Echo,         // delay+feedback from capture buffer
    RingMod,      // ring modulation with sine carrier
    Granular,     // scattered random grain playback
    TapeWarp,     // sinusoidal pitch-wobble (tape wow)
    Count
};

static constexpr const char* glitchTypeNames[] = {
    "STUTTER", "SLICE", "BITCRUSH", "STRETCH", "REVERSE",
    "GATE", "TAPESTOP", "SCRATCH", "ECHO", "RINGMOD",
    "GRANULAR", "TAPEWARP"
};

static constexpr const char* glitchTypeShort[] = {
    "STTR", "SLCE", "CRSH", "STCH", "RVRS",
    "GATE", "STOP", "SCRT", "ECHO", "RING",
    "GRAN", "WARP"
};

enum class StepLabel { X = 0, A, B, C, D };
static constexpr int NUM_STEPS  = 16;
static constexpr int NUM_LABELS = 4;

struct GlitchState {
    GlitchType type      = GlitchType::Stutter;
    float      intensity = 0.5f;
    uint32_t   seed      = 0;
};
