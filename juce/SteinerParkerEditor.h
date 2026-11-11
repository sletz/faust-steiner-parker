// Steiner-Parker Filter: the faceplate of the original plugin for the faust2juce build.
//
// Adapted from plugin/Source/PluginEditor.{h,cpp} of
// github.com/sofiavallejob/steiner-parker-filter, Copyright (C) 2026 Sofia Vallejo,
// AGPL-3.0-only: the same look and feel, knobs, scales, colours and layout, bound to the
// host parameters of the Faust processor (faust2juce's FAUST_CUSTOM_EDITOR contract,
// see architecture/juce/README.md) instead of the original's AudioProcessorValueTreeState.
//
// Differences with the original:
// - the preset bar steps through the host programs (faust2juce -presets juce/presets.json);
//   saving presets to files is left out;
// - the oversampling of a Faust program is fixed when it is compiled, so the Quality
//   selector shows the factor built in (SP_QUALITY, 1, 2 or 4) and cannot be turned.
//
// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <JuceHeader.h>

#ifndef SP_QUALITY
#define SP_QUALITY 2
#endif

namespace spui {
using namespace juce;

const Colour panel   { 0xff1e3a4c };   // petrol blue faceplate
const Colour panelHi { 0xff26475c };
const Colour silk    { 0xfff2ead3 };   // cream silkscreen
const Colour rail    { 0xffb9bcc0 };
const Colour strip   { 0xff13171b };   // preset bar
const Colour stripHi { 0xff22282e };
const Colour core    { 0xffe0533d };   // filter: Frequency, Resonance
const Colour mod     { 0xfff2b134 };   // modulation: CV offset, V/oct trim
const Colour signal  { 0xff4fb3a9 };   // signal path: Input level, Output
const Colour select  { 0xfff2ead3 };   // selectors: Response, Quality
constexpr int W = 460, STRIP = 48, FACE = 780, H = STRIP + FACE;

class PanelLookAndFeel : public LookAndFeel_V4 {
public:
    PanelLookAndFeel() {
        setColour(ComboBox::textColourId, silk);
        setColour(ComboBox::backgroundColourId, stripHi);
        setColour(ComboBox::outlineColourId, stripHi);
        setColour(ComboBox::arrowColourId, silk.withAlpha(0.6f));
        setColour(PopupMenu::backgroundColourId, stripHi);
        setColour(PopupMenu::textColourId, silk);
        setColour(PopupMenu::headerTextColourId, silk.withAlpha(0.5f));
        setColour(PopupMenu::highlightedBackgroundColourId, core);
        setColour(PopupMenu::highlightedTextColourId, strip);
        setColour(BubbleComponent::backgroundColourId, strip);
        setColour(BubbleComponent::outlineColourId, strip);
        setColour(TooltipWindow::textColourId, silk);
    }

    void drawRotarySlider(Graphics& g, int x, int y, int w, int h, float pos, float start, float end,
                          Slider& s) override {
        auto cap = Colour((uint32) (int64) s.getProperties().getWithDefault("cap", (int64) core.getARGB()));
        auto b = Rectangle<float>((float) x, (float) y, (float) w, (float) h);
        float r = jmin(b.getWidth(), b.getHeight()) * 0.5f - 6.0f;   // margin for the drop shadow
        auto c = b.getCentre();
        float ang = start + pos * (end - start);
        // drop shadow
        g.setColour(Colours::black.withAlpha(0.35f));
        g.fillEllipse(c.x - r + 1.5f, c.y - r + 3.0f, 2 * r, 2 * r);
        // knurled skirt
        float skirt = r;
        g.setColour(Colour(0xff15191d));
        g.fillEllipse(c.x - skirt, c.y - skirt, 2 * skirt, 2 * skirt);
        const int teeth = 28;
        g.setColour(Colour(0xff2c3338));
        for (int i = 0; i < teeth; ++i) {
            float a = MathConstants<float>::twoPi * (float) i / teeth;
            auto p = c.getPointOnCircumference(skirt - 1.5f, a);
            g.fillEllipse(p.x - 1.4f, p.y - 1.4f, 2.8f, 2.8f);
        }
        // coloured cap with a soft top-left highlight
        float cr = r * 0.74f;
        ColourGradient grad(cap.brighter(0.35f), c.x - cr * 0.6f, c.y - cr * 0.7f,
                            cap.darker(0.35f), c.x + cr * 0.7f, c.y + cr * 0.8f, true);
        g.setGradientFill(grad);
        g.fillEllipse(c.x - cr, c.y - cr, 2 * cr, 2 * cr);
        g.setColour(Colours::black.withAlpha(0.25f));
        g.drawEllipse(c.x - cr, c.y - cr, 2 * cr, 2 * cr, 1.0f);
        // pointer line from the skirt into the cap
        bool lightCap = cap.getPerceivedBrightness() > 0.75f;
        auto tip = c.getPointOnCircumference(r - 2.0f, ang);
        auto base = c.getPointOnCircumference(cr * 0.25f, ang);
        g.setColour(lightCap ? Colour(0xff1b1f23) : silk);
        g.drawLine({ base, tip }, jmax(2.0f, r * 0.08f));
    }

