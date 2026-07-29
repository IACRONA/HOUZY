#pragma once

#include "PluginProcessor.h"
#include "melatonin_blur/melatonin_blur.h"  // fast drop-shadows / glow

//==============================================================================
// PLUGIN VERSION — shown in the ACRONA pop-up (click "by ACRONA").
// BUMP THIS on every meaningful change (new feature, sound change, bug fix batch)
// so users can tell builds apart. Keep it in sync with installer/HouseComp.iss (the .iss filename is unchanged).
static constexpr const char* kPluginVersion = "v3.9";

//==============================================================================
// A rotary slider with a PRECISE mouse-wheel: each notch nudges the value by a
// small fixed fraction of the range (fine control), and Shift = 5x coarser. This
// replaces JUCE's default wheel step, which was too coarse for careful tweaking.
class WheelSlider : public juce::Slider,
                    private juce::Timer
{
public:
    ~WheelSlider() override { stopTimer(); }   // no late callback during teardown

    // Smoothed hover amount 0..1 — the LookAndFeel reads this to GROW the value arc
    // gently when the pointer is over the knob (soft ease, no snap).
    float hoverAmt { 0.0f };

    void mouseWheelMove (const juce::MouseEvent& e,
                         const juce::MouseWheelDetails& w) override
    {
        const double range = getMaximum() - getMinimum();
        if (range <= 0.0) return;
        const double fine = (e.mods.isShiftDown() ? 0.05 : 0.01) * range; // 1% / 5%
        const double dir  = (w.deltaY != 0.0f ? w.deltaY : w.deltaX) > 0.0f ? 1.0 : -1.0;
        double step = fine * dir;
        if (getInterval() > 0.0)   // snap to at least one interval so it always moves
            step = dir * juce::jmax (getInterval(), std::abs (step));
        setValue (getValue() + step, juce::sendNotificationSync);
    }

    // The hover-ease timer runs ONLY while animating (mouse enters, or the arc is
    // still easing back to 0). It stops itself when settled — so idle knobs cost 0.
    void mouseEnter (const juce::MouseEvent&) override { startTimerHz (60); }
    void mouseExit  (const juce::MouseEvent&) override { startTimerHz (60); }

private:
    void timerCallback() override
    {
        const float target = isMouseOverOrDragging() ? 1.0f : 0.0f;
        const float next   = hoverAmt + (target - hoverAmt) * 0.10f;   // slow, smooth ease
        hoverAmt = (std::abs (next - hoverAmt) > 0.001f) ? next : target;
        getProperties().set ("hoverAmt", hoverAmt);   // ALWAYS keep the property in sync
        repaint();
        if (hoverAmt == target) stopTimer();          // settled → stop (no idle cost)
    }
};

