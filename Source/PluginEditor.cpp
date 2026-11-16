#include "PluginEditor.h"
#include "SnareImageJuce.h"

using juce::Colour;
using juce::Justification;

namespace
{
    const Colour kBlueRing (60, 120, 220);
    const Colour kRedRing  (206, 84, 66);
    const Colour kBlueLed  (60, 140, 255);
    const Colour kSceneBg  (188, 198, 203);       // фон пиксельного барабана
}

// ------------------------------------------------------------------ ручки
class SnareTriggerAudioProcessorEditor::LnF : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                           float a0, float a1, juce::Slider& s) override
    {
        auto b = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (3.0f);
        const float r = juce::jmin (b.getWidth(), b.getHeight()) * 0.5f;
        const auto c = b.getCentre();
        const Colour ring ((juce::uint32) (int) s.getProperties().getWithDefault ("ring", (int) 0xffce5442));

        g.setColour (Colour (34, 35, 38));
        g.fillEllipse (c.x - r - 3, c.y - r - 3, 2 * r + 6, 2 * r + 6);
        g.setColour (ring);
        g.fillEllipse (c.x - r, c.y - r, 2 * r, 2 * r);
        g.setColour (Colour (74, 72, 76));
        g.fillEllipse (c.x - r * 0.78f, c.y - r * 0.78f, r * 1.56f, r * 1.56f);
        g.setColour (Colour (120, 112, 112));
        g.fillEllipse (c.x - r * 0.6f, c.y - r * 0.6f, r * 1.2f, r * 1.2f);

        const float ang = a0 + pos * (a1 - a0);       // от 12 часов по часовой
        const float sn = std::sin (ang), cs = -std::cos (ang);
        g.setColour (Colour (250, 250, 250));
        g.drawLine (c.x + sn * r * 0.15f, c.y + cs * r * 0.15f, c.x + sn * r * 0.85f, c.y + cs * r * 0.85f, 3.0f);
    }
};

// ------------------------------------------------------------------ панель
class TriggerPanel : public juce::Component, private juce::Timer
{
public:
    TriggerPanel (SnareTriggerAudioProcessor& p) : proc (p)
    {
        // ---- GATE ----
        addKnob (ids::attack,  "ATTACK",  46, 76,  Justification::centredTop, 12, 46, 110, 15, false, { 10, 132, 72, 18 }, " ms", 1);
        addKnob (ids::hold,    "HOLD",    46, 160, Justification::centredTop, 12, 46, 194, 15, false, { 10, 216, 72, 18 }, " ms", 1);
        addKnob (ids::release, "RELEASE", 46, 246, Justification::centredTop, 12, 46, 280, 15, false, { 10, 302, 72, 18 }, " ms", 0);
        // ---- INPUT ----
        addKnob (ids::inputVol, "VOLUME", 139, 166, Justification::centredTop, 12, 139, 200, 14, true, { 103, 222, 72, 18 }, " dB", 2);
        // ---- фильтры и подавление ----
        addKnob (ids::lowcut,   "LOWCUT",   194, 22,  Justification::topLeft, 13, 208, 62,  13, true,  { 232, 50, 68, 22 },  " hz", 0);
        addKnob (ids::highcut,  "HIGHCUT",  194, 94,  Justification::topLeft, 13, 208, 132, 13, true,  { 232, 120, 68, 22 }, " hz", 0);
        addKnob (ids::suppress, "SUPPRESS", 194, 264, Justification::topLeft, 13, 208, 300, 15, false, { 232, 288, 68, 24 }, "%", 0);
        // ---- справа ----
        addKnob (ids::sensitivity, "SENSITIVITY", 905, 46,  Justification::centredTop, 16, 860, 94,  20, false, { 893, 76, 72, 34 },  "", 0);
        addKnob (ids::retrigger,   "RETRIGGER",   905, 134, Justification::centredTop, 16, 860, 182, 18, false, { 893, 164, 72, 34 }, " ms", 0);
        addKnob (ids::detail,      "DETAIL",      905, 221, Justification::centredTop, 16, 860, 269, 20, false, { 893, 251, 72, 34 }, "", 0);
        // ---- OUTPUT ----
        addKnob (ids::outputVol, "VOLUME", 1022, 160, Justification::centredTop, 12, 1022, 196, 13, true, { 988, 216, 68, 18 }, " dB", 2);
        addKnob (ids::mix,       "MIX",    1022, 244, Justification::centredTop, 12, 1022, 276, 13, true, { 988, 300, 68, 22 }, "%", 0);

        gateBtn.setButtonText ("On");
        gateBtn.setClickingTogglesState (true);
        gateBtn.setColour (juce::TextButton::buttonColourId, Colour (40, 41, 45));
        gateBtn.setColour (juce::TextButton::buttonOnColourId, Colour (60, 130, 235));
        gateBtn.setColour (juce::TextButton::textColourOffId, Colour (170, 175, 184));
        gateBtn.setColour (juce::TextButton::textColourOnId, juce::Colours::white);
        gateBtn.setBounds (30, 40, 32, 22);
        addAndMakeVisible (gateBtn);
        gateAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, ids::gateOn, gateBtn);