    void drawButtonBackground(Graphics& g, Button& b, const Colour&, bool over, bool down) override {
        auto r = b.getLocalBounds().toFloat().reduced(0.5f);
        g.setColour(down ? core.darker(0.2f) : over ? core : stripHi);
        g.fillRoundedRectangle(r, 3.0f);
    }
    void drawButtonText(Graphics& g, TextButton& b, bool over, bool down) override {
        g.setColour(over || down ? strip : silk);
        g.setFont(FontOptions(13.0f));
        g.drawText(b.getButtonText(), b.getLocalBounds(), Justification::centred);
    }
    void drawComboBox(Graphics& g, int w, int h, bool, int, int, int, int, ComboBox& box) override {
        auto r = Rectangle<float>(0, 0, (float) w, (float) h).reduced(0.5f);
        g.setColour(box.isMouseOver(true) ? stripHi.brighter(0.08f) : stripHi);
        g.fillRoundedRectangle(r, 3.0f);
        Path arrow;
        float ax = (float) w - 18.0f, ay = (float) h * 0.5f;
        arrow.addTriangle(ax - 4, ay - 2, ax + 4, ay - 2, ax, ay + 3);
        g.setColour(silk.withAlpha(0.6f));
        g.fillPath(arrow);
    }
    Font getComboBoxFont(ComboBox&) override { return FontOptions(14.0f); }
    void positionComboBoxText(ComboBox& box, Label& label) override {
        label.setBounds(10, 0, box.getWidth() - 34, box.getHeight());
        label.setFont(getComboBoxFont(box));
        label.setJustificationType(Justification::centredLeft);
    }
    void drawBubble(Graphics& g, BubbleComponent&, const Point<float>&, const Rectangle<float>& body) override {
        g.setColour(strip.withAlpha(0.95f));
        g.fillRoundedRectangle(body, 3.0f);
    }
    Font getSliderPopupFont(Slider&) override { return FontOptions(13.0f); }
};

// The host parameter of the Faust processor with this ID. faust2juce names them after the
// shortnames of the controls ("Frequency", "CV_offset", "V_oct_trim"...), in which characters
// such as ' ' and '-' of the labels become '_': the IDs are compared with every character
// that is not a letter or a digit read as '_'.
inline String normalisedId(const String& id) {
    String out;
    for (auto c : id) out << (CharacterFunctions::isLetterOrDigit(c) ? c : juce_wchar('_'));
    return out;
}

inline RangedAudioParameter* findParameter(AudioProcessor& p, const String& id) {
    for (auto* prm : p.getParameters())
        if (auto* r = dynamic_cast<RangedAudioParameter*>(prm))
            if (normalisedId(r->getParameterID()) == normalisedId(id)) return r;
    jassertfalse;
    return nullptr;
}

// A knob with its silkscreened scale. For selectors, clicking a scale label selects it.
// Without a parameter, the knob is a fixed indicator (the Quality of the build).
class PanelKnob : public Component {
public:
    struct Mark { float pos; String text; };
    PanelKnob(RangedAudioParameter* prm, String t, Colour cap, float capRadius, int nTicks,
              std::vector<Mark> m, bool selector = false)
        : title(std::move(t)), capR(capRadius), ticks(nTicks), marks(std::move(m)), isSelector(selector) {
        startA = selector ? MathConstants<float>::pi * 1.75f : MathConstants<float>::pi * 1.25f;
        endA = selector ? MathConstants<float>::pi * 2.25f : MathConstants<float>::pi * 2.75f;
        slider.setSliderStyle(Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle(Slider::NoTextBox, false, 0, 0);
        slider.setRotaryParameters(startA, endA, true);
        slider.setPopupDisplayEnabled(!selector, true, nullptr, 900);
        slider.setMouseDragSensitivity(selector ? 120 : 220);
        slider.getProperties().set("cap", (int64) cap.getARGB());
        addAndMakeVisible(slider);
        if (prm != nullptr) {
            att = std::make_unique<SliderParameterAttachment>(*prm, slider);
            if (!selector) slider.setDoubleClickReturnValue(true, (double) prm->convertFrom0to1(prm->getDefaultValue()));
        } else {
            slider.setRange(0.0, (double) marks.size() - 1.0, 1.0);
            slider.setEnabled(false);
        }
    }

    void setFixed(int index) { slider.setValue(index, dontSendNotification); }

    void resized() override {
        auto c = centre();
        int d = (int) (2 * capR + 12);
        slider.setBounds(Rectangle<int>(d, d).withCentre(c.toInt()));
    }

    void paint(Graphics& g) override {
        auto c = centre();
        g.setColour(silk);
        g.setFont(FontOptions(13.5f, Font::bold));
        g.drawText(title, Rectangle<float>(0, 0, (float) getWidth(), 18), Justification::centred);
        g.setColour(silk.withAlpha(0.85f));
        for (int i = 0; i < ticks; ++i) {
            float pos = ticks > 1 ? (float) i / (ticks - 1) : 0.5f;
            float a = startA + pos * (endA - startA);
            bool major = isSelector || i % 2 == 0;
            float r0 = capR + 5.0f, r1 = capR + (major ? 12.0f : 9.0f);
            g.drawLine({ c.getPointOnCircumference(r0, a), c.getPointOnCircumference(r1, a) }, major ? 1.6f : 1.1f);
        }
        int sel = isSelector ? (int) std::round(slider.getValue() - slider.getMinimum()) : -1;
        for (size_t i = 0; i < marks.size(); ++i) {
            auto p = labelPos(marks[i].pos);
            bool on = isSelector && (int) i == sel;
            g.setColour(on ? mod : silk.withAlpha(isSelector ? 0.75f : 0.8f));
            g.setFont(FontOptions(isSelector ? 13.0f : 11.0f, on ? Font::bold : Font::plain));
            g.drawText(marks[i].text, Rectangle<float>(46, 16).withCentre(p), Justification::centred);
        }
    }

    void mouseDown(const MouseEvent& e) override {
        if (!isSelector || att == nullptr) return;
        int best = -1;
        float bestD = 18.0f;
        for (size_t i = 0; i < marks.size(); ++i) {
            float d = e.position.getDistanceFrom(labelPos(marks[i].pos));
            if (d < bestD) { bestD = d; best = (int) i; }
        }
        if (best >= 0) slider.setValue(slider.getMinimum() + best, sendNotificationSync);
    }

    Slider slider;

private:
    std::unique_ptr<SliderParameterAttachment> att;
    String title;
    float capR;
    int ticks;
    std::vector<Mark> marks;
    bool isSelector;
    float startA, endA;
    Point<float> centre() const { return { getWidth() * 0.5f, 22.0f + capR + 26.0f }; }
    Point<float> labelPos(float pos) const {
        float a = startA + pos * (endA - startA);
        return centre().getPointOnCircumference(capR + (isSelector ? 22.0f : 24.0f), a);
    }
};

inline std::vector<PanelKnob::Mark> zeroToTen() {
    std::vector<PanelKnob::Mark> m;
    for (int i = 0; i <= 10; i += 2) m.push_back({ i / 10.0f, String(i) });
    return m;
}

class SteinerParkerEditor : public AudioProcessorEditor, private Timer {
public:
    explicit SteinerParkerEditor(AudioProcessor& p)
        : AudioProcessorEditor(p), proc(p),
          frequency(findParameter(p, "Frequency"), "FREQUENCY", core, 46.0f, 21, zeroToTen()),
          resonance(findParameter(p, "Resonance"), "RESONANCE", core, 46.0f, 21, zeroToTen()),
          cvOffset(findParameter(p, "CV_offset"), "CV OFFSET", mod, 30.0f, 13, { { 0.0f, "-3" }, { 0.5f, "0" }, { 1.0f, "+3" } }),
          trim(findParameter(p, "V_oct_trim"), "V/OCT TRIM", mod, 30.0f, 11, { { 0.0f, "0" }, { 0.5f, "5" }, { 1.0f, "10" } }),
          inputLevel(findParameter(p, "Input_level"), "INPUT LEVEL", signal, 34.0f, 13, { { 0.0f, "-24" }, { 0.5f, "0" }, { 1.0f, "+24" } }),
          response(findParameter(p, "Response"), "RESPONSE", select, 30.0f, 3, { { 0.0f, "LP" }, { 0.5f, "BP" }, { 1.0f, "HP" } }, true),
          output(findParameter(p, "Output"), "OUTPUT", signal, 24.0f, 9, { { 0.0f, "-36" }, { 0.75f, "0" }, { 1.0f, "+12" } }),
          quality(nullptr, "QUALITY", select, 24.0f, 3, { { 0.0f, "1x" }, { 0.5f, "2x" }, { 1.0f, "4x" } }, true) {
        setLookAndFeel(&lnf);
        addAndMakeVisible(content);
        quality.setFixed(SP_QUALITY == 1 ? 0 : SP_QUALITY == 2 ? 1 : 2);
        quality.slider.setTooltip("Oversampling of this build (fixed when the Faust program is compiled)");
        for (auto* k : { &frequency, &resonance, &cvOffset, &trim, &inputLevel, &response, &output, &quality })
            content.addAndMakeVisible(*k);
        response.slider.onValueChange = [this] { response.repaint(); };

        for (auto* b : { &prevButton, &nextButton }) content.addAndMakeVisible(*b);
        content.addAndMakeVisible(presetBox);
        prevButton.onClick = [this] { stepPreset(-1); };
        nextButton.onClick = [this] { stepPreset(+1); };
        presetBox.onChange = [this] {
            int id = presetBox.getSelectedId();
            if (id > 0 && id - 1 != proc.getCurrentProgram()) proc.setCurrentProgram(id - 1);
        };
        presetBox.setTooltip("Factory presets (the host programs)");
        presetBox.addSectionHeading("Factory");
        for (int i = 0; i < proc.getNumPrograms(); ++i) presetBox.addItem(proc.getProgramName(i), i + 1);

        authorLink.setButtonText("GRAME   -   FAUST PORT");
        authorLink.setFont(FontOptions(9.0f, Font::bold), false, Justification::centred);
        authorLink.setColour(HyperlinkButton::textColourId, silk.withAlpha(0.8f));
        authorLink.setURL(URL("https://github.com/sletz/faust-steiner-parker"));
        content.addAndMakeVisible(authorLink);

        setResizable(true, true);
        getConstrainer()->setFixedAspectRatio((double) W / H);
        setResizeLimits(W * 6 / 10, H * 6 / 10, W * 3 / 2, H * 3 / 2);
        setSize(W * 9 / 10, H * 9 / 10);
        startTimerHz(10);
        timerCallback();
    }

