#pragma once
#include <array>
#include <atomic>
#include <juce_audio_processors/juce_audio_processors.h>
#include "Params.h"
#include "SampleBank.h"

class SnareTriggerAudioProcessor : public juce::AudioProcessor
{
public:
    SnareTriggerAudioProcessor();
    ~SnareTriggerAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "SnareTrigger"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 2.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // ---- для интерфейса ----
    void rescanSamples();                 // перечитать библиотеку (вызывать из потока интерфейса)
    void audition();                      // пробный удар случайной силы по кругу
    int  getNumSamples() const            { return numSamples.load(); }
    juce::String getSampleSource() const;
    juce::String getSampleName (int index) const;

    juce::AudioProcessorValueTreeState apvts;

    static constexpr int kCols = 520;     // колонок в окне волны (≈3 секунды)
    std::array<std::atomic<float>, kCols> waveCols;     // пик колонки (линейно)
    std::array<std::atomic<float>, kCols> hitCols;      // сила удара (0 = нет удара)
    std::atomic<int>   writeIdx { 0 };
    std::atomic<float> inPeak[2]  { { 0.0f }, { 0.0f } };
    std::atomic<float> outPeak[2] { { 0.0f }, { 0.0f } };
    std::atomic<float> thresholdDb { -33.0f };
    std::atomic<float> lastVelocity { 0.0f };
    std::atomic<int>   lastSampleIndex { -1 };

private:
    struct Biquad
    {
        float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
        void reset() { z1 = z2 = 0; }
        void setHighPass (double fs, double f)  { set (fs, f, true); }
        void setLowPass  (double fs, double f)  { set (fs, f, false); }
        float process (float x)
        {
            const float y = b0 * x + z1;
            z1 = b1 * x - a1 * y + z2;
            z2 = b2 * x - a2 * y;
            return y;
        }
    private:
        void set (double fs, double f, bool high)
        {
            const double w = 2.0 * juce::MathConstants<double>::pi * juce::jlimit (10.0, fs * 0.45, f) / fs;
            const double c = std::cos (w), s = std::sin (w), al = s / (2.0 * 0.7071);
            const double a0 = 1.0 + al;
            const double n0 = high ? (1.0 + c) * 0.5 : (1.0 - c) * 0.5;
            const double n1 = high ? -(1.0 + c)       :  (1.0 - c);
            b0 = (float) (n0 / a0); b1 = (float) (n1 / a0); b2 = (float) (n0 / a0);
            a1 = (float) (-2.0 * c / a0); a2 = (float) ((1.0 - al) / a0);
        }
    };

    struct Voice
    {
        const Sample* s = nullptr;
        double pos = 0.0, inc = 1.0;
        float gain = 1.0f;
        bool active = false;
        juce::uint64 age = 0;
    };

    void startVoice (const Bank& b, float velocity);

    mutable juce::CriticalSection bankLock;
    std::unique_ptr<Bank> bank;
    std::atomic<int> numSamples { 0 };
    std::atomic<int> auditionReq { -1 };
    int auditionStep = 0;

    static constexpr int kMaxVoices = 16;
    std::array<Voice, kMaxVoices> voices;
    juce::uint64 ageCounter = 0;

    double fs = 44100.0;
    int lookahead = 144;                           // задержка = окно поиска пика (~3 мс)
    std::vector<float> delayBuf[2];
    int delayPos = 0;

    Biquad hp, lp;
    float lastLowcut = -1.0f, lastHighcut = -1.0f;

    // детектор
    float env = 0.0f, supLevel = 0.0f;
    bool armed = true, pending = false;
    int pendCount = 0, retrigCount = 0;
    float pendPeak = 0.0f;

    // гейт: 0 = закрыт, 1 = атака, 2 = удержание, 3 = спад
    int gateState = 0, gateHoldCount = 0;
    float gateGain = 0.0f;

    // окно волны
    int colSamples = 1000, colCount = 0;
    float colPeak = 0.0f, colHit = 0.0f;

    float inGainPrev = 1.0f, outGainPrev = 1.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SnareTriggerAudioProcessor)
};
