#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

class TriggerPanel;

// Окно плагина: проектный размер 1070x746 (как на макете), масштабируется целиком.
class SnareTriggerAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit SnareTriggerAudioProcessorEditor (SnareTriggerAudioProcessor&);
    ~SnareTriggerAudioProcessorEditor() override;

    void resized() override;

    static constexpr int kDesignW = 1070;
    static constexpr int kDesignH = 746;

private:
    class LnF;
    std::unique_ptr<LnF> lnf;
    std::unique_ptr<TriggerPanel> panel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SnareTriggerAudioProcessorEditor)
};