        // ---- карточки внизу ----
        openBtn.setButtonText ("Open folder");
        openBtn.setBounds (30, 500, 96, 26);
        openBtn.onClick = [this] { SampleLoader::userFolder().createDirectory(); SampleLoader::userFolder().revealToUser(); };
        addAndMakeVisible (openBtn);

        rescanBtn.setButtonText ("Rescan");
        rescanBtn.setBounds (136, 500, 90, 26);
        rescanBtn.onClick = [this] { proc.rescanSamples(); repaint(); };
        addAndMakeVisible (rescanBtn);

        auditionBtn.setButtonText (">  Audition");
        auditionBtn.setBounds (850, 500, 186, 26);
        auditionBtn.onClick = [this] { proc.audition(); };
        addAndMakeVisible (auditionBtn);

        startTimerHz (30);
    }

    ~TriggerPanel() override { stopTimer(); }

    void paint (juce::Graphics& g) override
    {
        // ---------- верхняя половина: настройки ----------
        g.setGradientFill (juce::ColourGradient (Colour (98, 99, 102), 0, 0, Colour (68, 69, 73), 0, 346, false));
        g.fillRect (0, 0, 1070, 346);

        panel (g, 2, 8, 88, 330);   txt (g, "GATE", 46, 14, 13);
        panel (g, 96, 8, 86, 330);  txt (g, "INPUT", 139, 14, 13);
        panel (g, 976, 8, 92, 330); txt (g, "OUTPUT", 1022, 14, 13);
        meter (g, 108, 40, proc.inPeak);
        meter (g, 992, 40, proc.outPeak);
        for (auto& k : knobs)
            txt (g, k->title, k->tx, k->ty, k->size, Colour (255, 255, 255), k->just);

        drawWave (g);

        // ---------- нижняя половина: пиксельный барабан ----------
        g.setColour (kSceneBg);
        g.fillRect (0, 346, 1070, 400);
        g.setColour (Colour (30, 31, 35));
        g.fillRect (0, 346, 1070, 3);

        snareimg::draw (g, juce::Rectangle<float> (256.0f, 364.0f, 558.0f, 372.0f));

        card (g, 18, 368, 218, 176, "SAMPLES");
        const int n = proc.getNumSamples();
        txt (g, "Loaded: " + juce::String (n) + " / 30", 30, 402, 12, Colour (225, 228, 232), Justification::topLeft, false);
        txt (g, "Source: " + proc.getSampleSource(), 30, 424, 10, Colour (150, 155, 164), Justification::topLeft, false, 196);
        const int li = proc.lastSampleIndex.load();
        txt (g, li >= 0 ? "Last hit: " + juce::String (juce::roundToInt (proc.lastVelocity.load() * 100.0f)) + "%  ->  #" + juce::String (li + 1)
                        : juce::String ("Last hit: -"),
             30, 446, 11, Colour (225, 228, 232), Justification::topLeft, false, 196);
        txt (g, li >= 0 ? proc.getSampleName (li) : juce::String(), 30, 466, 10, Colour (150, 155, 164), Justification::topLeft, false, 196);
        txt (g, "Drop your WAV files into this folder", 30, 482, 10, Colour (150, 155, 164), Justification::topLeft, false, 196);

        card (g, 834, 368, 218, 176, "AUDITION");
        txt (g, "Plays a test hit from the library", 846, 402, 11, Colour (225, 228, 232), Justification::topLeft, false, 196);
        txt (g, "(soft ... hard, one after another)", 846, 420, 10, Colour (150, 155, 164), Justification::topLeft, false, 196);
    }

