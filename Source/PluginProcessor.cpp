#include "PluginProcessor.h"
#include "PluginEditor.h"

SnareTriggerAudioProcessor::SnareTriggerAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createParameterLayout())
{
    for (auto& c : waveCols) c.store (0.0f);
    for (auto& c : hitCols)  c.store (0.0f);
    rescanSamples();
}

// ---------------------------------------------------------------- семплы
void SnareTriggerAudioProcessor::rescanSamples()
{
    auto fresh = SampleLoader::load();                    // чтение файлов — вне блокировки
    const int n = (int) fresh->samples.size();
    {
        const juce::ScopedLock sl (bankLock);
        for (auto& v : voices) v.active = false;
        bank = std::move (fresh);
    }
    lastSampleIndex.store (-1);
    numSamples.store (n);
}

juce::String SnareTriggerAudioProcessor::getSampleSource() const
{
    const juce::ScopedLock sl (bankLock);
    return bank != nullptr ? bank->source : juce::String();
}

juce::String SnareTriggerAudioProcessor::getSampleName (int index) const
{
    const juce::ScopedLock sl (bankLock);
    if (bank == nullptr || index < 0 || index >= (int) bank->samples.size())
        return {};
    return bank->samples[(size_t) index].name;
}

void SnareTriggerAudioProcessor::audition()
{
    static const float steps[] = { 0.15f, 0.40f, 0.65f, 0.90f };
    auditionReq.store ((int) (steps[auditionStep++ % 4] * 1000.0f));
}

void SnareTriggerAudioProcessor::startVoice (const Bank& b, float velocity)
{
    const int idx = b.pickForVelocity (velocity);
    if (idx < 0)
        return;

    Voice* target = nullptr;
    for (auto& v : voices)
        if (! v.active) { target = &v; break; }
    if (target == nullptr)                                // все заняты — забираем самый старый
    {
        target = &voices[0];
        for (auto& v : voices)
            if (v.age < target->age) target = &v;
    }

    const Sample& s = b.samples[(size_t) idx];
    target->s = &s;
    target->pos = 0.0;
    target->inc = s.sampleRate / fs;
    target->gain = 1.0f;
    target->active = true;
    target->age = ++ageCounter;

    lastVelocity.store (velocity);
    lastSampleIndex.store (idx);
}

// ---------------------------------------------------------------- подготовка
bool SnareTriggerAudioProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto& out = l.getMainOutputChannelSet();
    return (out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo())
           && out == l.getMainInputChannelSet();
}

void SnareTriggerAudioProcessor::prepareToPlay (double sampleRate, int)
{
    fs = sampleRate;
    lookahead = juce::jmax (1, (int) (0.003 * fs));
    setLatencySamples (lookahead);

    for (auto& d : delayBuf) d.assign ((size_t) lookahead, 0.0f);
    delayPos = 0;

    hp.reset(); lp.reset();
    lastLowcut = lastHighcut = -1.0f;
    env = supLevel = 0.0f;
    armed = true; pending = false; pendCount = retrigCount = 0; pendPeak = 0.0f;
    gateState = 0; gateGain = 0.0f; gateHoldCount = 0;
    colSamples = juce::jmax (1, (int) (fs * 3.0 / (double) kCols));
    colCount = 0; colPeak = colHit = 0.0f;
    for (auto& v : voices) v.active = false;
}

