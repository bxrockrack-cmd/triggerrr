#include "SampleBank.h"
#include <algorithm>

#if SNARE_HAS_SAMPLES
 #include <BinaryData.h>
#endif

namespace
{
    bool readSample (Sample& s, juce::AudioFormatReader& r)
    {
        const auto len = r.lengthInSamples;
        if (len <= 0 || len > (juce::int64) (r.sampleRate * 20.0))      // защита: не длиннее 20 секунд
            return false;

        const int ch = (int) juce::jmin (r.numChannels, 2u);
        s.data.setSize (ch, (int) len);
        r.read (&s.data, 0, (int) len, 0, true, ch > 1);
        s.sampleRate = r.sampleRate;
        s.peak = s.data.getMagnitude (0, (int) len);
        return s.peak > 0.0f;
    }
}

namespace SampleLoader
{
    juce::File userFolder()
    {
        return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                   .getChildFile ("SnareTrigger").getChildFile ("Samples");
    }

    std::unique_ptr<Bank> load()
    {
        auto bank = std::make_unique<Bank>();
        juce::AudioFormatManager fm;
        fm.registerBasicFormats();

        const auto folder = userFolder();
        folder.createDirectory();

        auto files = folder.findChildFiles (juce::File::findFiles, false, "*.wav;*.aif;*.aiff;*.flac");
        if (files.size() > 0)
        {
            for (auto& f : files)
            {
                std::unique_ptr<juce::AudioFormatReader> reader (fm.createReaderFor (f));
                Sample s;
                if (reader != nullptr && readSample (s, *reader))
                {
                    s.name = f.getFileName();
                    bank->samples.push_back (std::move (s));
                }
            }
            bank->source = folder.getFullPathName();
        }
#if SNARE_HAS_SAMPLES
        else
        {
            for (int i = 0; i < BinaryData::namedResourceListSize; ++i)
            {
                const char* resName = BinaryData::namedResourceList[i];
                int size = 0;
                const char* data = BinaryData::getNamedResource (resName, size);
                if (data == nullptr || size <= 0)
                    continue;

                std::unique_ptr<juce::AudioFormatReader> reader (
                    fm.createReaderFor (std::make_unique<juce::MemoryInputStream> (data, (size_t) size, false)));
                Sample s;
                if (reader != nullptr && readSample (s, *reader))
                {
                    s.name = BinaryData::getNamedResourceOriginalFilename (resName);
                    bank->samples.push_back (std::move (s));
                }
            }
            bank->source = "built-in";
        }
#endif

        std::sort (bank->samples.begin(), bank->samples.end(), [] (const Sample& a, const Sample& b)
        {
            if (a.peak != b.peak)
                return a.peak < b.peak;
            return a.name < b.name;
        });
        return bank;
    }
}