//==============================================================================
// An N-segment switch: [ A | B | C ]. Clicking a segment selects it; the active
// segment is highlighted. Two segments bind to a bool APVTS param (false = left),
// three or more to a choice param (index = segment).
// Used for the pick-one mode groups (CLIP|HI-Q|MASK, SMART|IRC IV, AUTO|LUFS,
// CLASSIC|MODERN).
class SegmentedSwitch : public juce::Component,
                        public juce::SettableTooltipClient
{
public:
    std::function<void (int)> onChange;   // called with the new segment index
    juce::Typeface::Ptr drukFace;   // Druk Wide (set by the editor; NOT static — a
                                    // static JUCE Typeface::Ptr hangs on DLL unload)

    void setLabels (juce::StringArray l)            { labels = std::move (l); repaint(); }
    void setLabels (juce::String l, juce::String r) { setLabels (juce::StringArray { l, r }); }

    // PER-SEGMENT tooltips. One blob of text covering every position forces the reader
    // to work out which sentence applies to the thing they are pointing at; this shows
    // only what the segment under the cursor actually does. Falls back to the plain
    // component tooltip when no per-segment text has been set.
    void setSegmentTooltips (juce::StringArray t) { segTips = std::move (t); }

    juce::String getTooltip() override
    {
        const int seg = segmentAt ((float) getMouseXYRelative().x);
        if (seg >= 0 && seg < segTips.size() && segTips[seg].isNotEmpty())
            return segTips[seg];
        return juce::SettableTooltipClient::getTooltip();
    }
    void setIndex  (int i)                          { index = juce::jlimit (0, numSegments() - 1, i); repaint(); }
    int  getIndex() const                           { return index; }
    int  numSegments() const                        { return juce::jmax (1, labels.size()); }
    // Convenience for the 2-segment switches bound to a bool param.
    void setValue  (bool rightSelected)             { setIndex (rightSelected ? 1 : 0); }
    bool getValue() const                           { return index > 0; }

    void paint (juce::Graphics& g) override
    {
        // Neon-outline style: the ACTIVE half gets a cyan border + faint cyan
        // wash and bright text; the inactive half stays flat and dim.
        const juce::Colour cyan   { 0xff4dd8ef };
        const juce::Colour panel  { 0xff262d37 };  // -15%
        const juce::Colour edge   { 0xff343c47 };  // -15%
        const juce::Colour dimTxt { 0xff8796a1 };  // -15%

        auto b = getLocalBounds().toFloat();
        const float rad = 6.0f;

        // Recessed body: fill, a soft top inner-shadow (light comes from above so the
        // top edge is darker inside), and a faint bottom gloss catch — reads as a
        // shallow inset well, matching the plugin's single light model.
        g.setColour (panel);
        g.fillRoundedRectangle (b, rad);
        {
            juce::ColourGradient inner (juce::Colours::black.withAlpha (0.22f), b.getX(), b.getY(),
                                        juce::Colours::transparentBlack, b.getX(), b.getY() + b.getHeight() * 0.6f, false);
            g.setGradientFill (inner);
            g.fillRoundedRectangle (b, rad);
            juce::ColourGradient catchLt (juce::Colours::transparentBlack, b.getX(), b.getBottom() - 6.0f,
                                          juce::Colours::white.withAlpha (0.04f), b.getX(), b.getBottom(), false);
            g.setGradientFill (catchLt);
            g.fillRoundedRectangle (b, rad);
        }
        g.setColour (edge);
        g.drawRoundedRectangle (b.reduced (0.5f), rad, 1.0f);

        const int   n    = numSegments();
        const float segW = b.getWidth() / (float) n;
        auto segRect = [&] (int i) { return b.withX (b.getX() + segW * (float) i).withWidth (segW); };
        auto active  = segRect (index).reduced (2.0f);

        // Active segment = a lit inset: soft cyan glow, wash, crisp border.
        for (int i = 3; i >= 1; --i)
        {
            g.setColour (cyan.withAlpha (0.05f * (float) i / 3.0f));
            g.fillRoundedRectangle (active.expanded ((float) i * 0.8f), rad - 1.0f);
        }
        g.setColour (cyan.withAlpha (0.14f));
        g.fillRoundedRectangle (active, rad - 1.0f);
        g.setColour (cyan);
        g.drawRoundedRectangle (active.reduced (0.5f), rad - 1.0f, 1.2f);

        // Size the face to the SEGMENT, not to a fixed number. A switch that is narrow,
        // or one holding a long word like SMART, would otherwise print text wider than
        // the lit outline behind it — the label spilling past its own highlight is
        // exactly what makes a switch look broken.
        float fh = juce::jlimit (6.5f, 9.5f, juce::jmin (segW * 0.26f, b.getHeight() * 0.55f));

        auto faceAt = [this] (float h)
        {
            return drukFace ? juce::Font (juce::FontOptions (drukFace).withHeight (h))
                            : juce::Font (juce::FontOptions().withHeight (h + 0.5f)).boldened();
        };

        // Then shrink further if the longest label still would not fit inside the
        // highlight, so every segment is measured against what it actually has to hold.
        const float avail = segW - 10.0f;
        for (int guard = 0; guard < 6; ++guard)
        {
            float widest = 0.0f;
            auto f = faceAt (fh);
            for (int i = 0; i < n; ++i)
                widest = juce::jmax (widest, juce::GlyphArrangement::getStringWidth (f, labels[i]));
            if (widest <= avail || fh <= 6.5f) break;
            fh -= 0.5f;
        }

        g.setFont (faceAt (fh));
        for (int i = 0; i < n; ++i)
        {
            g.setColour (i == index ? cyan : dimTxt);
            g.drawText (labels[i], segRect (i), juce::Justification::centred);
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        const int clicked = segmentAt (e.position.x);
        if (clicked >= 0 && clicked != index)
        {
            index = clicked;
            repaint();
            if (onChange) onChange (index);
        }
    }

private:
    // Which segment a horizontal position falls in, or -1 if the size is not known yet.
    int segmentAt (float x) const
    {
        if (getWidth() <= 0) return -1;
        return juce::jlimit (0, numSegments() - 1,
                             (int) (x * (float) numSegments() / (float) getWidth()));
    }

    juce::StringArray labels { "A", "B" };
    juce::StringArray segTips;
    int index { 0 };
};

//==============================================================================
// A centred, animated "links" card: dims the whole editor, slides+fades in a small
// panel with TELEGRAM / SPOTIFY / YANDEX MUSIC buttons in the Druk face, and a close
// (×). Runs its own ease timer for a smooth open/close.
// Card height is sized to the button count — adding or removing one means adjusting
// `ch` below, or the last button lands outside the panel.
class LinksOverlay : public juce::Component,
                     private juce::Timer
{
public:
    juce::Typeface::Ptr drukFace;   // set by the editor (NOT static — avoids DLL-unload hang)

    LinksOverlay() { setInterceptsMouseClicks (true, true); setVisible (false); }
    ~LinksOverlay() override { stopTimer(); }   // no late callback during teardown

    void show()
    {
        opening = true; setVisible (true); toFront (true);
        if (! isTimerRunning()) startTimerHz (60);
    }
    void hide() { opening = false; if (! isTimerRunning()) startTimerHz (60); }

    void paint (juce::Graphics& g) override
    {
        // dim backdrop (fades with anim)
        g.fillAll (juce::Colours::black.withAlpha (0.55f * anim));

        // card, scaling up from 0.92 as it fades in
        auto full = getLocalBounds().toFloat();
        // +46 over the two-button card: one more 38 px button plus its 8 px gap.
        const float cw = 250.0f, ch = 214.0f;
        const float scale = 0.92f + 0.08f * anim;
        juce::Rectangle<float> card (0, 0, cw * scale, ch * scale);
        card.setCentre (full.getCentreX(), full.getCentreY());
        cardRect = card;

        juce::Graphics::ScopedSaveState ss (g);
        g.setOpacity (anim);

        // soft shadow + panel
        juce::Path shape; shape.addRoundedRectangle (card, 12.0f);
        melatonin::DropShadow { juce::Colours::black.withAlpha (0.6f), 24, { 0, 8 } }.render (g, shape);
        g.setColour (juce::Colour (0xff1d222a));
        g.fillRoundedRectangle (card, 12.0f);
        g.setColour (juce::Colour (0xff4dd8ef).withAlpha (0.35f));
        g.drawRoundedRectangle (card.reduced (0.5f), 12.0f, 1.2f);

        auto area = card.reduced (18.0f);

        // title + plugin version (small, under the name)
        g.setFont (drk (13.0f));
        g.setColour (juce::Colour (0xffe6f4f8));
        g.drawText (juce::String::fromUTF8 ("ACRONA"), area.removeFromTop (20.0f), juce::Justification::centred);
        g.setFont (drk (7.5f));
        g.setColour (juce::Colour (0xff77858f));
        g.drawText (kPluginVersion, area.removeFromTop (11.0f), juce::Justification::centred);
        area.removeFromTop (6.0f);

        // three link buttons
        telegramRect = area.removeFromTop (38.0f);
        area.removeFromTop (8.0f);
        spotifyRect  = area.removeFromTop (38.0f);
        area.removeFromTop (8.0f);
        yandexRect   = area.removeFromTop (38.0f);
        drawLinkButton (g, telegramRect, "TELEGRAM",     hovTelegram);
        drawLinkButton (g, spotifyRect,  "SPOTIFY",      hovSpotify);
        drawLinkButton (g, yandexRect,   "YANDEX MUSIC", hovYandex);

        // close ×
        closeRect = juce::Rectangle<float> (card.getRight() - 26.0f, card.getY() + 8.0f, 18.0f, 18.0f);
        g.setColour (juce::Colour (hovClose ? 0xff4dd8ef : 0xff77858f));
        g.setFont (drk (14.0f));
        g.drawText (juce::String::fromUTF8 ("\xC3\x97"), closeRect, juce::Justification::centred);
    }

    void mouseMove (const juce::MouseEvent& e) override
    {
        auto p = e.position;
        hovTelegram = telegramRect.contains (p);
        hovSpotify  = spotifyRect.contains (p);
        hovYandex   = yandexRect.contains (p);
        hovClose    = closeRect.contains (p);
        setMouseCursor ((hovTelegram || hovSpotify || hovYandex || hovClose)
                        ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
        repaint();
    }
    void mouseExit (const juce::MouseEvent&) override
    {
        hovTelegram = hovSpotify = hovYandex = hovClose = false; repaint();
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        auto p = e.position;
        if (telegramRect.contains (p)) juce::URL ("https://t.me/acronalife").launchInDefaultBrowser();
        else if (spotifyRect.contains (p)) juce::URL ("https://open.spotify.com/artist/7GqbuqTbfmOOr0QVFw1wj7").launchInDefaultBrowser();
        else if (yandexRect.contains (p)) juce::URL ("https://music.yandex.ru/artist/5852436").launchInDefaultBrowser();
        else if (closeRect.contains (p) || ! cardRect.contains (p)) hide();   // click card=stay, outside/×=close
    }

private:
    juce::Font drk (float h) { return drukFace ? juce::Font (juce::FontOptions (drukFace).withHeight (h))
                                               : juce::Font (juce::FontOptions().withHeight (h)); }

    void drawLinkButton (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& t, bool hov)
    {
        g.setColour (juce::Colour (hov ? 0xff273744 : 0xff141a21));
        g.fillRoundedRectangle (r, 7.0f);
        g.setColour (juce::Colour (0xff4dd8ef).withAlpha (hov ? 0.9f : 0.35f));
        g.drawRoundedRectangle (r.reduced (0.5f), 7.0f, 1.2f);
        g.setColour (juce::Colour (hov ? 0xff9be9f7 : 0xffcfe9f0));
        g.setFont (drk (11.0f));
        g.drawText (t, r, juce::Justification::centred);
    }

    void timerCallback() override
    {
        const float target = opening ? 1.0f : 0.0f;
        anim += (target - anim) * 0.22f;
        if (std::abs (anim - target) < 0.01f)
        {
            anim = target; stopTimer();
            if (! opening) setVisible (false);
        }
        repaint();
    }

    float anim { 0.0f };
    bool  opening { false };
    bool  hovTelegram { false }, hovSpotify { false }, hovYandex { false }, hovClose { false };
    juce::Rectangle<float> cardRect, telegramRect, spotifyRect, yandexRect, closeRect;
};

//==============================================================================
// Clickable "by ACRONA" byline: brightens on hover, opens the LinksOverlay on click.
class LinkByline : public juce::Component
{
public:
    juce::Typeface::Ptr drukFace;   // set by the editor (NOT static — avoids DLL-unload hang)
    std::function<void()> onClick;
    juce::String text { juce::String::fromUTF8 ("by ACRONA") };

    void paint (juce::Graphics& g) override
    {
        const bool hot = hotText;
        g.setFont (face());
        g.setColour (hot ? juce::Colour (0xff4dd8ef) : juce::Colour (0xff77858f));
        g.drawText (text, getLocalBounds(), juce::Justification::centred);
    }

    // The component spans the whole header so the text can be centred on the panel —
    // but only the WORDS should be clickable. Without this the entire 740px strip acted
    // as a button, so clicks nowhere near the text opened the links card.
    bool hitTest (int x, int y) override { return textArea().contains (x, y); }

    void mouseEnter (const juce::MouseEvent&) override
    {
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
        hotText = true;  repaint();
    }
    void mouseExit  (const juce::MouseEvent&) override { hotText = false; repaint(); }
    void mouseUp    (const juce::MouseEvent&) override { if (onClick) onClick(); }

private:
    juce::Font face() const
    {
        return drukFace ? juce::Font (juce::FontOptions (drukFace).withHeight (10.0f))
                        : juce::Font (juce::FontOptions().withHeight (10.0f));
    }
    juce::Rectangle<int> textArea() const
    {
        const int w = juce::jmax (40, (int) std::ceil (juce::GlyphArrangement::getStringWidth (face(), text))) + 8;
        return getLocalBounds().withSizeKeepingCentre (w, getHeight());
    }
    bool hotText { false };
};

//==============================================================================
class HouseCompAudioProcessorEditor : public juce::AudioProcessorEditor,
                                      private juce::Timer
{
public:
    explicit HouseCompAudioProcessorEditor (HouseCompAudioProcessor&);
    ~HouseCompAudioProcessorEditor() override
    {
        stopTimer();               // stop our 30Hz animation timer FIRST (no late callback)
        setLookAndFeel (nullptr);  // detach LnF before members are destroyed
    }

    void paint (juce::Graphics&) override;
    void paintContent (juce::Graphics&);   // the real painting, done on the base grid
    void resized() override;

    // (The old setBoundsConstrained() was removed: JUCE never called it, so the
    //  aspect ratio was never actually enforced — that's what let the window be
    //  dragged out of ratio and show black bars. The constrainer now does it.)
    void layoutControls();   // lays out all children ONCE in base 740x500 coords
    bool laidOut { false };  // guard so the base layout runs only once
    float appliedScale { 0.0f };  // last scale we pushed via setTransform()

    // ALL controls live inside this, and THIS is what gets scaled — never the editor.
    //
    // Transforming the editor itself is what made resizing unstable: setTransform() on a
    // top-level editor calls resized() straight back, and the size then had to be read
    // back out through the very transform that had just been applied, so the input
    // depended on its own output. It behaved as a fixed point — usually nothing moved,
    // and when the host's bounds did slip through first, a whole drag's worth of change
    // arrived in one jump ("grabbed the corner and it flew").
    //
    // Scaling a CHILD has no such loop: the editor's bounds are whatever the host says,
    // full stop, and the child is simply drawn scaled. paint() already draws on the base
    // 740x500 grid, so it only needs the same factor applied to its graphics context.
    // It also does the PAINTING. Drawing the background on the editor while the controls
    // lived in here meant the two were in different coordinate systems: the editor was
    // window-sized with a scaled graphics context, the container was the 740x500 grid.
    // Clip paths (the rounded centre card) were then built in one space and applied in
    // the other, and the orb came out sliced into vertical bands. Painting inside the
    // container puts every coordinate back on the same grid.
    struct Content : juce::Component
    {
        HouseCompAudioProcessorEditor* owner { nullptr };
        void paint (juce::Graphics& g) override;
    };
    Content content;
    // Re-rendering the cached chrome is expensive and pointless mid-drag: it would run
    // on every pixel of mouse travel. Deferred to a one-shot timer that only fires once
    // the drag settles, so the resize itself stays smooth and the art still ends sharp.
    void scheduleChromeRender();
    struct ChromeTimer : juce::Timer
    {
        HouseCompAudioProcessorEditor* owner { nullptr };
        void timerCallback() override;
    };
    ChromeTimer chromeTimer;

private:
    void timerCallback() override;
    void drawBackgroundGif (juce::Graphics&);
    void drawGrGraph (juce::Graphics&);
    void drawMeter (juce::Graphics&, juce::Rectangle<int>, float valueDb,
                    const juce::String& caption, float topDb, float botDb,
                    float peakDb, bool over);
    void updateBandEnablement();

    // --- Localization (RU / ENG) -------------------------------------------
    // Seeded from the SYSTEM language on first ever run, then whatever the user picked.
    // The choice lives in a small settings file next to the plugin's other user data,
    // NOT in the plugin state: language is a property of the person, not of the project,
    // and storing it per-project would hand a Russian session to an English collaborator.
    bool ru { false };                // true = Russian, false = English
    void applyLanguage();             // (re)assign all tooltips for the current lang
    juce::TextButton langButton  { "RU" };  // corner toggle: RU / EN

    // Where the remembered choice is kept. A plain file rather than the registry so it
    // works the same for a hand-copied .vst3 with no installer involved.
    static juce::File languageFile();
    static bool  loadLanguageChoice();     // system language when nothing is saved yet
    static void  saveLanguageChoice (bool russian);

    // Window size is remembered the same way and for the same reason: it belongs to the
    // person and their screen, not to the project. Stored as WIDTH only — the aspect
    // ratio is fixed, so height follows and cannot drift out of step.
    static juce::File windowSizeFile();
    static int   loadWindowWidth();        // 0 = nothing saved, use the default
    static void  saveWindowSize (int width);

    HouseCompAudioProcessor& proc;

    // A LookAndFeel whose default font is the embedded Cyrillic-capable font, so
    // tooltips and labels render Russian correctly (no mojibake).
    struct CyrLookAndFeel : juce::LookAndFeel_V4
    {
        juce::Typeface::Ptr cyr;
        juce::Typeface::Ptr druk;   // wide display face (Druk Wide) — latin headings/values
        float animPhase { 0.0f };   // live phase (updated each timer tick) for arc pulse
        juce::Font getPopupMenuFont() override            { return withDruk (11.0f); }  // -15%
        // Buttons (toggles + text buttons) and combo boxes render in Druk Wide too.
        juce::Font getTextButtonFont (juce::TextButton&, int h) override
        { return withDruk (juce::jmin (13.0f, (float) h * 0.5f)); }
        juce::Font getComboBoxFont (juce::ComboBox&) override { return withDruk (9.5f); }

        // ComboBox drawn as a recessed well (matches switches/toggles/meters depth).
        void drawComboBox (juce::Graphics& g, int width, int height, bool,
                           int, int, int, int, juce::ComboBox& box) override
        {
            juce::Rectangle<float> b (0.0f, 0.0f, (float) width, (float) height);
            g.setColour (juce::Colour (0xff141a21));
            g.fillRoundedRectangle (b, 4.0f);
            juce::ColourGradient inner (juce::Colours::black.withAlpha (0.32f), b.getX(), b.getY(),
                                        juce::Colours::transparentBlack, b.getX(), b.getCentreY(), false);
            g.setGradientFill (inner);
            g.fillRoundedRectangle (b, 4.0f);
            g.setColour (juce::Colour (0xff343c47));
            g.drawRoundedRectangle (b.reduced (0.5f), 4.0f, 1.0f);

            // arrow (cyan chevron) on the right
            const float cx = width - 12.0f, cy = height * 0.5f, s = 3.2f;
            juce::Path arr;
            arr.startNewSubPath (cx - s, cy - s * 0.5f);
            arr.lineTo (cx,     cy + s * 0.6f);
            arr.lineTo (cx + s, cy - s * 0.5f);
            g.setColour (box.findColour (juce::ComboBox::arrowColourId));
            g.strokePath (arr, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved,
                                                     juce::PathStrokeType::rounded));
        }

        // Slider value read-outs (20.0 / 6.0 / 27.1) in Druk Wide too.
        juce::Label* createSliderTextBox (juce::Slider& s) override
        {
            auto* l = juce::LookAndFeel_V4::createSliderTextBox (s);
            l->setComponentID ("drukVal");   // value digits: Druk Wide, fixed 25%-smaller size
            return l;
        }
        juce::Font getLabelFont (juce::Label& l) override
        {
            // The value read-outs (28.0 / -3.0) are the INTERNAL label of a Slider —
            // catch them by parent type (createSliderTextBox isn't always called, so
            // an ID tag can be missed). Druk Wide at a fixed ~25%-smaller size.
            if (druk && dynamic_cast<juce::Slider*> (l.getParentComponent()) != nullptr)
                return juce::Font (juce::FontOptions (druk).withHeight (11.25f));
            // ComboBox internal label ("32x") — created lazily, catch by parent.
            const bool inCombo = (dynamic_cast<juce::ComboBox*> (l.getParentComponent()) != nullptr);
            if (druk && (l.getComponentID() == "druk" || inCombo))
                return juce::Font (juce::FontOptions (druk).withHeight (inCombo ? 9.5f : l.getFont().getHeight()));
            return withCyr (l.getFont().getHeight());
        }
        juce::Font withCyr (float h)
        {
            if (cyr) return juce::Font (juce::FontOptions (cyr).withHeight (h));
            return juce::Font (juce::FontOptions().withHeight (h));
        }
        juce::Font withDruk (float h)
        {
            if (druk) return juce::Font (juce::FontOptions (druk).withHeight (h));
            return withCyr (h);
        }

        // Toggle buttons (ALL MIX / BYPASS / LOOKAHEAD) — Druk Wide text next to a tick.
        void drawToggleButton (juce::Graphics& g, juce::ToggleButton& b,
                               bool /*over*/, bool /*down*/) override
        {
            const float boxSz = juce::jmin (18.0f, (float) b.getHeight() * 0.7f);
            juce::Rectangle<float> box (2.0f, (b.getHeight() - boxSz) * 0.5f, boxSz, boxSz);

            // Recessed well: dark fill + top inner-shadow so the box looks sunk in.
            g.setColour (juce::Colour (0xff141a21));
            g.fillRoundedRectangle (box, 4.0f);
            juce::ColourGradient inner (juce::Colours::black.withAlpha (0.35f), box.getX(), box.getY(),
                                        juce::Colours::transparentBlack, box.getX(), box.getCentreY(), false);
            g.setGradientFill (inner);
            g.fillRoundedRectangle (box, 4.0f);
            g.setColour (juce::Colour (0xff343c47));
            g.drawRoundedRectangle (box.reduced (0.5f), 4.0f, 1.2f);

            if (b.getToggleState())
            {
                // lit cyan chip with a soft glow — sits inside the well
                g.setColour (juce::Colour (0xff4dd8ef).withAlpha (0.25f));
                g.fillRoundedRectangle (box.reduced (2.0f), 2.0f);
                g.setColour (juce::Colour (0xff4dd8ef));
                g.fillRoundedRectangle (box.reduced (4.0f), 1.5f);
                g.setColour (juce::Colours::white.withAlpha (0.5f));
                g.fillRoundedRectangle (box.reduced (4.0f).removeFromTop (boxSz * 0.28f).reduced (1.0f, 0.0f), 1.0f);
            }

            g.setColour (b.findColour (juce::ToggleButton::textColourId));
            g.setFont (withDruk (8.25f));   // -25%
            auto txt = b.getLocalBounds().toFloat().withTrimmedLeft (boxSz + 8.0f);
            g.drawText (b.getButtonText(), txt, juce::Justification::centredLeft);
        }

        // Neon knob: thin dark track ring, glowing cyan value arc (drawn a few
        // times with growing width / low alpha to fake a bloom), and a round thumb.
        void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h,
                               float pos, float startAngle, float endAngle,
                               juce::Slider& s) override
        {
            const juce::Colour cyan { 0xff4dd8ef };
            const juce::Colour trackCol { 0xff313943 };  // -15%

            // Hover feedback: use the SMOOTHED hover amount from the WheelSlider so the
            // value arc eases up on enter and back down on exit (no snap).
            const bool  hot   = s.isMouseOverOrDragging();
            // Smoothed hover is stashed on the slider's property by WheelSlider — read
            // it (no RTTI). Default 0.0 so the arc eases up from calm (never snaps to
            // full on the first hovered frame, which caused a flicker).
            const float hover = (float) s.getProperties().getWithDefault ("hoverAmt", 0.0);

            auto bounds = juce::Rectangle<int> (x, y, w, h).toFloat().reduced (3.0f);
            const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
            const float cx = bounds.getCentreX(), cy = bounds.getCentreY();
            const float thick = juce::jmax (2.7f, radius * 0.1424f);  // 11% thinner
            const float baseR = radius - thick * 0.5f;      // track sits here (fixed)
            const float arcR  = baseR;                       // arc stays on the track (no drift)
            const float angle = startAngle + pos * (endAngle - startAngle);

            // ---- KNOB BODY: a raised, glassy disc so the knob reads as a physical
            // object, not a flat gap. Drop shadow underneath, a radial "domed" fill
            // (lighter top-left → darker bottom-right), a crisp rim, and a soft top
            // gloss. This is what gives the grey centre life. ----
            const float discR = baseR - thick * 0.5f - 2.0f;  // fixed (only the arc grows)
            if (discR > 4.0f && s.getComponentID() != "noDisc"
                             && s.getComponentID() != "amount")   // skip over-orb knobs
            {
                juce::Rectangle<float> disc (cx - discR, cy - discR, discR * 2.0f, discR * 2.0f);

                // drop shadow — PERSISTENT instance so melatonin's blur cache survives
                // across frames (a throwaway would re-blur every paint).
                juce::Path dp; dp.addEllipse (disc);
                melatonin::DropShadow { juce::Colours::black.withAlpha (0.55f), 9, { 0, 3 } }.render (g, dp);

                // domed radial fill: highlight offset toward the top-left
                juce::ColourGradient dome (juce::Colour (0xff2b333d), cx - discR * 0.45f, cy - discR * 0.5f,
                                           juce::Colour (0xff141920), cx + discR * 0.6f,  cy + discR * 0.7f, true);
                dome.addColour (0.55, juce::Colour (0xff1e242d));
                g.setGradientFill (dome);
                g.fillEllipse (disc);

                // inner shading ring near the rim for extra roundness
                g.setColour (juce::Colours::black.withAlpha (0.22f));
                g.drawEllipse (disc.reduced (1.0f), 1.6f);

                // crisp rim + a faint cyan life-tint that lifts on hover
                g.setColour (cyan.withAlpha (0.10f + 0.18f * hover));
                g.drawEllipse (disc.reduced (0.5f), 1.0f);

                // top gloss: a soft bright crescent along the upper edge (glass)
                auto gloss = disc.reduced (discR * 0.16f).translated (0.0f, -discR * 0.16f);
                juce::ColourGradient gl (juce::Colours::white.withAlpha (0.10f), cx, gloss.getY(),
                                         juce::Colours::transparentBlack, cx, gloss.getCentreY(), false);
                g.setGradientFill (gl);
                g.fillEllipse (gloss);
            }

            // ---- Track = a slim unfilled arc (thin, quiet). Stays at baseR. ----
            const float trackW = juce::jmax (2.0f, thick * 0.62f);
            juce::Path track;
            track.addCentredArc (cx, cy, baseR, baseR, 0.0f, startAngle, endAngle, true);
            g.setColour (trackCol);
            g.strokePath (track, juce::PathStrokeType (trackW, juce::PathStrokeType::curved,
                                                       juce::PathStrokeType::rounded));

            // ---- Value arc = a REFINED thin line with a gradient ALONG its length:
            // near-transparent at the tail, full colour at the head. No fat tube, no
            // white core, no bloom — just an elegant fading stroke. ----
            if (pos > 0.001f)
            {
                // Line grows WIDER on hover (stays on the track — no sideways drift).
                const float lineW = juce::jmax (2.0f, thick * 0.62f) + 3.0f * hover;
                const int   segs  = juce::jmax (14, (int) (pos * 24.0f));   // fewer strokes, same look
                const juce::Colour base = hot ? cyan.brighter (0.12f) : cyan;
                for (int i = 0; i < segs; ++i)
                {
                    const float t0 = (float) i       / (float) segs;   // 0 = tail .. 1 = head
                    const float t1 = (float) (i + 1) / (float) segs;
                    const float a0 = startAngle + t0 * (angle - startAngle);
                    const float a1 = startAngle + t1 * (angle - startAngle);
                    juce::Path seg;
                    seg.addCentredArc (cx, cy, arcR, arcR, 0.0f, a0, a1, true);

                    // alpha eases from ~0.12 at the tail to 1.0 at the head (quadratic)
                    const float alpha = 0.12f + 0.88f * (t0 * t0);
                    g.setColour (base.withAlpha (alpha));
                    g.strokePath (seg, juce::PathStrokeType (lineW, juce::PathStrokeType::curved,
                                                             juce::PathStrokeType::rounded));
                }
            }

            // Thumb: a rounded 3D bead on the arc — soft drop shadow, a shaded body
            // (bright top-left, darker bottom-right) and a small specular highlight,
            // so it reads as a physical glass pearl instead of a flat white dot.
            const float tx = cx + arcR * std::cos (angle - juce::MathConstants<float>::halfPi);
            const float ty = cy + arcR * std::sin (angle - juce::MathConstants<float>::halfPi);
            const float tr = juce::jmax (2.5f, thick * 0.5f);   // small, refined
            juce::Rectangle<float> bead (tx - tr, ty - tr, tr * 2.0f, tr * 2.0f);

            // faint halo only on hover
            if (hover > 0.0f)
            {
                g.setColour (cyan.withAlpha (0.20f * hover));
                g.fillEllipse (bead.expanded (tr * 0.8f));
            }
            // clean white dot with a thin cyan ring
            g.setColour (juce::Colours::white);
            g.fillEllipse (bead);
            g.setColour (cyan.withAlpha (0.6f));
            g.drawEllipse (bead.reduced (0.3f), 1.0f);

            juce::ignoreUnused (s);
        }
    };
    CyrLookAndFeel cyrLnf;

    juce::TooltipWindow tooltipWindow { this, 600 };  // hover-help popups
    juce::Rectangle<int> grGraphBounds;
    juce::Rectangle<int> rmsMeterBounds, lufsMeterBounds;
    juce::Rectangle<int> bandZone;   // area covering the 4 band knobs (for fade veil)
    juce::Rectangle<int> bandCards[4]; // card panel behind each band knob
    juce::Rectangle<int> ioCard;       // card panel behind INPUT + OUTPUT
    juce::Rectangle<int> centreZone; // middle column (gif is clipped to this)

    WheelSlider amountKnob;            // big center knob
    WheelSlider inGainKnob;   // input gain
    WheelSlider outGainKnob;  // OUTPUT trim (attenuation only, <= 0 dB)
    WheelSlider atkKnob, relKnob;      // attack / release slew
    WheelSlider charKnob;              // CHARACTER (bipolar tone/feel)
    WheelSlider bandKnob[4];           // 4-band comp amounts (All Mix Comp)

    // Plain toggles that stay as-is.
    juce::ToggleButton bypassButton   { "BYPASS" };
    juce::ToggleButton allMixButton   { "ALL MIX" };
    juce::ToggleButton lookaheadButton{ "LOOKAHEAD" };

    // Segmented pick-one switches (bound to bool params). left=false / right=true.
    SegmentedSwitch clipSwitch;   // CLIP (ADAA1) | HI-Q (ADAA2) | MASK -> clipmode
    SegmentedSwitch limSwitch;    // SMART | IRC IV                 -> irc4  (right=irc4)
    // NOTE: the AUTO | LUFS switch is gone — LUFS make-up is now always on, so there
    // was nothing left to choose. The parameter still exists and is forced true.
    // A single letter that lives INSIDE the RELEASE knob. It glows and grows a little
    // under the pointer so it reads as pressable without needing a button frame — a
    // frame that size would fight the knob it sits on.
    // (juce::Button already is a SettableTooltipClient, so setTooltip works as-is.)
    class InlineAutoButton : public juce::Button,
                             private juce::Timer
    {
    public:
        InlineAutoButton() : juce::Button ({})
        {
            setClickingTogglesState (true);
            // It sits ON the knob's face, so keep its hit area to the word itself —
            // otherwise it would swallow drags meant for the knob underneath.
            setMouseCursor (juce::MouseCursor::PointingHandCursor);
        }
        ~InlineAutoButton() override { stopTimer(); }

        juce::Typeface::Ptr drukFace;

        void paintButton (juce::Graphics& g, bool, bool) override
        {
            const juce::Colour cyan { 0xff4dd8ef };
            auto b = getLocalBounds().toFloat();

            const bool on = getToggleState();
            const float lit = juce::jmax (hover, on ? 1.0f : 0.0f);
            const float h = 7.5f + 1.2f * lit;          // grows under the pointer

            if (lit > 0.01f)                            // soft halo, no hard frame
            {
                g.setColour (cyan.withAlpha (0.13f * lit));
                g.fillRoundedRectangle (b.reduced (1.0f), 4.0f);
            }

            g.setFont (drukFace ? juce::Font (juce::FontOptions (drukFace).withHeight (h))
                                : juce::Font (juce::FontOptions().withHeight (h)).boldened());
            g.setColour (on ? cyan
                            : juce::Colour (0xff8796a1).interpolatedWith (cyan, hover));
            g.drawText ("AUTO", b, juce::Justification::centred);
        }

        void mouseEnter (const juce::MouseEvent& e) override { juce::Button::mouseEnter (e); startTimerHz (60); }
        void mouseExit  (const juce::MouseEvent& e) override { juce::Button::mouseExit  (e); startTimerHz (60); }

    private:
        void timerCallback() override
        {
            const float target = isMouseOver (true) ? 1.0f : 0.0f;
            const float next = hover + (target - hover) * 0.18f;
            hover = (std::abs (next - hover) > 0.005f) ? next : target;
            repaint();
            if (hover == target) stopTimer();
        }
        float hover { 0.0f };
    };
    InlineAutoButton relAutoButton;   // sits inside the RELEASE knob
    SegmentedSwitch engineSwitch; // CLASSIC | MODERN | FUTURE | FUTURE 2 -> enginemode
    SegmentedSwitch relTypeSwitch;// BEAT | SMART (FUTURE only)     -> relsmart
    // FREE | BOND retired from the panel. The 'bond' parameter and the engine code that
    // reads it are still there (pinned off); re-adding a switch here brings it back.

    WheelSlider        clipShapeKnob;   // clipper hardness morph (hard <-> soft)
    juce::Label        clipShapeLabel { {}, "CLIP SHAPE" };
    WheelSlider        upKnob;          // FUTURE only: lift quiet tones (0 = off)
    juce::Label        upLabel { {}, "UPWARD" };
    WheelSlider        relNoteKnob;     // FUTURE only: RELEASE as a note value
    WheelSlider        atkCycKnob;      // FUTURE only: ATTACK in wave cycles
    juce::ComboBox     osBox;
    juce::Label        osLabel { {}, "OVERSAMPLE" };

    // A/B compare: two snapshot slots of all parameters, with an optional loudness
    // MATCH so switching A/B compares CHARACTER, not just "louder = better".
    juce::TextButton abAButton    { "A" };
    juce::TextButton abBButton    { "B" };
    juce::TextButton abMatchButton{ "=" };   // between A and B: loudness-match on/off
    void captureToSlot (int slot);   // save current params into slot A(0)/B(1)
    void recallSlot (int slot);      // load slot into the live params
    void switchToSlot (int slot);    // save current -> active slot, then recall target
    juce::ValueTree abSlot[2];       // stored parameter snapshots
    int   abActive { 0 };            // which slot is live (0=A, 1=B)
    float abLoudness[2] { -100.0f, -100.0f };  // last measured LUFS of each slot
    bool  abMatchOn { false };       // loudness-match engaged?
    // After switching slots we must RE-MEASURE the new slot before trimming it —
    // its stored loudness is stale (it was edited since). Counts down in the timer.
    int   measureSlotCountdown { 0 };

    // CHARACTER is remembered PER ENGINE: CLASSIC and MODERN each keep their own
    // setting, so switching back restores what you last dialled in for that mode.
    // First time into MODERN it starts at -0.24 (its natural home), CLASSIC at -0.40.
    // Indexed by the enginemode PARAMETER value, not by segment: [0] CLASSIC [1] MODERN
    // [2] FUTURE [3] the trial alias of FUTURE, kept so a project saved during the trial
    // still finds a slot rather than indexing past the end.
    float charMemory[4] { -0.40f, -0.24f, -0.24f, -0.24f };
    bool  charMemInit[4] { false, false, false, false };  // set by the user in this mode yet?
    int   lastEngineIndex { 2 };                     // default engine is FUTURE
    // Tracks whether the panel is currently dressed for FUTURE, so the caption/tint
    // swap only runs on an actual change instead of every timer tick.
    int   futureUiShown { -1 };
    int   relAutoShown  { -1 };   // is the RELEASE knob currently handed over to AUTO?
    int   clipShapeDimmed { -1 }; // CLIP SHAPE is inert in ACR (tracks the clipper switch)

    juce::Label amountLabel { {}, "COMPRESSION" };   // was AMOUNT (renamed per request)
    juce::Label gainLabel   { {}, "INPUT" };
    juce::Label outLabel    { {}, "OUTPUT" };   // cut-only behaviour is stated in the tooltip
    juce::Label atkLabel    { {}, "ATTACK" };
    juce::Label relLabel    { {}, "RELEASE" };
    juce::Label charLabel   { {}, "CHARACTER" };
    juce::Label titleLabel  { {}, "HOUZY" };
    LinkByline    bylineLabel;   // clickable "by ACRONA"
    LinksOverlay  linksOverlay;  // centred animated card with the streaming links
    juce::Label bandLabel[4] {
        { {}, "LOW" }, { {}, "LO-MID" }, { {}, "HI-MID" }, { {}, "HIGH" } };

    float meterGrDb { 0.0f };
    float compGrShownDb { 0.0f }, limGrShownDb { 0.0f }, clipActShown { 0.0f };
    float rmsShownDb { -60.0f }, lufsShownDb { -60.0f };
    // Peak-hold: highest reading seen, decays slowly; latches an OVER lamp >-0.3 dBFS.
    float rmsPeakDb { -60.0f }, lufsPeakDb { -60.0f };
    bool  rmsOver { false }, lufsOver { false };
    int   rmsOverHold { 0 }, lufsOverHold { 0 };
    float bandFade { 0.0f };   // 0..1 smooth fade for the band knobs (All Mix)
    float ringGlow { 0.0f };   // 0..1 smoothed glow amount for the GR ring
    float animPhase { 0.0f };  // ever-advancing phase (radians) for orbiting sparks
    float clipFlash { 0.0f };  // 0..1 decaying flash when the clipper slices

    // --- Particle field around the orb (idea borrowed from generative-art flow
    // fields + particle systems). Each particle has a STABLE seeded character
    // (orbit radius, base speed, size) so the swarm looks organic, not mechanical.
    // Behaviour is driven by the signal: compression speeds the flow, clipper
    // transients spawn bright bursts. This is the ONE orchestrated motion moment.
    struct Particle
    {
        float ang        { 0.0f };  // current angle on the orbit
        float radius     { 1.0f };  // orbit radius (seeded)
        float speed      { 0.0f };  // base angular speed (seeded, signed = direction)
        float size       { 1.0f };  // dot radius (seeded)
        float life       { 1.0f };  // 0..1; burst particles fade, ambient ones stay ~1
        float fade       { 1.0f };  // 0..1 fade-IN envelope (burst grows in, avoids pop)
        float wobbleSeed { 0.0f };  // phase offset for radius wobble
        bool  burst      { false }; // true = spawned by a clip transient (fades out)
    };
    static constexpr int kMaxParticles = 40;
    std::vector<Particle> particles;
    juce::Random          partRng;   // seeded RNG for stable particle characters
    float prevClipAct { 0.0f };      // to detect a rising clip edge (spawn bursts)
    float partVisible { 0.0f };      // 0..1 global fade: particles only show while
                                     // compressing, ramping 0 (<5 dB) → 1 (~60 dB).

    // Animated background: pre-sliced GIF frames, advanced one per timer tick.
    juce::Array<juce::Image> gifFrames;
    int   gifFrame { 0 };

    // Cached STATIC chrome (background gradient + header + all card panels with their
    // drop-shadows). These never change once laid out, so they are rendered ONCE into
    // this image and just blitted each frame — removing ~7 shadow blurs + ~24 rounded
    // rects from every animated repaint (the big editor-CPU win).
    juce::Image chromeCache;
    float chromeScale { 1.0f };   // scale the cache was rendered at (HiDPI sharpness)
    void renderChrome();          // (re)builds chromeCache for the current scale

    using SliderAttach = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttach = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using ComboAttach  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    std::unique_ptr<SliderAttach> amountAttach, inGainAttach, outGainAttach, atkAttach, relAttach, charAttach, clipShapeAttach, upAttach;
    // RELEASE is two knobs stacked in the same place, each permanently bound to its own
    // parameter: dB/ms for MODERN, a note value for FUTURE. Only one is ever visible.
    // Swapping a single knob's attachment at runtime is what broke the plugin before.
    std::unique_ptr<SliderAttach> relNoteAttach, atkCycAttach;
    std::unique_ptr<SliderAttach> bandAttach[4];
    std::unique_ptr<ButtonAttach> bypassAttach, allMixAttach, lookaheadAttach, relAutoAttach;
    std::unique_ptr<ComboAttach>  osAttach;

    // Helper: wire a SegmentedSwitch to a bool APVTS param both ways.
    void bindSwitch (SegmentedSwitch& sw, const juce::String& paramID, int firstValue = 0);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HouseCompAudioProcessorEditor)
};