private:
    struct Knob
    {
        juce::Slider slider;
        juce::Label lcd;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
        juce::String title;
        float tx = 0, ty = 0, size = 12;
        Justification just = Justification::centredTop;
    };

    void addKnob (const char* id, const char* title, float tx, float ty, Justification just, float size,
                  int cx, int cy, int r, bool blue, juce::Rectangle<int> lcdRect, const char* suffix, int decimals)
    {
        auto k = std::make_unique<Knob>();
        k->title = title; k->tx = tx; k->ty = ty; k->size = size; k->just = just;

        auto& s = k->slider;
        s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        s.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
        s.setTextValueSuffix (suffix);
        s.setNumDecimalPlacesToDisplay (decimals);
        s.getProperties().set ("ring", (int) (blue ? kBlueRing : kRedRing).getARGB());
        s.setBounds (cx - r - 5, cy - r - 5, 2 * r + 10, 2 * r + 10);
        addAndMakeVisible (s);

        auto& l = k->lcd;
        l.setJustificationType (Justification::centred);
        l.setFont (juce::Font (11.0f, juce::Font::bold));
        l.setColour (juce::Label::backgroundColourId, Colour (247, 247, 247));
        l.setColour (juce::Label::textColourId, Colour (30, 30, 34));
        l.setColour (juce::Label::outlineColourId, Colour (30, 30, 33));
        l.setInterceptsMouseClicks (false, false);
        l.setBounds (lcdRect);
        addAndMakeVisible (l);

        auto* kp = k.get();
        s.onValueChange = [kp] { kp->lcd.setText (kp->slider.getTextFromValue (kp->slider.getValue()), juce::dontSendNotification); };
        k->att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, id, s);
        s.onValueChange();
        knobs.push_back (std::move (k));
    }

    // ---- рисование ----
    static void txt (juce::Graphics& g, const juce::String& t, float x, float y, float size,
                     Colour c = Colour (255, 255, 255), Justification j = Justification::centredTop,
                     bool bold = true, float width = 300.0f)
    {
        g.setColour (c);
        g.setFont (juce::Font (size, bold ? juce::Font::bold : juce::Font::plain));
        float bx = x;
        if (j == Justification::centredTop) bx = x - width * 0.5f;
        else if (j == Justification::topRight) bx = x - width;
        g.drawText (t, juce::Rectangle<float> (bx, y, width, size + 6.0f), j, true);
    }

    static void panel (juce::Graphics& g, int x, int y, int w, int h)
    {
        g.setColour (Colour (62, 63, 68));
        g.fillRoundedRectangle ((float) x, (float) y, (float) w, (float) h, 5.0f);
        g.setColour (Colour (36, 37, 41));
        g.drawRoundedRectangle ((float) x, (float) y, (float) w, (float) h, 5.0f, 2.0f);
    }

    static void card (juce::Graphics& g, int x, int y, int w, int h, const juce::String& title)
    {
        g.setColour (Colour (52, 53, 58));
        g.fillRoundedRectangle ((float) x, (float) y, (float) w, (float) h, 6.0f);
        g.setColour (Colour (30, 31, 34));
        g.drawRoundedRectangle ((float) x, (float) y, (float) w, (float) h, 6.0f, 2.0f);
        txt (g, title, (float) x + 12, (float) y + 8, 11, Colour (235, 237, 240), Justification::topLeft);
    }

    static void meter (juce::Graphics& g, int x, int y, const std::atomic<float>* peaks)
    {
        static const char* labels[6] = { "-0", "-3", "-6", "-12", "-24", "-48" };
        static const float db[6] = { -0.5f, -3.0f, -6.0f, -12.0f, -24.0f, -48.0f };
        g.setColour (Colour (40, 41, 45));
        g.fillRoundedRectangle ((float) x, (float) y, 62.0f, 112.0f, 3.0f);
        for (int i = 0; i < 6; ++i)
        {
            const float yy = (float) y + 8.0f + (float) i * 96.0f / 5.0f;
            txt (g, labels[i], (float) x + 31.0f, yy - 6.0f, 8, Colour (230, 232, 236), Justification::centredTop, true, 40.0f);
            for (int c = 0; c < 2; ++c)
            {
                const float lvl = juce::Decibels::gainToDecibels (peaks[c].load(), -100.0f);
                g.setColour (lvl >= db[i] ? kBlueLed : Colour (64, 70, 82));
                g.fillRect ((float) x + (c == 0 ? 7.0f : 47.0f), yy - 3.0f, 8.0f, 6.0f);
            }
        }
    }

    void drawWave (juce::Graphics& g)
    {
        const juce::Rectangle<float> area (310.0f, 8.0f, 520.0f, 330.0f);
        g.setColour (Colour (11, 12, 16));
        g.fillRoundedRectangle (area, 4.0f);
        g.setColour (Colour (30, 31, 34));
        g.drawRoundedRectangle (area, 4.0f, 2.0f);
        txt (g, "Input", 318, 14, 12, Colour (120, 124, 132), Justification::topLeft);

        const float cy = area.getCentreY() + 8.0f, half = 150.0f;
        auto hOf = [half] (float lin)
        {
            const float db = juce::Decibels::gainToDecibels (lin, -60.0f);
            return juce::jlimit (0.0f, 1.0f, (db + 54.0f) / 54.0f) * half;
        };

        const int w0 = proc.writeIdx.load();
        for (int i = 0; i < SnareTriggerAudioProcessor::kCols; ++i)
        {
            const int idx = (w0 + i) % SnareTriggerAudioProcessor::kCols;
            const float x = area.getX() + 1.0f + (float) i;
            const float pk = proc.waveCols[(size_t) idx].load();
            const float h = hOf (pk);
            const float hit = proc.hitCols[(size_t) idx].load();
            if (hit > 0.0f)
            {
                g.setColour (Colour (95, 98, 108));
                g.drawVerticalLine ((int) x, area.getY() + 4.0f, area.getBottom() - 4.0f);
                g.setColour (Colour (255, 140, 50));
                juce::Path t1, t2;
                t1.addTriangle (x - 4, cy - h - 7, x + 4, cy - h - 7, x, cy - h);
                t2.addTriangle (x - 4, cy + h + 7, x + 4, cy + h + 7, x, cy + h);
                g.fillPath (t1); g.fillPath (t2);
            }
            g.setColour (Colour (52, 160, 255));
            g.drawVerticalLine ((int) x, cy - h, cy + h + 1.0f);
        }

        const float th = hOf (juce::Decibels::decibelsToGain (proc.thresholdDb.load()));
        g.setColour (Colour (225, 228, 232));
        g.fillRect (area.getX() + 1.0f, cy - th - 1.0f, area.getWidth() - 2.0f, 2.0f);
        g.fillRect (area.getX() + 1.0f, cy + th - 1.0f, area.getWidth() - 2.0f, 2.0f);
    }

    void timerCallback() override
    {
        repaint (310, 8, 520, 330);
        repaint (108, 40, 62, 112);
        repaint (992, 40, 62, 112);
        repaint (18, 368, 218, 126);
    }

    SnareTriggerAudioProcessor& proc;
    std::vector<std::unique_ptr<Knob>> knobs;
    juce::TextButton gateBtn, openBtn, rescanBtn, auditionBtn;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> gateAtt;
};

// ------------------------------------------------------------------ окно
SnareTriggerAudioProcessorEditor::SnareTriggerAudioProcessorEditor (SnareTriggerAudioProcessor& p)
    : AudioProcessorEditor (&p),
      lnf (std::make_unique<LnF>()),
      panel (std::make_unique<TriggerPanel> (p))
{
    panel->setLookAndFeel (lnf.get());
    addAndMakeVisible (*panel);

    setResizable (true, true);
    setResizeLimits (kDesignW / 2, kDesignH / 2, kDesignW * 2, kDesignH * 2);
    getConstrainer()->setFixedAspectRatio ((double) kDesignW / (double) kDesignH);
    setSize (kDesignW, kDesignH);
}

SnareTriggerAudioProcessorEditor::~SnareTriggerAudioProcessorEditor()
{
    panel->setLookAndFeel (nullptr);
}

void SnareTriggerAudioProcessorEditor::resized()
{
    panel->setBounds (0, 0, kDesignW, kDesignH);
    panel->setTransform (juce::AffineTransform::scale ((float) getWidth() / (float) kDesignW));
}
