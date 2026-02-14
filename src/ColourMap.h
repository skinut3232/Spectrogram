#pragma once

#include <juce_graphics/juce_graphics.h>
#include <algorithm>
#include <cmath>

class ColourMap
{
public:
    enum class Type { heat, magma, inferno, grayscale, rainbow };

    static constexpr int numTypes = 5;

    static const char* getName(Type type) noexcept
    {
        switch (type)
        {
            case Type::heat:      return "Heat";
            case Type::magma:     return "Magma";
            case Type::inferno:   return "Inferno";
            case Type::grayscale: return "Grayscale";
            case Type::rainbow:   return "Rainbow";
        }
        return "Heat";
    }

    static juce::Colour map(Type type, float t) noexcept
    {
        t = std::clamp(t, 0.0f, 1.0f);
        switch (type)
        {
            case Type::heat:      return heatMap(t);
            case Type::magma:     return magmaMap(t);
            case Type::inferno:   return infernoMap(t);
            case Type::grayscale: return grayscaleMap(t);
            case Type::rainbow:   return rainbowMap(t);
        }
        return heatMap(t);
    }

    static juce::Colour fromDb(Type type, float db, float dbFloor, float dbCeiling) noexcept
    {
        float t = (db - dbFloor) / (dbCeiling - dbFloor);
        return map(type, t);
    }

    // Keep legacy API working
    static juce::Colour heatMap(float t) noexcept
    {
        t = std::clamp(t, 0.0f, 1.0f);

        if (t < 0.2f)
        {
            float s = t / 0.2f;
            return juce::Colour::fromFloatRGBA(0.0f, 0.0f, s, 1.0f);
        }
        if (t < 0.4f)
        {
            float s = (t - 0.2f) / 0.2f;
            return juce::Colour::fromFloatRGBA(0.0f, s, 1.0f, 1.0f);
        }
        if (t < 0.6f)
        {
            float s = (t - 0.4f) / 0.2f;
            return juce::Colour::fromFloatRGBA(s, 1.0f, 1.0f - s, 1.0f);
        }
        if (t < 0.8f)
        {
            float s = (t - 0.6f) / 0.2f;
            return juce::Colour::fromFloatRGBA(1.0f, 1.0f - s, 0.0f, 1.0f);
        }

        float s = (t - 0.8f) / 0.2f;
        return juce::Colour::fromFloatRGBA(1.0f, s, s, 1.0f);
    }

    static juce::Colour fromDb(float db, float dbFloor, float dbCeiling) noexcept
    {
        float t = (db - dbFloor) / (dbCeiling - dbFloor);
        return heatMap(t);
    }

private:
    static juce::Colour lerp3(float t, float r0, float g0, float b0,
                               float r1, float g1, float b1) noexcept
    {
        return juce::Colour::fromFloatRGBA(r0 + t * (r1 - r0),
                                            g0 + t * (g1 - g0),
                                            b0 + t * (b1 - b0), 1.0f);
    }

    // Magma: black → dark purple → magenta → orange → pale yellow
    static juce::Colour magmaMap(float t) noexcept
    {
        t = std::clamp(t, 0.0f, 1.0f);
        if (t < 0.25f)
            return lerp3(t / 0.25f,           0.0f,  0.0f,  0.02f,  0.27f, 0.0f,  0.33f);
        if (t < 0.5f)
            return lerp3((t - 0.25f) / 0.25f, 0.27f, 0.0f,  0.33f,  0.73f, 0.21f, 0.47f);
        if (t < 0.75f)
            return lerp3((t - 0.5f) / 0.25f,  0.73f, 0.21f, 0.47f,  0.99f, 0.57f, 0.25f);
        return lerp3((t - 0.75f) / 0.25f,     0.99f, 0.57f, 0.25f,  0.99f, 0.99f, 0.75f);
    }

    // Inferno: black → dark purple → red-orange → yellow → pale yellow
    static juce::Colour infernoMap(float t) noexcept
    {
        t = std::clamp(t, 0.0f, 1.0f);
        if (t < 0.25f)
            return lerp3(t / 0.25f,           0.0f,  0.0f,  0.02f,  0.34f, 0.06f, 0.38f);
        if (t < 0.5f)
            return lerp3((t - 0.25f) / 0.25f, 0.34f, 0.06f, 0.38f,  0.85f, 0.21f, 0.16f);
        if (t < 0.75f)
            return lerp3((t - 0.5f) / 0.25f,  0.85f, 0.21f, 0.16f,  0.99f, 0.64f, 0.03f);
        return lerp3((t - 0.75f) / 0.25f,     0.99f, 0.64f, 0.03f,  0.98f, 0.99f, 0.64f);
    }

    // Grayscale: black → white
    static juce::Colour grayscaleMap(float t) noexcept
    {
        t = std::clamp(t, 0.0f, 1.0f);
        return juce::Colour::fromFloatRGBA(t, t, t, 1.0f);
    }

    // Classic rainbow: red → orange → yellow → green → cyan → blue → violet
    static juce::Colour rainbowMap(float t) noexcept
    {
        t = std::clamp(t, 0.0f, 1.0f);
        // Use HSV with hue cycling from 270 (violet) at 0 to 0 (red) at 1
        float hue = (1.0f - t) * 0.75f; // 0.75 = 270/360
        return juce::Colour::fromHSV(hue, 1.0f, t > 0.01f ? 1.0f : 0.0f, 1.0f);
    }
};