    ~SteinerParkerEditor() override { setLookAndFeel(nullptr); }

    void paint(Graphics& g) override { g.fillAll(strip); }

    void resized() override {
        float s = (float) getWidth() / (float) W;
        content.setBounds(0, 0, W, H);
        content.setTransform(AffineTransform::scale(s));
        auto bar = Rectangle<int>(0, 0, W, STRIP).reduced(14, 10);
        prevButton.setBounds(bar.removeFromLeft(28));
        bar.removeFromLeft(6);
        nextButton.setBounds(bar.removeFromRight(28));
        bar.removeFromRight(6);
        presetBox.setBounds(bar);
        authorLink.setBounds(44, STRIP + FACE - 16, W - 88, 16);
        const int top = STRIP, colW = 200, x1 = 30, x2 = W - 30 - colW;
        auto place = [&](PanelKnob& k, int x, int y, int h) { k.setBounds(x, top + y, colW, h); };
        place(frequency, x1, 104, 156);
        place(resonance, x2, 104, 156);
        place(cvOffset, x1, 300, 128);
        place(trim, x2, 300, 128);
        place(inputLevel, x1, 468, 134);
        place(response, x2, 468, 134);
        place(output, x1, 642, 116);
        place(quality, x2, 642, 116);
    }

private:
    struct Content : Component {
        void paint(Graphics& g) override {
            const float top = (float) STRIP, w = (float) W, f = (float) FACE;
            g.setColour(strip);
            g.fillRect(0.0f, 0.0f, w, top);
            auto face = Rectangle<float>(0, top, w, f);
            g.setGradientFill(ColourGradient(panelHi, 0, top, panel, 0, top + f, false));
            g.fillRect(face);
            for (float y : { top, top + f - 16.0f }) {
                g.setColour(rail.withAlpha(0.18f));
                g.fillRect(0.0f, y, w, 16.0f);
                for (float x : { 22.0f, w - 22.0f }) {
                    g.setColour(Colour(0xffc9ccd0));
                    g.fillEllipse(x - 6, y + 2, 12, 12);
                    g.setColour(Colour(0xff6c7278));
                    g.drawLine(x - 3.5f, y + 8, x + 3.5f, y + 8, 1.5f);
                    g.drawLine(x, y + 4.5f, x, y + 11.5f, 1.5f);
                }
            }
            g.setColour(silk);
            g.setFont(FontOptions(28.0f, Font::bold));
            g.drawText("STEINER-PARKER", Rectangle<float>(0, top + 22, w, 34), Justification::centred);
            g.setColour(silk.withAlpha(0.6f));
            g.setFont(FontOptions(11.5f));
            g.drawText("WAVE DIGITAL MODEL   /   AFTER YUSYNTH   /   FAUST", Rectangle<float>(0, top + 56, w, 16), Justification::centred);
            struct Div { float y; Colour c; const char* name; };
            for (auto d : { Div { 92, core, "FILTER" }, Div { 288, mod, "MODULATION" },
                            Div { 456, signal, "SIGNAL" }, Div { 630, select, "SETTINGS" } }) {
                float y = top + d.y;
                Font fo = FontOptions(10.0f, Font::bold);
                float tw = GlyphArrangement::getStringWidth(fo, d.name);
                g.setColour(d.c);
                g.fillRoundedRectangle(30.0f, y - 1.5f, 22.0f, 3.0f, 1.5f);
                g.setColour(silk.withAlpha(0.55f));
                g.setFont(fo);
                g.drawText(d.name, Rectangle<float>(60, y - 7, tw + 4, 14), Justification::centredLeft);
                g.setColour(silk.withAlpha(0.16f));
                g.fillRect(60.0f + tw + 12.0f, y, w - 30.0f - (60.0f + tw + 12.0f), 1.0f);
            }
        }
    };

    void timerCallback() override {
        int cur = proc.getCurrentProgram();
        if (cur != shown) {
            shown = cur;
            presetBox.setSelectedId(cur + 1, dontSendNotification);
        }
    }

    void stepPreset(int delta) {
        int n = proc.getNumPrograms();
        if (n <= 0) return;
        proc.setCurrentProgram((jmax(0, proc.getCurrentProgram()) + delta + n) % n);
        timerCallback();
    }

    AudioProcessor& proc;
    PanelLookAndFeel lnf;
    Content content;
    PanelKnob frequency, resonance, cvOffset, trim, inputLevel, response, output, quality;
    TextButton prevButton { "<" }, nextButton { ">" };
    ComboBox presetBox;
    HyperlinkButton authorLink;
    int shown = -2;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SteinerParkerEditor)
};
} // namespace spui

// faust2juce's custom editor contract
struct FaustEditorBridge {
    void prepare(double, int, int) {}
    void processMidi(juce::MidiBuffer&, int) {}
    template <typename T> void processAudio(const juce::AudioBuffer<T>&, int) {}
};

inline juce::AudioProcessorEditor* createFaustEditor(juce::AudioProcessor& p, FaustEditorBridge&) {
    return new spui::SteinerParkerEditor(p);
}
