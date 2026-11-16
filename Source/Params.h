#pragma once
// Параметры плагина (ровно те органы управления, что нарисованы на макете).
#include <juce_audio_processors/juce_audio_processors.h>

namespace ids
{
    inline constexpr const char* gateOn      = "gateOn";
    inline constexpr const char* attack      = "attack";
    inline constexpr const char* hold        = "hold";
    inline constexpr const char* release     = "release";
    inline constexpr const char* inputVol    = "inputVol";
    inline constexpr const char* lowcut      = "lowcut";
    inline constexpr const char* highcut     = "highcut";
    inline constexpr const char* suppress    = "suppress";
    inline constexpr const char* sensitivity = "sensitivity";
    inline constexpr const char* retrigger   = "retrigger";
    inline constexpr const char* detail      = "detail";
    inline constexpr const char* outputVol   = "outputVol";
    inline constexpr const char* mix         = "mix";
}

inline juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    using P = juce::AudioParameterFloat;
    using R = juce::NormalisableRange<float>;
    juce::AudioProcessorValueTreeState::ParameterLayout l;

    l.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { ids::gateOn, 1 }, "Gate", true));
    l.add (std::make_unique<P> (juce::ParameterID { ids::attack, 1 },      "Attack",      R (0.1f, 50.0f, 0.01f, 0.4f), 0.5f));
    l.add (std::make_unique<P> (juce::ParameterID { ids::hold, 1 },        "Hold",        R (0.0f, 500.0f, 0.1f, 0.4f), 4.0f));
    l.add (std::make_unique<P> (juce::ParameterID { ids::release, 1 },     "Release",     R (5.0f, 2000.0f, 1.0f, 0.4f), 450.0f));
    l.add (std::make_unique<P> (juce::ParameterID { ids::inputVol, 1 },    "Input Volume",  R (-24.0f, 24.0f, 0.01f), 0.0f));
    l.add (std::make_unique<P> (juce::ParameterID { ids::lowcut, 1 },      "Lowcut",      R (0.0f, 2000.0f, 1.0f, 0.4f), 0.0f));
    l.add (std::make_unique<P> (juce::ParameterID { ids::highcut, 1 },     "Highcut",     R (1000.0f, 20000.0f, 1.0f, 0.4f), 20000.0f));
    l.add (std::make_unique<P> (juce::ParameterID { ids::suppress, 1 },    "Suppress",    R (0.0f, 100.0f, 1.0f), 0.0f));
    l.add (std::make_unique<P> (juce::ParameterID { ids::sensitivity, 1 }, "Sensitivity", R (0.0f, 100.0f, 1.0f), 51.0f));
    l.add (std::make_unique<P> (juce::ParameterID { ids::retrigger, 1 },   "Retrigger",   R (1.0f, 250.0f, 1.0f, 0.5f), 16.0f));
    l.add (std::make_unique<P> (juce::ParameterID { ids::detail, 1 },      "Detail",      R (0.0f, 100.0f, 1.0f), 14.0f));
    l.add (std::make_unique<P> (juce::ParameterID { ids::outputVol, 1 },   "Output Volume", R (-24.0f, 12.0f, 0.01f), 0.0f));
    l.add (std::make_unique<P> (juce::ParameterID { ids::mix, 1 },         "Mix",         R (0.0f, 100.0f, 1.0f), 100.0f));
    return l;
}
