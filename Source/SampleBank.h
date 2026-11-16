#pragma once
// Библиотека семплов: все WAV одной загружаются в память и сортируются по громкости.
// Чем сильнее удар в записи, тем более громкий семпл выбирается.
#include <juce_audio_formats/juce_audio_formats.h>
#include <memory>
#include <vector>

struct Sample
{
    juce::String name;
    juce::AudioBuffer<float> data;
    double sampleRate = 44100.0;
    float peak = 0.0f;
};

struct Bank
{
    std::vector<Sample> samples;     // по возрастанию громкости (peak)
    juce::String source;             // "built-in" или путь к папке пользователя

    // velocity 0..1 -> индекс семпла (всё диапазон поровну делится между семплами)
    int pickForVelocity (float v) const
    {
        const int n = (int) samples.size();
        if (n == 0)
            return -1;
        return juce::jlimit (0, n - 1, (int) (v * (float) n));
    }
};

namespace SampleLoader
{
    // Папка для замены семплов без пересборки: Documents/SnareTrigger/Samples
    juce::File userFolder();

    // Если в папке пользователя есть WAV/AIFF/FLAC — берёт их, иначе — вшитые в плагин.
    std::unique_ptr<Bank> load();
}