// ---------------------------------------------------------------- обработка
void SnareTriggerAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numCh = juce::jmin (buffer.getNumChannels(), 2);
    const int n = buffer.getNumSamples();
    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, n);
    if (numCh == 0 || n == 0)
        return;

    // ---- параметры блока ----
    auto P = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };
    const bool  gateOn      = P (ids::gateOn) > 0.5f;
    const float attackMs    = P (ids::attack);
    const float holdMs      = P (ids::hold);
    const float releaseMs   = P (ids::release);
    const float inGain      = juce::Decibels::decibelsToGain (P (ids::inputVol));
    const float lowcut      = P (ids::lowcut);
    const float highcut     = P (ids::highcut);
    const float suppress    = P (ids::suppress) * 0.01f;
    const float sens        = P (ids::sensitivity);
    const float retrigMs    = P (ids::retrigger);
    const float detail      = P (ids::detail) * 0.01f;
    const float outGain     = juce::Decibels::decibelsToGain (P (ids::outputVol));
    const float mix         = P (ids::mix) * 0.01f;

    if (lowcut != lastLowcut)   { hp.setHighPass (fs, lowcut);   lastLowcut = lowcut; }
    if (highcut != lastHighcut) { lp.setLowPass (fs, highcut);   lastHighcut = highcut; }
    const bool hpOn = lowcut > 20.0f;
    const bool lpOn = highcut < 19000.0f;

    const float thrDb    = juce::jmap (sens, 0.0f, 100.0f, -6.0f, -60.0f);
    const float thrLin   = juce::Decibels::decibelsToGain (thrDb);
    thresholdDb.store (thrDb);
    const float rearm    = juce::jmap (detail, 0.35f, 0.80f);              // выше Detail -> раньше готов к новому удару
    const float relMs    = juce::jmap (detail, 40.0f, 4.0f);               // и быстрее отпускает детектор
    const float relCoef  = std::exp (-1.0f / (relMs * 0.001f * (float) fs));
    const float supCoef  = std::exp (-1.0f / (0.150f * (float) fs));
    const int   retrigS  = (int) (retrigMs * 0.001f * (float) fs);
    const float atkStep  = 1.0f / juce::jmax (1.0f, attackMs * 0.001f * (float) fs);
    const int   holdS    = (int) (holdMs * 0.001f * (float) fs);
    const float relStep  = 1.0f / juce::jmax (1.0f, releaseMs * 0.001f * (float) fs);

    float* ch[2] = { buffer.getWritePointer (0), numCh > 1 ? buffer.getWritePointer (1) : nullptr };
    float blkInPeak[2] = { 0, 0 }, blkOutPeak[2] = { 0, 0 };

    // семплы читаем под блокировкой; если интерфейс сейчас перечитывает библиотеку — блок идёт без семплов
    const juce::ScopedTryLock lock (bankLock);
    const Bank* bnk = (lock.isLocked() && bank != nullptr && ! bank->samples.empty()) ? bank.get() : nullptr;

    const int req = auditionReq.exchange (-1);
    if (req >= 0 && bnk != nullptr)
        startVoice (*bnk, (float) req * 0.001f);

    for (int i = 0; i < n; ++i)
    {
        const float t = (float) i / (float) n;
        const float ig = inGainPrev  + (inGain  - inGainPrev)  * t;
        const float og = outGainPrev + (outGain - outGainPrev) * t;

        float x[2] = { ch[0][i] * ig, ch[1] != nullptr ? ch[1][i] * ig : 0.0f };
        const float sum = numCh > 1 ? 0.5f * (x[0] + x[1]) : x[0];
        float peakRaw = std::abs (x[0]);
        if (numCh > 1) peakRaw = juce::jmax (peakRaw, std::abs (x[1]));
        for (int c = 0; c < numCh; ++c) blkInPeak[c] = juce::jmax (blkInPeak[c], std::abs (x[c]));

        // ---- детектор ----
        float f = sum;
        if (hpOn) f = hp.process (f);
        if (lpOn) f = lp.process (f);
        const float a = std::abs (f);
        env = a > env ? a : a + (env - a) * relCoef;
        supLevel *= supCoef;
        const float thr = thrLin + supLevel;

        if (pending)
        {
            pendPeak = juce::jmax (pendPeak, peakRaw);
            if (--pendCount <= 0)
            {
                pending = false;
                retrigCount = retrigS;
                const float pkDb = juce::Decibels::gainToDecibels (pendPeak, -100.0f);
                const float vel = juce::jlimit (0.0f, 1.0f, (pkDb - thrDb) / juce::jmax (6.0f, -3.0f - thrDb));
                supLevel = pendPeak * suppress * 0.6f;
                colHit = juce::jmax (colHit, 0.05f + 0.95f * vel);
                if (bnk != nullptr) startVoice (*bnk, vel);
                gateState = 1;                                   // открыть гейт оригинала
            }
        }
        else if (armed && retrigCount <= 0 && env > thr)
        {
            pending = true;
            pendCount = lookahead;
            pendPeak = peakRaw;
            armed = false;
        }
        if (! armed && env < thr * rearm)
            armed = true;
        if (retrigCount > 0)
            --retrigCount;

        // ---- гейт оригинала ----
        switch (gateState)
        {
            case 1: gateGain += atkStep; if (gateGain >= 1.0f) { gateGain = 1.0f; gateState = 2; gateHoldCount = holdS; } break;
            case 2: if (--gateHoldCount <= 0) gateState = 3; break;
            case 3: gateGain -= relStep; if (gateGain <= 0.0f) { gateGain = 0.0f; gateState = 0; } break;
            default: break;
        }
        const float g = gateOn ? gateGain : 1.0f;

        // ---- оригинал с задержкой (чтобы совпасть по времени с семплом) ----
        float dry[2] = { 0, 0 };
        for (int c = 0; c < numCh; ++c)
        {
            dry[c] = delayBuf[c][(size_t) delayPos];
            delayBuf[c][(size_t) delayPos] = x[c];
        }
        if (++delayPos >= lookahead) delayPos = 0;

        // ---- голоса семплов ----
        float smp[2] = { 0, 0 };
        for (auto& v : voices)
        {
            if (! v.active) continue;
            const auto& d = v.s->data;
            const int len = d.getNumSamples();
            const int i0 = (int) v.pos;
            if (i0 >= len - 1) { v.active = false; continue; }
            const float fr = (float) (v.pos - (double) i0);
            const int sc = d.getNumChannels();
            for (int c = 0; c < 2; ++c)
            {
                const float* p = d.getReadPointer (juce::jmin (c, sc - 1));
                smp[c] += v.gain * (p[i0] + (p[i0 + 1] - p[i0]) * fr);
            }
            v.pos += v.inc;
        }
        if (numCh == 1) smp[0] = 0.5f * (smp[0] + smp[1]);

        for (int c = 0; c < numCh; ++c)
        {
            const float out = (dry[c] * g * (1.0f - mix) + smp[c] * mix) * og;
            ch[c][i] = out;
            blkOutPeak[c] = juce::jmax (blkOutPeak[c], std::abs (out));
        }

        // ---- окно волны для интерфейса ----
        colPeak = juce::jmax (colPeak, peakRaw);
        if (++colCount >= colSamples)
        {
            const int w = writeIdx.load();
            waveCols[(size_t) w].store (colPeak);
            hitCols[(size_t) w].store (colHit);
            writeIdx.store ((w + 1) % kCols);
            colCount = 0; colPeak = 0.0f; colHit = 0.0f;
        }
    }

    inGainPrev = inGain;
    outGainPrev = outGain;
    for (int c = 0; c < 2; ++c)
    {
        inPeak[c].store (juce::jmax (blkInPeak[c],  inPeak[c].load()  * 0.85f));
        outPeak[c].store (juce::jmax (blkOutPeak[c], outPeak[c].load() * 0.85f));
    }
}

// ---------------------------------------------------------------- состояние
void SnareTriggerAudioProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, dest);
}

void SnareTriggerAudioProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* SnareTriggerAudioProcessor::createEditor()
{
    return new SnareTriggerAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SnareTriggerAudioProcessor();
}
