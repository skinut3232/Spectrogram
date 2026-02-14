#pragma once

#include <juce_graphics/juce_graphics.h>
#include <algorithm>
#include <cmath>

class ColourMap
{
public:
    // Maps a normalised value (0..1) to a heat-map colour:
    // black → blue → cyan → yellow → red → white
    static juce::Colour heatMap(float t) noexcept
    {
        t = std::clamp(t, 0.0f, 1.0f);

        // 5-stop gradient
        if (t < 0.2f)
        {
            float s = t / 0.2f;
            return juce::Colour::fromFloatRGBA(0.0f, 0.0f, s, 1.0f);          // black → blue
        }
        if (t < 0.4f)
        {
            float s = (t - 0.2f) / 0.2f;
            return juce::Colour::fromFloatRGBA(0.0f, s, 1.0f, 1.0f);          // blue → cyan
        }
        if (t < 0.6f)
        {
            float s = (t - 0.4f) / 0.2f;
            return juce::Colour::fromFloatRGBA(s, 1.0f, 1.0f - s, 1.0f);      // cyan → yellow
        }
        if (t < 0.8f)
        {
            float s = (t - 0.6f) / 0.2f;
            return juce::Colour::fromFloatRGBA(1.0f, 1.0f - s, 0.0f, 1.0f);   // yellow → red
        }

        float s = (t - 0.8f) / 0.2f;
        return juce::Colour::fromFloatRGBA(1.0f, s, s, 1.0f);                  // red → white
    }

    // Maps a dB value to a colour using the given floor/ceiling range
    static juce::Colour fromDb(float db, float dbFloor, float dbCeiling) noexcept
    {
        float t = (db - dbFloor) / (dbCeiling - dbFloor);
        return heatMap(t);
    }
};
