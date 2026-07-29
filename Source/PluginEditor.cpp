#include "PluginEditor.h"
#include "BinaryData.h"
#include <algorithm>   // std::remove_if for the particle swarm

//==============================================================================
// Palette — cool neon-cyan on near-black (the single, dark theme).
namespace pal
{
    const juce::Colour bgTop      { 0xff181c23 };
    const juce::Colour bgBot      { 0xff15181e };
    const juce::Colour panel      { 0xff1d222a };
    const juce::Colour panelEdge  { 0xff282f36 };
    const juce::Colour cyan       { 0xff4dd8ef };
    const juce::Colour cyanDim    { 0xff246d7c };
    const juce::Colour track      { 0xff252b33 };
    const juce::Colour text       { 0xffe6f4f8 };
    const juce::Colour textDim    { 0xff77858f };
    const juce::Colour headerBg   { 0xff161a21 };
    const juce::Colour amber      { 0xffe0a24a };
}

//==============================================================================
HouseCompAudioProcessorEditor::HouseCompAudioProcessorEditor (HouseCompAudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    // Load the embedded Cyrillic-capable font and route ALL UI text through a
    // LookAndFeel that uses it, so Russian tooltips/labels render (no mojibake).
    cyrLnf.cyr = juce::Typeface::createSystemTypefaceFor (BinaryData::AppFont_ttf,
                                                          BinaryData::AppFont_ttfSize);
    // Druk Wide — wide, heavy display face for the title, group labels and values.
    cyrLnf.druk = juce::Typeface::createSystemTypefaceFor (BinaryData::DrukWide_ttf,
                                                           BinaryData::DrukWide_ttfSize);
    clipSwitch.drukFace = limSwitch.drukFace
        = relTypeSwitch.drukFace = cyrLnf.druk;
    setLookAndFeel (&cyrLnf);

    // Load the embedded animated-background sprite sheet and slice it into frames.
    {
        auto sheet = juce::ImageCache::getFromMemory (BinaryData::bg_sprite_png,
                                                      BinaryData::bg_sprite_pngSize);
        if (sheet.isValid())
        {
            const int fw = sheet.getWidth();
            const int fh = fw;                       // frames are square (200x200)
            const int count = sheet.getHeight() / fh;
            for (int i = 0; i < count; ++i)
                gifFrames.add (sheet.getClippedImage ({ 0, i * fh, fw, fh }));
        }
    }

    auto setupKnob = [this] (juce::Slider& s)
    {
        s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 64, 16);   // digits as etalon
        s.setColour (juce::Slider::rotarySliderFillColourId,    pal::cyan);
        s.setColour (juce::Slider::rotarySliderOutlineColourId, pal::track);
        s.setColour (juce::Slider::thumbColourId,               juce::Colours::white);
        s.setColour (juce::Slider::textBoxTextColourId,         pal::cyan);
        s.setColour (juce::Slider::textBoxOutlineColourId,      juce::Colours::transparentBlack);
        // Smooth, gradual feel: pure linear drag with a LONG travel distance, so
        // moving the mouse changes the value slowly/precisely (no jumps). Velocity
        // mode off — it was what caused the abrupt jumps.
        s.setVelocityBasedMode (false);
        s.setMouseDragSensitivity (600);   // bigger = slower/finer (was 320)
        addAndMakeVisible (s);
    };

    setupKnob (amountKnob);
    setupKnob (inGainKnob);
    setupKnob (outGainKnob);
    setupKnob (atkKnob);
    setupKnob (relKnob);
    setupKnob (charKnob);
    // Only the big AMOUNT knob sits directly ON the orb (no disc body + no glow).
    // ATTACK / RELEASE / OUTPUT are normal knobs WITH a disc body, like the band knobs.
    amountKnob.setComponentID ("amount");
    setupKnob (clipShapeKnob);
    setupKnob (upKnob);
    setupKnob (relNoteKnob);
    setupKnob (atkCycKnob);
    // Both are shown only in FUTURE, stacked on top of their dB/ms counterparts.
    relNoteKnob.setVisible (false);
    atkCycKnob.setVisible (false);
    for (auto& k : bandKnob) setupKnob (k);

    // CHARACTER is a compact bipolar horizontal slider (centre = current sound).
    charKnob.setSliderStyle (juce::Slider::LinearHorizontal);
    charKnob.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    charKnob.setColour (juce::Slider::trackColourId,          pal::cyan);
    charKnob.setColour (juce::Slider::backgroundColourId,     pal::track);
    charKnob.setColour (juce::Slider::thumbColourId,          juce::Colours::white);

    // CLIP SHAPE: compact horizontal slider (0 = hard/brickwall = default sound).
    clipShapeKnob.setSliderStyle (juce::Slider::LinearHorizontal);
    clipShapeKnob.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    clipShapeKnob.setColour (juce::Slider::trackColourId,      pal::amber); // single warm accent
    clipShapeKnob.setColour (juce::Slider::backgroundColourId, pal::track);
    clipShapeKnob.setColour (juce::Slider::thumbColourId,      juce::Colours::white);

    // UPWARD (FUTURE only): same compact horizontal form. 0 = off = today's sound.
    upKnob.setSliderStyle (juce::Slider::LinearHorizontal);
    upKnob.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    upKnob.setColour (juce::Slider::trackColourId,      pal::cyan);
    upKnob.setColour (juce::Slider::backgroundColourId, pal::track);
    upKnob.setColour (juce::Slider::thumbColourId,      juce::Colours::white);

    // Group labels are QUIET eyebrows in the WIDE Druk display face: small, dimmed —
    // the bright cyan values stay the loud layer, captions quietly name them.
    // componentID "druk" routes them to the Druk Wide typeface (see getLabelFont).
    auto setupLabel = [this] (juce::Label& l, float h)
    {
        l.setJustificationType (juce::Justification::centred);
        l.setColour (juce::Label::textColourId, pal::textDim);
        l.setComponentID ("druk");
        l.setFont (juce::Font (juce::FontOptions().withHeight (h)));
        addAndMakeVisible (l);
    };
    // Deliberate type scale (Druk Wide): TITLE 20 / feature-caption 11 (AMOUNT, the
    // hero) / every other GROUP CAPTION one single size 9.5 / band captions 9.5.
    // One size per role — no more 8.5/9/10/11 scatter for the same job.
    const float kCaption = 7.1f;           // -25%
    setupLabel (amountLabel, 11.0f);       // hero caption (one tier up)
    setupLabel (gainLabel,   kCaption);
    setupLabel (outLabel,    kCaption);
    setupLabel (atkLabel,    kCaption);
    setupLabel (relLabel,    kCaption);
    setupLabel (charLabel,   kCaption);
    setupLabel (clipShapeLabel, kCaption);
    setupLabel (upLabel,     kCaption);
    // These three captions head a shared column, and they are different LENGTHS
    // (UPWARD / CHARACTER / CLIP SHAPE). Centred, the short one drifts right of the long
    // ones and the whole column reads as misaligned even though every bound is identical.
    // Left-justified, all three start on the same edge.
    for (auto* l : { &upLabel, &charLabel, &clipShapeLabel })
        l->setJustificationType (juce::Justification::centredLeft);
    // TITLE at 28, not 20. The 20 was sized for "HOUSE-COMP" — ten characters spanning
    // the panel. "HOUZY" is half that, and at the old height it sat marooned in the
    // middle of a 740 px row looking like a caption rather than the product name.
    setupLabel (titleLabel,  28.0f);
    for (auto& l : bandLabel) setupLabel (l, kCaption);
    // Title: white text with wide letter spacing feel (reference look).
    titleLabel.setColour (juce::Label::textColourId, pal::text);
    titleLabel.setJustificationType (juce::Justification::centred);
    amountLabel.setColour (juce::Label::textColourId, pal::textDim);

    // Clickable "by ACRONA" byline → centred animated links card.
    bylineLabel.drukFace  = cyrLnf.druk;
    linksOverlay.drukFace = cyrLnf.druk;
    bylineLabel.onClick = [this] { linksOverlay.show(); };
    addAndMakeVisible (bylineLabel);
    addChildComponent (linksOverlay);   // hidden until shown; covers the whole editor

    auto setupToggle = [this] (juce::ToggleButton& b)
    {
        b.setColour (juce::ToggleButton::textColourId, pal::text);
        b.setColour (juce::ToggleButton::tickColourId, pal::cyan);
        b.setColour (juce::ToggleButton::tickDisabledColourId, pal::panelEdge);
        addAndMakeVisible (b);
    };
    setupToggle (bypassButton);
    setupToggle (allMixButton);
    setupToggle (lookaheadButton);

    // Segmented pick-one switches (left = param false, right = param true).
    // clipmode value 1 = HI-Q, 2 = ACR. Value 0 (the retired plain CLIP) is not shown;
    // the switch is bound with firstValue = 1 so segment 0 means HI-Q.
    clipSwitch.setLabels   (juce::StringArray { "HI-Q", "ACR" });
    // limmode value 0 = SMART (3-band), 1 = IRC IV (6-band). Value 2 (PHASE) is retired
    // from the panel — see the note on CrestDisperser for why the idea was dropped.
    // Reads "T3" on the panel. The PARAMETER value is still named "IRC IV" and must stay
    // that way: limmode is a choice parameter, and the host stores the selection by the
    // NAME of the option — rename it and every saved project loses its limiter mode.
    // Same split as CLASSIC|MODERN|HOUZY: label and stored name are deliberately different.
    limSwitch.setLabels    (juce::StringArray { "SMART", "T3" });
    // AUTO | LUFS removed: LUFS make-up is always on, so the switch had no real choice
    // left to offer. The 'lufsgain' parameter stays and is pinned true below.
    // enginemode: 0 = CLASSIC, 1 = MODERN, 2 = FUTURE. Value 3 exists in the parameter as
    // an alias of 2 (the trial name "FUTURE 2") so projects saved during that trial still
    // load; it has no segment of its own because it is not a separate sound.
    // The third segment reads "HOUZY" on the panel, but the PARAMETER value is still
    // named FUTURE and must stay that way: the host stores a choice parameter by the
    // NAME of the option, so renaming it would break every saved project and any
    // automation pointing at it. Label and stored value are deliberately different.
    engineSwitch.setLabels (juce::StringArray { "CLASSIC", "MODERN", "HOUZY" });
    // How the FUTURE release decides to let go. Sits right next to the RELEASE knob.
    relTypeSwitch.setLabels ("BEAT", "SMART");
    // FREE | BOND is retired from the panel. The 'bond' parameter and its engine code
    // stay (pinned off), so projects that stored it still load and it can be brought
    // back by re-adding the switch — same treatment as the retired CLIP and CLASSIC.
    for (auto* s : { &clipSwitch, &limSwitch, &engineSwitch, &relTypeSwitch })
        addAndMakeVisible (*s);

    // A/B compare buttons.
    auto setupTextButton = [this] (juce::TextButton& b)
    {
        b.setColour (juce::TextButton::buttonColourId,   pal::panel);
        b.setColour (juce::TextButton::buttonOnColourId, pal::cyan);
        b.setColour (juce::TextButton::textColourOffId,  pal::textDim);
        b.setColour (juce::TextButton::textColourOnId,   juce::Colour (0xff08111a));
        addAndMakeVisible (b);
    };
    setupTextButton (abAButton);
    setupTextButton (abBButton);
    setupTextButton (abMatchButton);
    relAutoButton.drukFace = cyrLnf.druk;
    addAndMakeVisible (relAutoButton);
    abAButton.setClickingTogglesState (false);
    abBButton.setClickingTogglesState (false);
    abMatchButton.setClickingTogglesState (true);   // it's an on/off toggle
    abAButton.setToggleState (true, juce::dontSendNotification);

    osBox.addItem ("1x", 1); osBox.addItem ("2x", 2); osBox.addItem ("4x", 3);
    osBox.addItem ("8x", 4); osBox.addItem ("16x", 5); osBox.addItem ("32x", 6);
    osBox.addItem ("64x", 7);
    osBox.setColour (juce::ComboBox::backgroundColourId, pal::panel);
    osBox.setColour (juce::ComboBox::textColourId,       pal::text);
    osBox.setColour (juce::ComboBox::outlineColourId,    pal::panelEdge);
    osBox.setColour (juce::ComboBox::arrowColourId,      pal::cyan);
    osBox.setJustificationType (juce::Justification::centred);
    // Route the combo's own text ("32x") through the Druk display face like the rest.
    for (auto* c : osBox.getChildren())
        if (auto* lbl = dynamic_cast<juce::Label*> (c))
            lbl->setComponentID ("druk");
    addAndMakeVisible (osBox);
    setupLabel (osLabel, 9.5f);   // same caption tier as the other group labels

    amountAttach  = std::make_unique<SliderAttach>(proc.apvts, "amount",  amountKnob);
    inGainAttach  = std::make_unique<SliderAttach>(proc.apvts, "ingain",  inGainKnob);
    outGainAttach = std::make_unique<SliderAttach>(proc.apvts, "outgain", outGainKnob);
    atkAttach     = std::make_unique<SliderAttach>(proc.apvts, "atkslew", atkKnob);
    relAttach     = std::make_unique<SliderAttach>(proc.apvts, "relslew", relKnob);
    relNoteAttach = std::make_unique<SliderAttach>(proc.apvts, "relnote",   relNoteKnob);
    atkCycAttach  = std::make_unique<SliderAttach>(proc.apvts, "atkcycles", atkCycKnob);
    charAttach    = std::make_unique<SliderAttach>(proc.apvts, "character", charKnob);
    clipShapeAttach = std::make_unique<SliderAttach>(proc.apvts, "clipshape", clipShapeKnob);
    upAttach        = std::make_unique<SliderAttach>(proc.apvts, "upcomp",    upKnob);
    bandAttach[0] = std::make_unique<SliderAttach>(proc.apvts, "complow",   bandKnob[0]);
    bandAttach[1] = std::make_unique<SliderAttach>(proc.apvts, "complomid", bandKnob[1]);
    bandAttach[2] = std::make_unique<SliderAttach>(proc.apvts, "comphimid", bandKnob[2]);
    bandAttach[3] = std::make_unique<SliderAttach>(proc.apvts, "comphigh",  bandKnob[3]);

    bypassAttach  = std::make_unique<ButtonAttach>(proc.apvts, "bypass",   bypassButton);
    allMixAttach  = std::make_unique<ButtonAttach>(proc.apvts, "allmix",   allMixButton);
    lookaheadAttach=std::make_unique<ButtonAttach>(proc.apvts, "lookahead",lookaheadButton);
    relAutoAttach = std::make_unique<ButtonAttach>(proc.apvts, "relauto",  relAutoButton);

    // Segmented switches <-> params (segment index = param value, offset by firstValue
    // where retired options are hidden from the panel).
    bindSwitch (clipSwitch,   "clipmode", 1);   // hide the retired plain CLIP (value 0)
    bindSwitch (limSwitch,    "limmode");
    // NOTE: LUFS make-up is forced on inside processBlock, NOT from here. Writing a
    // parameter with setValueNotifyingHost while the editor is still being constructed
    // makes the host treat it as a user edit and re-apply state — which reset every
    // other parameter and made the whole plugin look dead.
    bindSwitch (engineSwitch, "enginemode");      // CLASSIC | MODERN | FUTURE, no offset
    bindSwitch (relTypeSwitch, "relsmart");

    // CHARACTER remembers a separate value per engine. The slot is the PARAMETER value,
    // not the segment index — the panel hides CLASSIC, so segment 0 now means MODERN,
    // and indexing by segment would make MODERN and CLASSIC share one memory slot.
    {
        constexpr int kEngineFirst = 0;   // must match the bindSwitch offset above

        lastEngineIndex = juce::jlimit (0, 3, engineSwitch.getIndex() + kEngineFirst);
        charMemInit[lastEngineIndex] = true;
        charMemory[lastEngineIndex]  = (float) charKnob.getValue();

        auto pushSwitch = engineSwitch.onChange;   // the binding installed above
        engineSwitch.onChange = [this, pushSwitch, kEngineFirst] (int index)
        {
            const int from = juce::jlimit (0, 3, lastEngineIndex);
            const int to   = juce::jlimit (0, 3, index + kEngineFirst);
            charMemory[from]  = (float) charKnob.getValue();   // remember what we had
            charMemInit[from] = true;

            if (pushSwitch) pushSwitch (index);                // actually change engine

            charKnob.setValue (charMemory[to], juce::sendNotificationSync);
            charMemInit[to] = true;
            lastEngineIndex = to;
        };
    }

    // Double-click resets each knob to its parameter default.
    amountKnob.setDoubleClickReturnValue (true, 20.0);
    inGainKnob.setDoubleClickReturnValue (true, 0.0);
    outGainKnob.setDoubleClickReturnValue (true, 0.0);   // unity (no attenuation)
    atkKnob.setDoubleClickReturnValue    (true, 20.0);   // MODERN / CLASSIC scale
    relKnob.setDoubleClickReturnValue    (true, 7.0);
    charKnob.setDoubleClickReturnValue   (true, -0.40); // default = softer side
    clipShapeKnob.setDoubleClickReturnValue (true, 0.0); // default = hard brickwall
    upKnob.setDoubleClickReturnValue (true, 0.0);        // default = off
    for (auto& k : bandKnob) k.setDoubleClickReturnValue (true, 20.0);
    relNoteKnob.setDoubleClickReturnValue (true, 13.0);   // 1/32
    atkCycKnob.setDoubleClickReturnValue  (true, 2.3);    // FUTURE scale (wave cycles)

    osAttach      = std::make_unique<ComboAttach> (proc.apvts, "osfactor", osBox);

    // Language toggle (corner). Click flips RU <-> EN and re-applies all tooltips.
    langButton.setColour (juce::TextButton::buttonColourId,  juce::Colour (0xff2a2f3a));
    langButton.setColour (juce::TextButton::textColourOffId, pal::textDim);
    langButton.onClick = [this] { ru = ! ru; saveLanguageChoice (ru); applyLanguage(); };
    addAndMakeVisible (langButton);
    ru = loadLanguageChoice();   // saved choice, or the system language on a first run
    applyLanguage();   // set initial tooltips + language button caption

    // --- A/B compare wiring. Both slots start as a snapshot of the loaded state. ---
    captureToSlot (0);
    captureToSlot (1);
    abAButton.onClick = [this] { switchToSlot (0); };
    abBButton.onClick = [this] { switchToSlot (1); };
    abMatchButton.onClick = [this]
    {
        abMatchOn = abMatchButton.getToggleState();
        // Always drop the trim first, then RE-MEASURE the current slot before
        // matching. Recomputing from stored (stale, possibly already-trimmed)
        // values is what made the level run away when switching + editing.
        proc.abMatchTrimDb.store (0.0f);
        measureSlotCountdown = abMatchOn ? 45 : 0;   // ~1.5 s at 30 Hz
    };

    // (Pick-one behaviour is now built into the segmented switches — a switch always
    //  has exactly one side active, so the old radio wiring is no longer needed.)

    // When All Mix Comp toggles, grey out the inactive set.
    allMixButton.onClick = [this] { updateBandEnablement(); };
    updateBandEnablement();

    // While All Mix Comp is OFF, the 4 band knobs MIRROR the AMOUNT knob so that
    // switching into All Mix Comp starts from the same value (no jump, no re-dial).
    amountKnob.onValueChange = [this]
    {
        if (! allMixButton.getToggleState())
        {
            const double v = amountKnob.getValue();
            for (auto& k : bandKnob)
                k.setValue (v, juce::sendNotificationSync);
        }
    };

    // Resizable, locked to the 740x500 aspect ratio. JUCE's own constrainer enforces
    // both the limits and the ratio while dragging, so the window can never end up
    // taller/wider than the scaled content (which is what produced the black bars).
    // resized() then only pushes ONE transform — it no longer fights the constrainer.
    setResizable (true, true);
    if (auto* c = getConstrainer())
    {
        c->setFixedAspectRatio (740.0 / 500.0);
        c->setSizeLimits (592, 400, 1776, 1200);   // 0.8x .. 2.4x
    }
    // REPARENT every control into the scaling container. Done here, once, after all the
    // addAndMakeVisible() calls above, so each control can keep registering itself the
    // normal way and none of that code has to know the container exists.
    // The existing order is preserved, which keeps the z-order (linksOverlay on top).
    {
        content.owner = this;
        addAndMakeVisible (content);
        content.setInterceptsMouseClicks (false, true);   // pass clicks to the controls
        auto kids = getChildren();                        // copy: we mutate as we go
        for (auto* c : kids)
            if (c != &content)
            {
                // PRESERVE VISIBILITY. Some children are deliberately added hidden with
                // addChildComponent — linksOverlay is, because it must only appear when
                // "by ACRONA" is clicked. Reparenting with addAndMakeVisible would show
                // every one of them, which is exactly what made the links card come up
                // on its own the moment the plugin opened.
                const bool wasVisible = c->isVisible();
                content.addChildComponent (c);
                c->setVisible (wasVisible);
            }
    }

    // Reopen at whatever size was last used; otherwise the default, which is 32% above
    // the base grid (740 x 1.15 x 1.15). The layout itself stays 740x500 — that grid is
    // what every child is positioned on — so only the view scale changes and the
    // proportions are identical whatever the size.
    {
        const int savedW = loadWindowWidth();
        const int w = savedW > 0 ? savedW : 723;                       // 851 x 0.85
        const int h = juce::roundToInt ((float) w * 500.0f / 740.0f);  // locked ratio
        setSize (w, h);
    }

    // Seed the ambient particle swarm (stable per-particle "character").
    partRng.setSeedRandomly();
    for (int i = 0; i < 22; ++i)
    {
        Particle p;
        p.ang        = partRng.nextFloat() * juce::MathConstants<float>::twoPi;
        p.radius     = 0.5f + partRng.nextFloat() * 0.75f;   // fraction of ring radius (0.5..1.25)
        p.speed      = (partRng.nextBool() ? 1.0f : -1.0f) * (0.010f + partRng.nextFloat() * 0.030f);
        p.size       = 0.8f + partRng.nextFloat() * 1.8f;
        p.life       = 1.0f;
        p.fade       = 1.0f;   // ambient particles are already faded in
        p.wobbleSeed = partRng.nextFloat() * juce::MathConstants<float>::twoPi;
        p.burst      = false;
        particles.push_back (p);
    }

    startTimerHz (30);
}

//==============================================================================
// Wire a SegmentedSwitch two-way to an APVTS param: init from the param, and on a
// click push the new segment index to it (with proper gesture wrapping). The timer
// keeps the switch in sync if the param changes from the host/A-B/state load.
//
// Works for BOTH bool params (2 segments) and choice params (3+): going through
// convertTo0to1/convertFrom0to1 means the segment index maps to the parameter's own
// range, so a 3-choice param lands on 0 / 0.5 / 1 without any special-casing here.
// `firstValue` lets the panel show only PART of a choice parameter's range: segment 0
// maps to that value, segment 1 to the next, and so on. That is how retired options
// (plain CLIP, and CLASSIC) stay in the parameter — so presets that stored them still
// load — while disappearing from the UI.
void HouseCompAudioProcessorEditor::bindSwitch (SegmentedSwitch& sw, const juce::String& paramID,
                                                int firstValue)
{
    if (auto* p = dynamic_cast<juce::RangedAudioParameter*> (proc.apvts.getParameter (paramID)))
    {
        const int v = juce::roundToInt (p->convertFrom0to1 (p->getValue()));
        sw.setIndex (juce::jmax (0, v - firstValue));
    }

    sw.onChange = [this, paramID, firstValue] (int index)
    {
        if (auto* p = dynamic_cast<juce::RangedAudioParameter*> (proc.apvts.getParameter (paramID)))
        {
            p->beginChangeGesture();
            p->setValueNotifyingHost (p->convertTo0to1 ((float) (index + firstValue)));
            p->endChangeGesture();
        }
    };
}

//==============================================================================
// A/B compare: each slot is a snapshot of every parameter value. Switching saves
// the live params into the current slot, then loads the target slot's params, so
// you can flip A/B to compare two versions of your tweak without Ctrl+Z.
void HouseCompAudioProcessorEditor::captureToSlot (int slot)
{
    juce::ValueTree t ("AB");
    for (auto* p : proc.getParameters())
        if (auto* pw = dynamic_cast<juce::AudioProcessorParameterWithID*> (p))
            t.setProperty (pw->paramID, pw->getValue(), nullptr);   // 0..1 normalised
    abSlot[slot] = t;
}

void HouseCompAudioProcessorEditor::recallSlot (int slot)
{
    const auto& t = abSlot[slot];
    if (! t.isValid()) return;
    for (auto* p : proc.getParameters())
        if (auto* pw = dynamic_cast<juce::AudioProcessorParameterWithID*> (p))
            if (t.hasProperty (pw->paramID))
            {
                pw->beginChangeGesture();
                pw->setValueNotifyingHost ((float) t.getProperty (pw->paramID));
                pw->endChangeGesture();
            }
}

void HouseCompAudioProcessorEditor::switchToSlot (int slot)
{
    if (slot == abActive) return;

    // Store the TRUE loudness of the slot we're leaving. The meter reads the signal
    // BEFORE abMatchTrimDb is applied... but only if we subtract any trim currently
    // in effect — otherwise a matched slot records its trimmed level, the next match
    // is computed from that, and the error compounds every switch (level runaway).
    abLoudness[abActive] = proc.meterLufsDb.load() - proc.abMatchTrimDb.load();

    captureToSlot (abActive);   // save current edits into the slot we're leaving
    abActive = slot;
    recallSlot (slot);          // load the target
    abAButton.setToggleState (slot == 0, juce::dontSendNotification);
    abBButton.setToggleState (slot == 1, juce::dontSendNotification);

    // Drop the trim to 0 immediately: the incoming slot's stored loudness may be
    // stale (it was edited since), so applying an old trim is what made B jump.
    // measureSlotCountdown re-measures the new slot and applies a fresh trim.
    proc.abMatchTrimDb.store (0.0f);
    measureSlotCountdown = abMatchOn ? 45 : 0;   // ~1.5 s at 30 Hz to settle
}

//==============================================================================
// REMEMBERED LANGUAGE.
//
// A plain text file in the user's application-data folder, not the registry and not the
// plugin state. The registry would tie the setting to an installer this plugin does not
// require — a hand-copied .vst3 has to behave the same. Plugin state would be worse
// still: language belongs to the person at the keyboard, so storing it in the project
// would hand a Russian session to whoever opens the file next.
juce::File HouseCompAudioProcessorEditor::languageFile()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
             .getChildFile ("ACRONA AUDIO")
             .getChildFile ("HOUZY")
             .getChildFile ("language.txt");
}

bool HouseCompAudioProcessorEditor::loadLanguageChoice()
{
    const auto f = languageFile();
    if (f.existsAsFile())
    {
        const auto s = f.loadFileAsString().trim().toLowerCase();
        if (s == "ru") return true;
        if (s == "en") return false;
        // Anything else means the file was corrupted or hand-edited; fall through to
        // the system language rather than silently picking a side.
    }

    // FIRST RUN: follow the operating system. getUserLanguage returns an ISO code such
    // as "ru" or "en"; Russian-language Windows gives "ru" regardless of region, which
    // is exactly the question being asked.
    return juce::SystemStats::getUserLanguage().toLowerCase().startsWith ("ru");
}

void HouseCompAudioProcessorEditor::saveLanguageChoice (bool russian)
{
    const auto f = languageFile();
    f.getParentDirectory().createDirectory();   // no-op when it already exists
    f.replaceWithText (russian ? "ru" : "en");  // failure here is not worth interrupting
}                                               // the user over — it just won't persist

//==============================================================================
// REMEMBERED WINDOW SIZE. Same folder, same reasoning as the language: it is a property
// of this person's screen, not of the project, so it must not travel inside the session.
juce::File HouseCompAudioProcessorEditor::windowSizeFile()
{
    return languageFile().getSiblingFile ("window.txt");
}

int HouseCompAudioProcessorEditor::loadWindowWidth()
{
    const auto f = windowSizeFile();
    if (! f.existsAsFile()) return 0;

    // Clamped to the same limits the constrainer uses. A file carrying a size from a
    // monitor that is no longer attached would otherwise open the editor off-screen or
    // absurdly small, with no obvious way back.
    const int w = f.loadFileAsString().trim().getIntValue();
    return (w >= 592 && w <= 1776) ? w : 0;   // must match the constrainer limits
}

void HouseCompAudioProcessorEditor::saveWindowSize (int width)
{
    const auto f = windowSizeFile();
    f.getParentDirectory().createDirectory();
    f.replaceWithText (juce::String (width));
}

//==============================================================================
// Localization: assign every tooltip (and the localizable labels) in RU or EN.
// Called on startup and whenever the RU/EN button is clicked. The embedded font
// renders Cyrillic, so Russian shows correctly with no mojibake.
void HouseCompAudioProcessorEditor::applyLanguage()
{
    langButton.setButtonText (ru ? "RU" : "EN");

    // NOTE: knob NAMES stay fixed (English) regardless of language — only the
    // hover DESCRIPTIONS (tooltips) below switch RU/EN. The static names are set
    // once at construction (COMPRESS / ATTACK / RELEASE / INPUT / CHARACTER / ...).

    auto T = [this] (juce::SettableTooltipClient& c, const char* rus, const char* eng)
    { c.setTooltip (ru ? juce::String::fromUTF8 (rus) : juce::String (eng)); };

    // Only controls that aren't self-explanatory (per request: INPUT / ALL MIX
    // COMP are obvious, no tip needed there).
    T (amountKnob, "ГЛАВНАЯ РУЧКА — сила сжатия сигнала. Больше = плотнее и ровнее, "
                   "тихие и громкие места сближаются. Крути её первой.\n\n"
                   "ВАЖНО: если не хватает громкости — НЕ добавляй сюда, прибавляй INPUT. "
                   "Эта ручка про плотность, а не про громкость. Не пережимай.",
                   "THE MAIN KNOB - how hard the signal is compressed. More = denser and "
                   "flatter, quiet and loud parts move closer together. Start here.\n\n"
                   "IMPORTANT: if you need more loudness, do NOT add it here - turn up INPUT. "
                   "This knob is about density, not volume. Don't over-compress.");

    // MODERN only — in FUTURE this knob is swapped for atkCycKnob (wave cycles).
    T (atkKnob,    "Как быстро компрессор реагирует на удар. "
                   "Больше = хватает мгновенно, кик придавливается сразу (ровнее). "
                   "Меньше = пропускает начало удара вперёд (кик звучит острее).",
                   "How quickly the compressor reacts to a hit. "
                   "Higher = grabs instantly, the kick is held down right away (flatter). "
                   "Lower = lets the very start of the hit through (punchier kick).");

    T (atkCycKnob,
                   "Скорость атаки в ПЕРИОДАХ ВОЛНЫ каждого тона, а не в миллисекундах. "
                   "2.3 значит: гейн двигается за 2.3 колебания того тона, который жмёт.\n\n"
                   "Почему так: 5 мс на басу 50 Гц — это четверть его волны, гейн успевает "
                   "дёрнуться внутри одного колебания и пачкает низ. На хэте те же 5 мс — "
                   "вечность. Одно число не годится обоим. В периодах одна ручка ведёт "
                   "себя верно на всём спектре: бас промодулировать НЕЛЬЗЯ, верх всё равно "
                   "хватается мгновенно.\n\n"
                   "Меньше = резче и плотнее. Больше = мягче, транзиенты живее. "
                   "На верхах ручка почти не слышна — там атака и так на пределе.",
                   "Attack speed in PERIODS OF EACH TONE'S OWN WAVE, not milliseconds. "
                   "2.3 means the gain moves over 2.3 oscillations of whichever tone is "
                   "being compressed.\n\n"
                   "Why: 5 ms on a 50 Hz bass note is a quarter of its wave, so the gain "
                   "moves inside a single oscillation and dirties the low end - while the "
                   "same 5 ms is an eternity for a hi-hat. One number cannot serve both. In "
                   "cycles a single knob behaves correctly across the spectrum: the bass "
                   "CANNOT be modulated, the top is still grabbed instantly.\n\n"
                   "Lower = tighter and denser. Higher = softer, transients more alive. "
                   "Barely audible up top, where the attack is already at its limit.");

    // MODERN only — in FUTURE this knob is swapped for relNoteKnob (note values), which
    // carries its own tooltip. Nothing here should mention beats or tempo.
    T (relKnob,    "Как быстро компрессор отпускает после удара. "
                   "Меньше = держит дольше, звук ровнее и плотнее. "
                   "Больше = отпускает быстро, слышнее «дыхание» между ударами.",
                   "How quickly the compressor lets go after a hit. "
                   "Lower = holds longer, steadier and denser. "
                   "Higher = releases quickly, you hear more 'breathing' between hits.");

    T (relTypeSwitch,
                   "Работает ТОЛЬКО в HOUZY. Два характера, а не «лучше/хуже» — "
                   "выбирай под трек.\n\n"
                   "SMART — тон отпускается, когда реально стих. Верх чище и свободнее, "
                   "хвосты не придавлены. Обычный выбор.\n\n"
                   "BEAT — весь спектр держит заданную долю такта. Верх плотнее и "
                   "ритмичнее, больше «дыхания» в такт. На одних треках это то что надо, "
                   "на других лишнее — сравни на своём.",
                   "Works ONLY in HOUZY. Two characters, not better/worse - pick per track.\n\n"
                   "SMART - a tone is let go once it has actually died away. Cleaner, more "
                   "open on top, tails are not held down. The usual choice.\n\n"
                   "BEAT - every tone holds the full note value. Denser and more rhythmic up "
                   "top, more breathing in time with the groove. Right on some tracks, too "
                   "much on others - compare on your own material.");

    T (relNoteKnob,
                   "Длительность релиза в долях такта: 1/4, 1/8, 1/16 и так далее. Ручка "
                   "щёлкает по нотам, между ними не встанешь — попадаешь в сетку точно.\n\n"
                   "ВАЖНО: темп берётся из НАСТРОЕК ПРОЕКТА. Выстави в проекте темп своего "
                   "трека — тогда релиз ляжет ровно между ударами. Поменяешь темп — релиз "
                   "подстроится сам.\n\n"
                   "С чего начать: 1/8 — универсально, 1/16 — плотнее и суше, 1/4 — чтобы "
                   "дышало.",
                   "Release length as a note value: 1/4, 1/8, 1/16 and so on. The knob "
                   "clicks between values - you cannot land between them, so you always sit "
                   "on the grid.\n\n"
                   "IMPORTANT: the tempo comes from your PROJECT SETTINGS. Set the project "
                   "tempo to match your track and the release lands neatly between hits. "
                   "Change the tempo and it follows automatically.\n\n"
                   "Where to start: 1/8 is a safe default, 1/16 is tighter and drier, 1/4 "
                   "lets it breathe.");

    T (charKnob,   "Общий характер сжатия. "
                   "Влево = мягче и открытее, больше динамики. "
                   "В центре и вправо = плотнее и жёстче, всё сильнее сбивается в одну линию.",
                   "The overall feel of the compression. "
                   "Left = softer and more open, more dynamics left alone. "
                   "Centre and right = denser and harder, everything pressed closer together.");

    T (outGainKnob, "Финальная громкость на выходе. Работает ТОЛЬКО на убавление "
                    "(0 дБ = без изменений). Поднять её нельзя специально: подъём после "
                    "обработки создал бы новые пики и испортил бы уже готовый сигнал.",
                    "Final output level. It can only turn DOWN (0 dB = unchanged). "
                    "Boosting is deliberately not allowed: raising the level after processing "
                    "would create new peaks and spoil an already finished signal.");

    T (bandKnob[0], "Сжатие только низа (до 160 Гц) — бочка и бас. "
                    "Работает, когда включён ALL MIX.",
                    "Compresses the low end only (below 160 Hz) - kick and bass. "
                    "Active when ALL MIX is on.");
    T (bandKnob[1], "Сжатие нижней середины (160-1500 Гц) — тело баса, малый барабан, нижние синты.",
                    "Compresses the low mids (160-1500 Hz) - bass body, snare, lower synths.");
    T (bandKnob[2], "Сжатие верхней середины (1500-7000 Гц) — вокал, лид-синты, атака инструментов.",
                    "Compresses the high mids (1500-7000 Hz) - vocals, lead synths, instrument attack.");
    T (bandKnob[3], "Сжатие верхов (выше 7000 Гц) — хэты, тарелки, воздух.",
                    "Compresses the highs (above 7000 Hz) - hats, cymbals, air.");

    T (bypassButton, "Полностью отключает обработку — чтобы сравнить, как звучит с плагином и без.",
                     "Turns all processing off - so you can compare the sound with and without the plugin.");

    T (lookaheadButton, "Плагин заранее «видит» подходящий пик и начинает убирать его плавно, "
                        "а не рывком. Звучит мягче, но добавляет крошечную задержку "
                        "(программа её компенсирует сама).",
                        "The plugin sees a peak coming and starts easing it down smoothly instead of "
                        "reacting abruptly. Sounds gentler, at the cost of a tiny delay "
                        "(your DAW compensates for it automatically).");

    T (osBox, "Качество обработки пиков. Чем выше значение, тем чище звучат срезанные пики, "
              "но тем больше нагрузка на процессор. 16x — хороший баланс для работы, "
              "32x и выше есть смысл включать при финальном экспорте.",
              "Peak-processing quality. Higher values mean cleaner-sounding peaks but more CPU load. "
              "16x is a good balance while working; 32x and above are worth it for the final export.");

    T (upKnob, "Upward-компрессия: поднимает тихие участки.",
               "Upward compression: lifts the quiet parts.");

    T (clipShapeKnob, "Мягкость среза пиков. Слева — чисто и нейтрально, вправо — "
                      "теплее, с насыщением.\n\n"
                      "Работает только в HI-Q. В ACR пики снимаются ещё до клиппера, "
                      "поэтому форму среза менять уже не на чем — ручка гаснет.",
                      "How gently peaks are shaved. Left - clean and neutral, right - "
                      "warmer, with saturation.\n\n"
                      "Works in HI-Q only. In ACR the peaks are removed before the "
                      "clipper, so there is no cut left to reshape - the knob dims.");

    // Segmented switches.
    // clipSwitch / engineSwitch use PER-SEGMENT tooltips (set in applyLanguage below),
    // so there is no single blanket description for either of them.

    // limSwitch uses PER-SEGMENT tooltips (set in applyLanguage below).

    T (relAutoButton,
                  "Работает ТОЛЬКО в HOUZY. Плагин сам подбирает длительность релиза: "
                  "он ИЗМЕРЯЕТ, за сколько реально стихают тона твоего материала, и "
                  "округляет до ближайшей ноты. Ручка RELEASE при этом гаснет и "
                  "показывает выбранное значение.\n\n"
                  "У ATTACK такой кнопки нет намеренно: атака и так автоматическая — "
                  "каждый тон получает своё время от своей частоты. Ручка задаёт только "
                  "характер (панч или плотность), а это вкус, его автомат не решит.",
                  "Works ONLY in HOUZY. The plugin picks the release length itself: it "
                  "MEASURES how long your material's tones actually take to fade and "
                  "rounds that to the nearest note value. The RELEASE knob dims and "
                  "shows what was chosen.\n\n"
                  "ATTACK deliberately has no such button: it is already automatic - "
                  "every tone gets its own timing from its own frequency. The knob only "
                  "sets character (punch or density), and that is taste, which no "
                  "automatic value can decide for you.");

    // Per-segment tooltips: point at a position, read what THAT position does.
    // Russian literals MUST go through String::fromUTF8 in this file — passing them
    // straight to juce::String produces mojibake (the rest of applyLanguage does the same).
    engineSwitch.setSegmentTooltips (ru
        ? juce::StringArray {
            juce::String::fromUTF8 (
            "CLASSIC — исходный звук плагина, без более поздних исправлений. Плотнее и "
            "громче: сжимает всё в одну ровную линию. Бери, когда нужна максимальная "
            "плотность или чтобы старый проект звучал как раньше."),

            juce::String::fromUTF8 (
            "MODERN — обычный компрессор, аккуратно настроенный. Бочка дышит, низ чистый, "
            "мало «накачки». Бери, если HOUZY грузит процессор."),

            juce::String::fromUTF8 (
            "HOUZY — новый вид компрессии. Звук разбирается на отдельные тона, и "
            "громкость каждого выравнивается ОТДЕЛЬНО. Общего гейна нет, поэтому кик не "
            "давит хэты и «накачки» не бывает в принципе. Начало и затухание каждого "
            "звука движок определяет по слуховой полосе, поэтому хэты, тело малого и "
            "хвосты ревера не проскакивают мимо сжатия, а низ дожимается до конца. Атака "
            "при этом защищена у каждого тона отдельно, и у низких — дольше, так что кик "
            "держит панч, как бы сильно ты ни жал.") }
        : juce::StringArray {
            "CLASSIC - the plugin's original sound, without the later corrections. Denser "
            "and louder: presses everything into one flat line. Use it when you want "
            "maximum density, or to make an old project sound the way it did.",

            "MODERN - a conventional compressor, carefully tuned. The kick breathes, the "
            "low end is clean, little pumping. Use it if HOUZY costs too much CPU.",

            "HOUZY - a new kind of compression. The sound is taken apart into individual "
            "tones and each tone's loudness is evened out SEPARATELY. No shared gain, so "
            "the kick cannot duck the hats and pumping does not exist here. Where each "
            "sound starts and stops is read from a whole band of hearing, so hats, snare "
            "bodies and reverb tails do not slip past the compression and the low end is "
            "squeezed all the way. Each tone's attack is guarded on its own - longer for "
            "low tones - so the kick keeps its punch however hard you squeeze." });

    limSwitch.setSegmentTooltips (ru
        ? juce::StringArray {
            juce::String::fromUTF8 (
            "SMART — 3 полосы, ориентируется на то, как слышит ухо. Мягче, но грубее "
            "по частотам."),

            juce::String::fromUTF8 (
            "T3 — 6 полос. Пик в басу не глушит верха. Обычный выбор.") }
        : juce::StringArray {
            "SMART - 3 bands, following how the ear hears. Softer, but coarser across "
            "frequency.",

            "T3 - 6 bands. A peak in the bass will not duck the highs. The usual choice." });

    clipSwitch.setSegmentTooltips (ru
        ? juce::StringArray {
            juce::String::fromUTF8 (
            "HI-Q — обычный чистый клиппер. Срезает верхушку волны точно и без "
            "призвуков. Проверенный, предсказуемый вариант."),

            juce::String::fromUTF8 (
            "ACR — НОВАЯ ТЕХНОЛОГИЯ КЛИППИРОВАНИЯ, разработанная для этого плагина.\n\n"
            "Обычный клиппер СРЕЗАЕТ верхушку волны, и этот срез бьёт по всему спектру "
            "сразу — поэтому каждый удар кика подрубает хэты и верх. Оверсэмплинг это "
            "не лечит.\n\n"
            "ACR вместо среза ВЫЧИТАЕТ короткий импульс точно в момент пика. Спектр "
            "импульса подобран так, чтобы искажение легло туда, где его маскирует сам "
            "кик. Пик снимается так же ровно, но верх остаётся целым.\n\n"
            "Громкость та же, что у HI-Q — сравнивать можно напрямую.") }
        : juce::StringArray {
            "HI-Q - a conventional clean clipper. Slices the top off the waveform "
            "precisely and without artefacts. The proven, predictable option.",

            "ACR - instead of slicing the peak it subtracts a short pulse shaped to sit "
            "under the masking curve. The kick is cut just as flat but stops chopping the "
            "hats and highs on every beat. Same loudness as HI-Q." });

    T (abAButton, "Вариант настроек A. Нажми, чтобы переключиться на него.",
                  "Settings version A. Click to switch to it.");
    T (abBButton, "Вариант настроек B. Нажми, чтобы переключиться на него. "
                  "Удобно: настрой два варианта и сравнивай.",
                  "Settings version B. Click to switch to it. "
                  "Handy for dialling in two versions and comparing them.");
    T (abMatchButton, "Выравнивает громкость A и B. Включи — и оба варианта будут звучать "
                      "одинаково громко, чтобы ты сравнивал именно характер, а не то, "
                      "что просто громче.",
                      "Matches the loudness of A and B. Turn it on and both versions play equally "
                      "loud, so you compare the actual character instead of just picking "
                      "whichever is louder.");
    T (langButton, "Язык подсказок: русский / английский.", "Tooltip language: Russian / English.");

    T (inGainKnob, "Громкость ДО обработки. Подними, если трек тихий и плагин почти не работает — "
                   "тогда сжатие начнёт действовать. Опусти, если давит слишком сильно.",
                   "Level going INTO the plugin. Turn it up if the track is quiet and nothing much "
                   "is happening - that's what makes the compression start working. Turn it down "
                   "if it's squashing too hard.");

    T (allMixButton, "Включает раздельное сжатие по четырём диапазонам: низ, нижняя середина, "
                     "верхняя середина, верх. Главная ручка при этом замирает, а сила сжатия "
                     "задаётся отдельно для каждого диапазона.",
                     "Switches to compressing four frequency ranges separately: lows, low mids, "
                     "high mids and highs. The main knob freezes and each range gets its own "
                     "amount instead.");
}

void HouseCompAudioProcessorEditor::updateBandEnablement()
{
    const bool allMix = allMixButton.getToggleState();
    // Enable/disable only; the alpha fade is animated in timerCallback().
    amountKnob.setEnabled (! allMix);
    for (int b = 0; b < 4; ++b)
        bandKnob[b].setEnabled (allMix);
}

//==============================================================================
void HouseCompAudioProcessorEditor::timerCallback()
{
    // Keep segmented switches in sync with their params (host automation / A-B /
    // state load can change them behind the UI's back).
    auto syncSw = [this] (SegmentedSwitch& sw, const char* id, int firstValue = 0)
    {
        if (auto* p = dynamic_cast<juce::RangedAudioParameter*> (proc.apvts.getParameter (id)))
        {
            const int v = juce::roundToInt (p->convertFrom0to1 (p->getValue()));
            const int idx = juce::jlimit (0, sw.numSegments() - 1, v - firstValue);
            if (idx != sw.getIndex())
                sw.setIndex (idx);
        }
    };
    syncSw (clipSwitch,   "clipmode", 1);
    syncSw (limSwitch,    "limmode");
    syncSw (engineSwitch, "enginemode");

    // Controls that only mean something in FUTURE. UPWARD and BEAT|SMART have no
    // per-tone stage to act on elsewhere, so they grey out and stop responding — a knob
    // should never silently pretend to do something.
    //
    // ATTACK and RELEASE stay live in both engines but change their MEANING in FUTURE
    // (cycles of each tone's wave / a fraction of a beat, instead of milliseconds), so
    // they get a different caption and a slightly different tint to make that visible.
    {
        bool future = false;
        if (auto* p = dynamic_cast<juce::RangedAudioParameter*> (proc.apvts.getParameter ("enginemode")))
            future = juce::roundToInt (p->convertFrom0to1 (p->getValue())) >= 2;

        if ((int) future != futureUiShown)
        {
            futureUiShown = (int) future;

            // Dim UPWARD by COLOUR, not by setAlpha. Fading the whole component washes
            // out its track and thumb unevenly, so the row reads as misaligned next to
            // CHARACTER and CLIP SHAPE even though its bounds are identical.
            upKnob.setEnabled (future);
            upKnob.setColour (juce::Slider::trackColourId,
                              future ? pal::cyan : pal::cyan.withAlpha (0.30f));
            upKnob.setColour (juce::Slider::thumbColourId,
                              future ? juce::Colours::white
                                     : juce::Colours::white.withAlpha (0.35f));
            upKnob.setColour (juce::Slider::backgroundColourId, pal::track);
            upLabel.setColour (juce::Label::textColourId,
                               future ? pal::textDim : pal::textDim.withAlpha (0.40f));
            upKnob.repaint();
            upLabel.repaint();

            // CHARACTER is the mirror image: it only means something in MODERN. In
            // FUTURE its threshold/ratio work belongs to compressors that are held at
            // zero, and AMOUNT already sets how deeply each tone is squeezed — so the
            // knob is switched off there rather than left quietly changing loudness.
            charKnob.setEnabled (! future);
            charKnob.setColour (juce::Slider::trackColourId,
                                future ? pal::cyan.withAlpha (0.30f) : pal::cyan);
            charKnob.setColour (juce::Slider::thumbColourId,
                                future ? juce::Colours::white.withAlpha (0.35f)
                                       : juce::Colours::white);
            charLabel.setColour (juce::Label::textColourId,
                                 future ? pal::textDim.withAlpha (0.40f) : pal::textDim);
            charKnob.repaint();
            charLabel.repaint();

            relTypeSwitch.setEnabled (future);
            relTypeSwitch.setAlpha (future ? 1.0f : 0.35f);
            relAutoButton.setEnabled (future);
            relAutoButton.setAlpha (future ? 1.0f : 0.35f);

            atkLabel.setText (future ? "CYCLES" : "ATTACK",  juce::dontSendNotification);
            relLabel.setText (future ? "NOTE"   : "RELEASE", juce::dontSendNotification);

            // RELEASE drives a DIFFERENT parameter per engine: dB/ms in MODERN, a note
            // value in FUTURE. TWO separate knobs, each permanently attached to its own
            // parameter, and we simply show one and hide the other.
            //
            // The previous approach — destroying and rebuilding the attachment on the
            // fly — was the bug that made the whole plugin stop responding. It tore down
            // a SliderAttachment from a deferred callback that captured `this` raw, so a
            // closed editor left a dangling call in the queue; and rebuilding it pushed a
            // value from the old scale into the new parameter's range. Swapping visibility
            // costs nothing and cannot corrupt anything.
            relKnob.setVisible (! future);
            relNoteKnob.setVisible (future);
            atkKnob.setVisible (! future);
            atkCycKnob.setVisible (future);

            // The FUTURE pair gets the warm tint so the change of MEANING is visible at
            // a glance; the MODERN pair keeps the standard cyan.
            atkCycKnob.setColour (juce::Slider::rotarySliderFillColourId, pal::amber);
            relNoteKnob.setColour (juce::Slider::rotarySliderFillColourId, pal::amber);
            atkKnob.setColour (juce::Slider::rotarySliderFillColourId, pal::cyan);
            relKnob.setColour (juce::Slider::rotarySliderFillColourId, pal::cyan);
            atkCycKnob.repaint(); relNoteKnob.repaint();
            atkKnob.repaint();    relKnob.repaint();
        }

        // CLIP SHAPE follows the CLIPPER, not the engine — live in HI-Q, inert in ACR.
        // Its own check, because the two switches move independently: folding it into
        // the engine block meant flipping engines never updated it, and flipping the
        // clipper wrongly re-ran the engine's controls (which is what left AUTO stuck).
        int clipIdx = 1;
        if (auto* p = dynamic_cast<juce::RangedAudioParameter*> (proc.apvts.getParameter ("clipmode")))
            clipIdx = juce::roundToInt (p->convertFrom0to1 (p->getValue()));
        const bool acr = (clipIdx == 2);

        if ((int) acr != clipShapeDimmed)
        {
            clipShapeDimmed = (int) acr;
            clipShapeKnob.setEnabled (! acr);
            clipShapeKnob.setColour (juce::Slider::trackColourId,
                                     acr ? pal::amber.withAlpha (0.30f) : pal::amber);
            clipShapeKnob.setColour (juce::Slider::thumbColourId,
                                     acr ? juce::Colours::white.withAlpha (0.35f)
                                         : juce::Colours::white);
            clipShapeLabel.setColour (juce::Label::textColourId,
                                      acr ? pal::textDim.withAlpha (0.40f) : pal::textDim);
            clipShapeKnob.repaint();
            clipShapeLabel.repaint();
        }

        // While AUTO is picking the note, the knob has nothing to set — dim it and let
        // it follow the engine's choice, so it shows what is happening instead of
        // sitting on a value that is being ignored.
        const int autoNote = proc.autoRelNote.load();
        const bool autoOn = future && autoNote >= 0;
        if ((int) autoOn != relAutoShown)
        {
            relAutoShown = (int) autoOn;
            relNoteKnob.setEnabled (! autoOn);
            relNoteKnob.setAlpha (autoOn ? 0.5f : 1.0f);
        }
        if (autoOn && (int) relNoteKnob.getValue() != autoNote)
            relNoteKnob.setValue (autoNote, juce::dontSendNotification);
    }

    // A/B loudness-match: after a slot switch, wait for the meter to settle on the
    // NEW slot, store its true loudness, and only then apply the matching trim. This
    // is what stops the level running away when you keep switching and editing.
    if (measureSlotCountdown > 0 && --measureSlotCountdown == 0)
    {
        abLoudness[abActive] = proc.meterLufsDb.load();   // trim is 0 right now
        if (abMatchOn && abLoudness[0] > -60.0f && abLoudness[1] > -60.0f)
        {
            // Trim the CURRENT slot toward the other one, and only ever attenuate
            // (a boost here could push the already peak-safe output over the ceiling).
            const float diff = abLoudness[abActive ^ 1] - abLoudness[abActive];
            proc.abMatchTrimDb.store (juce::jlimit (-12.0f, 0.0f, diff));
        }
    }

    // Smoother meters (lighter coefficient = gentler movement, no jitter).
    meterGrDb = meterGrDb * 0.82f + proc.currentGainReductionDb.load() * 0.18f;

    auto smooth = [] (float& shown, float target)
    { shown = shown * 0.85f + juce::jlimit (-60.0f, 6.0f, target) * 0.15f; };
    smooth (rmsShownDb,  proc.meterRmsDb.load());
    smooth (lufsShownDb, proc.meterLufsDb.load());

    // Honest per-stage GR + clip activity (smoothed for a steady readout).
    compGrShownDb = compGrShownDb * 0.80f + proc.compGrDb.load()    * 0.20f;
    limGrShownDb  = limGrShownDb  * 0.80f + proc.limiterGrDb.load() * 0.20f;
    clipActShown  = clipActShown  * 0.75f + proc.clipActivity.load()* 0.25f;

    // Peak-hold on RMS & LUFS: latch the max, decay slowly; light an OVER lamp for
    // ~1 s whenever a reading crosses -0.3 dBFS (clipping-loud territory).
    auto peakHold = [] (float shown, float& peak, bool& over, int& hold)
    {
        if (shown > peak) peak = shown;
        else              peak -= 0.15f;                 // slow fall (~ dB/tick)
        peak = juce::jlimit (-60.0f, 6.0f, peak);
        if (shown > -0.3f) { over = true; hold = 30; }   // 30 ticks @30Hz ≈ 1 s
        else if (hold > 0) --hold;
        else over = false;
    };
    peakHold (rmsShownDb,  rmsPeakDb,  rmsOver,  rmsOverHold);
    peakHold (lufsShownDb, lufsPeakDb, lufsOver, lufsOverHold);

    // GR ring glow: grows with how much we're compressing now (0..~12 dB).
    const float glowTarget = juce::jlimit (0.0f, 1.0f, -meterGrDb / 10.0f);
    ringGlow = ringGlow * 0.85f + glowTarget * 0.15f;

    // Orbiting-spark phase: always advances; a touch faster while the limiter works
    // (busier signal = livelier orbit). clipFlash latches on clip activity, decays.
    animPhase += 0.045f + 0.09f * juce::jlimit (0.0f, 1.0f, -limGrShownDb / 6.0f);
    if (animPhase > juce::MathConstants<float>::twoPi) animPhase -= juce::MathConstants<float>::twoPi;
    cyrLnf.animPhase = animPhase;   // let the knob LnF pulse its value arc in time
    clipFlash = juce::jmax (clipFlash * 0.80f, clipActShown);

    // --- Particle visibility: NONE below 5 dB of reduction, ramping to FULL at
    // ~60 dB. Heavily smoothed so the swarm fades in/out gently (no pop-in). ---
    const float grAmt   = -meterGrDb;                                  // dB of reduction (>=0)
    const float visTgt  = juce::jlimit (0.0f, 1.0f, (grAmt - 5.0f) / 55.0f);  // 5→0 .. 60→1
    partVisible += (visTgt - partVisible) * 0.06f;                    // slow, smooth fade

    // --- Particle field update (flow driven by the signal) ---
    // Compression accelerates the whole swarm; a rising clip edge spawns a burst of
    // bright short-lived sparks (like sparks flying off a hard-hit transient).
    const float flow = 1.0f + 2.2f * ringGlow;                  // compression = faster flow
    for (auto& p : particles)
    {
        p.ang += p.speed * flow;
        if (p.ang > juce::MathConstants<float>::twoPi) p.ang -= juce::MathConstants<float>::twoPi;
        if (p.ang < 0.0f)                              p.ang += juce::MathConstants<float>::twoPi;
        if (p.fade < 1.0f) p.fade = juce::jmin (1.0f, p.fade + 0.12f);  // smooth fade-in
        if (p.burst)       p.life -= 0.03f;                             // bursts fade out
    }
    // remove dead burst particles
    particles.erase (std::remove_if (particles.begin(), particles.end(),
                     [] (const Particle& p) { return p.burst && p.life <= 0.0f; }),
                     particles.end());
    // rising clip edge → spawn a ring of burst sparks
    if (clipActShown > prevClipAct + 0.05f && (int) particles.size() < kMaxParticles)
    {
        const int n = juce::jmin (6, kMaxParticles - (int) particles.size());
        for (int i = 0; i < n; ++i)
        {
            Particle p;
            p.ang        = partRng.nextFloat() * juce::MathConstants<float>::twoPi;
            p.radius     = 0.95f + partRng.nextFloat() * 0.35f;    // spawn just outside the ring
            p.speed      = (partRng.nextBool() ? 1.0f : -1.0f) * (0.03f + partRng.nextFloat() * 0.05f);
            p.size       = 1.4f + partRng.nextFloat() * 2.2f;
            p.life       = 1.0f;
            p.fade       = 0.0f;   // grows in smoothly (no pop-in)
            p.wobbleSeed = partRng.nextFloat() * juce::MathConstants<float>::twoPi;
            p.burst      = true;
            particles.push_back (p);
        }
    }
    prevClipAct = clipActShown;

    // Advance the animated background (~30 fps at the 30 Hz timer = 1 frame/tick).
    if (! gifFrames.isEmpty())
        gifFrame = (gifFrame + 1) % gifFrames.size();

    // Band knobs fade in/out smoothly when All Mix Comp toggles (veil drawn in
    // paint via bandFade). AMOUNT dims as the bands take over.
    const float fadeTarget = allMixButton.getToggleState() ? 1.0f : 0.0f;
    bandFade = bandFade * 0.80f + fadeTarget * 0.20f;
    amountKnob.setAlpha (1.0f - 0.6f * bandFade);

    // REGION REPAINT (performance): repaint ONLY the animated areas each tick — the
    // orb/particles centre, the two meters and the GR graph. The static cards, knob
    // bodies and header are NOT redrawn every frame, so playing audio no longer
    // makes the knobs feel laggy (the whole-editor repaint was the bottleneck).
    // These regions are in BASE-GRID coordinates and the container is what draws them,
    // so the repaint has to be asked of the container. Sent to the editor instead they
    // land in a component that paints nothing, and the meters, the GR graph and the orb
    // simply stop updating while the audio keeps running.
    if (! centreZone.isEmpty())      content.repaint (centreZone);
    if (! grGraphBounds.isEmpty())   content.repaint (grGraphBounds);
    if (! rmsMeterBounds.isEmpty())  content.repaint (rmsMeterBounds.expanded (2));
    if (! lufsMeterBounds.isEmpty()) content.repaint (lufsMeterBounds.expanded (2));

    // While the band-fade veil is animating, the band knobs also need refreshing.
    if (bandFade > 0.001f && bandFade < 0.999f && ! bandZone.isEmpty())
        content.repaint (bandZone);
}

//==============================================================================
// Animated GIF background, centred behind the AMOUNT knob. Darkened + vignetted
// so it never competes with the knobs sitting on top of it.
void HouseCompAudioProcessorEditor::drawBackgroundGif (juce::Graphics& g)
{
    if (gifFrames.isEmpty() || centreZone.isEmpty()) return;
    const juce::Image& frame = gifFrames.getReference (juce::jlimit (0, gifFrames.size()-1, gifFrame));
    if (! frame.isValid()) return;

    // Clip to the centre card so the art never spills into the side panels.
    juce::Graphics::ScopedSaveState save (g);
    auto cardArea = centreZone.toFloat().reduced (4.0f, 2.0f);
    juce::Path cardClip;
    cardClip.addRoundedRectangle (cardArea, 6.0f);
    g.reduceClipRegion (cardClip);

    // Orb fills the WHOLE card (edge to edge). fillDestination scales the frame to
    // cover the entire cardArea; clipped to the rounded card so corners stay clean.
    g.setOpacity (0.85f);
    g.drawImage (frame, cardArea,
                 juce::RectanglePlacement::centred | juce::RectanglePlacement::fillDestination);
    g.setOpacity (1.0f);

    // (No bottom veil — the orb is drawn clean, no rectangular fade mask.) The
    // control rows below still sit on the solid centre panel, which is painted
    // first in paint() before the orb, so they read fine over the lower orb area.
}

//==============================================================================
// Render the STATIC chrome (background + header + all card panels + shadows) once
// into chromeCache. Called from layoutControls(); paint() just blits the result.
void HouseCompAudioProcessorEditor::renderChrome()
{
    // Render at the CURRENT display scale, then draw it back down to base coords in
    // paint(). Rendering at a fixed 740x500 and letting the transform enlarge it made
    // the chrome soft on HiDPI / enlarged windows; this keeps it crisp.
    chromeScale = juce::jlimit (1.0f, 2.0f, appliedScale > 0.01f ? appliedScale : 1.0f);
    const int pw = juce::roundToInt (740.0f * chromeScale);
    const int ph = juce::roundToInt (500.0f * chromeScale);
    chromeCache = juce::Image (juce::Image::ARGB, pw, ph, true);
    juce::Graphics g (chromeCache);
    g.addTransform (juce::AffineTransform::scale (chromeScale));   // draw in base coords

    // Background gradient.
    juce::ColourGradient bg (pal::bgTop, 0, 0, pal::bgBot, 0, 500.0f, false);
    g.setGradientFill (bg);
    g.fillAll();

    // Header bar.
    {
        g.setColour (pal::headerBg);
        g.fillRect (juce::Rectangle<float> (0.0f, 0.0f, 740.0f, 48.0f));
        g.setColour (pal::panelEdge);
        g.drawLine (0.0f, 48.0f, 740.0f, 48.0f, 1.0f);
        juce::ColourGradient hs (juce::Colours::black.withAlpha (0.28f), 0, 48.0f,
                                 juce::Colours::transparentBlack, 0, 58.0f, false);
        g.setGradientFill (hs);
        g.fillRect (juce::Rectangle<float> (0.0f, 48.0f, 740.0f, 10.0f));
    }

    // Card panels (drop-shadow + fill + gloss + edge). Same look as before.
    auto card = [&g] (juce::Rectangle<int> r)
    {
        if (r.isEmpty()) return;
        auto f = r.toFloat();
        juce::Path shape;
        shape.addRoundedRectangle (f, 6.0f);
        melatonin::DropShadow { juce::Colours::black.withAlpha (0.55f), 14, { 0, 4 } }.render (g, shape);
        g.setColour (pal::panel);
        g.fillRoundedRectangle (f, 6.0f);
        juce::ColourGradient gloss (juce::Colours::white.withAlpha (0.045f), f.getX(), f.getY(),
                                    juce::Colours::transparentBlack, f.getX(), f.getY() + 18.0f, false);
        g.setGradientFill (gloss);
        g.fillRoundedRectangle (f, 6.0f);
        g.setColour (pal::panelEdge);
        g.drawRoundedRectangle (f.reduced (0.5f), 6.0f, 1.0f);
    };
    if (! centreZone.isEmpty()) card (centreZone.reduced (4, 2));
    for (auto& c : bandCards) card (c);
    card (ioCard);
}

//==============================================================================
// The editor itself paints nothing: everything is drawn by the container, on the same
// 740x500 grid the controls are laid out on, so clip paths and drawing always agree.
void HouseCompAudioProcessorEditor::Content::paint (juce::Graphics& g)
{
    if (owner != nullptr) owner->paintContent (g);
}

void HouseCompAudioProcessorEditor::paint (juce::Graphics&)
{
    // Deliberately empty — see Content::paint above.
}

void HouseCompAudioProcessorEditor::paintContent (juce::Graphics& g)
{
    // Static chrome: one image blit instead of re-rendering gradient+header+6 cards+
    // 7 shadow blurs every frame. Built lazily / on layout / on scale change.
    if (! chromeCache.isValid())
        renderChrome();
    // The cache is stored at chromeScale; draw it back into base 740x500 coords.
    g.drawImage (chromeCache, juce::Rectangle<float> (0.0f, 0.0f, 740.0f, 500.0f),
                 juce::RectanglePlacement::stretchToFit);

    drawBackgroundGif (g);

    // --- GR ring around the big knob (neon cyan, glows with compression) ----
    auto kb = amountKnob.getBounds().toFloat().expanded (10.0f);
    const float meterAmount = juce::jlimit (0.0f, 1.0f, -meterGrDb / 18.0f);

    // A SMALL, soft dark pool only right behind the readout text so "27.1" stays
    // legible — kept tight so it doesn't grey out the whole orb.
    {
        auto pool = kb.withSizeKeepingCentre (kb.getWidth() * 0.62f, kb.getHeight() * 0.62f);
        juce::ColourGradient sg (pal::bgBot.withAlpha (0.55f), kb.getCentreX(), kb.getCentreY(),
                                 pal::bgBot.withAlpha (0.0f),  pool.getRight(),  kb.getCentreY(), true);
        g.setGradientFill (sg);
        g.fillEllipse (pool);
    }

    const float cxR = kb.getCentreX(), cyR = kb.getCentreY();
    const float baseR = kb.getWidth() * 0.5f - 4.0f;

    // (A) CLIP SCAN-FLASH: when the clipper slices, a bright cyan halo pulses out
    // from the ring and a thin horizontal scan-line sweeps the orb. Ties the visual
    // to the one moment the compressor is doing its most aggressive work.
    if (clipFlash > 0.01f)
    {
        g.setColour (pal::cyan.withAlpha (0.10f * clipFlash));
        g.drawEllipse (kb.reduced (4.0f).expanded (6.0f * clipFlash), 2.0f + 3.0f * clipFlash);
        const float scanY = cyR + std::sin (animPhase * 3.0f) * baseR * 0.9f;
        juce::ColourGradient sc (pal::cyan.withAlpha (0.0f),               cxR - baseR, scanY,
                                 pal::cyan.withAlpha (0.0f),               cxR + baseR, scanY, false);
        sc.addColour (0.5, pal::cyan.withAlpha (0.30f * clipFlash));
        g.setGradientFill (sc);
        g.fillRect (juce::Rectangle<float> (cxR - baseR, scanY - 1.0f, baseR * 2.0f, 2.0f));
    }

    // Base track ring, then the PULSING value arc.
    g.setColour (pal::track.withAlpha (0.9f));
    g.drawEllipse (kb.reduced (4.0f), 2.5f);
    juce::Path arc;
    const float startA = juce::degreesToRadians (135.0f);
    const float endA   = startA + juce::degreesToRadians (270.0f) * meterAmount;
    arc.addCentredArc (cxR, cyR, baseR, baseR, 0.0f, startA, endA, true);

    // GR ring: plain line only — NO glow halo around the orb.
    g.setColour (pal::cyan.brighter (0.4f * ringGlow));
    g.strokePath (arc, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved,
                                             juce::PathStrokeType::rounded));

    // (D) PARTICLE FIELD: a generative swarm orbiting the ring (flow-field idea).
    // Only visible WHILE compressing (partVisible fades 0 below 5 dB → 1 near 60 dB),
    // so the swarm gently appears under load and dissolves when the comp relaxes.
    if (partVisible > 0.003f)
    {
        const float ringR = baseR;
        const float vis   = partVisible;                 // global fade multiplier
        for (const auto& p : particles)
        {
            // radius wobbles gently over time (flow-field feel), scaled off the ring
            const float wob = std::sin (animPhase * 1.7f + p.wobbleSeed) * 0.06f;
            const float rad = ringR * (p.radius + wob) + 4.0f;
            const float px  = cxR + std::cos (p.ang) * rad;
            const float py  = cyR + std::sin (p.ang) * rad;

            const float twinkle = 0.55f + 0.45f * std::sin (animPhase * 2.3f + p.wobbleSeed);
            const float life    = (p.burst ? p.life : 1.0f) * p.fade;   // include fade-in

            // Pseudo-3D DEPTH: treat the orbit as tilted — particles at the front
            // (sin(ang) > 0, lower half) come "toward" us: bigger, brighter, sharper;
            // particles at the back recede: smaller, dimmer, softer. z in -1..+1.
            const float z      = std::sin (p.ang);                 // -1 back .. +1 front
            const float depth  = 0.55f + 0.45f * z;                // 0.1 far .. 1.0 near
            const float baseA  = (p.burst ? 0.80f : 0.32f) * life * depth * vis;   // fade with compression

            // motion trail behind the particle (fades with depth too)
            const int trailN = p.burst ? 5 : 3;
            for (int t = 1; t <= trailN; ++t)
            {
                const float ta = p.ang - (float) t * (p.speed > 0 ? 0.06f : -0.06f);
                const float tx = cxR + std::cos (ta) * rad;
                const float ty = cyR + std::sin (ta) * rad;
                const float tr = p.size * depth * (1.0f - 0.16f * (float) t);
                g.setColour (pal::cyan.withAlpha (baseA * (0.5f + 0.5f * twinkle) * (1.0f - (float) t / (trailN + 1))));
                g.fillEllipse (tx - tr, ty - tr, tr * 2.0f, tr * 2.0f);
            }

            // radius scales with depth so near beads are visibly larger (3D read)
            const float rr = p.size * depth * (0.95f + 0.4f * twinkle);

            // 1) soft outer glow
            g.setColour (pal::cyan.withAlpha (baseA * (0.35f + 0.4f * twinkle)));
            g.fillEllipse (px - rr * 2.0f, py - rr * 2.0f, rr * 4.0f, rr * 4.0f);

            // 2) SPHERICAL body: radial gradient, bright core offset to the upper-left,
            //    fading to a darker cyan rim — this is what makes it read as a 3D bead.
            juce::Rectangle<float> body (px - rr, py - rr, rr * 2.0f, rr * 2.0f);
            juce::ColourGradient sph (juce::Colours::white.withAlpha (0.95f * life * vis),
                                      px - rr * 0.35f, py - rr * 0.4f,
                                      pal::cyanDim.withAlpha (0.15f * life * vis),
                                      px + rr, py + rr, true);
            sph.addColour (0.45, pal::cyan.withAlpha (0.9f * depth * life * vis));
            g.setGradientFill (sph);
            g.fillEllipse (body);

            // 3) tiny specular highlight (top-left) — the wet-glass glint
            const float sr = rr * 0.4f;
            g.setColour (juce::Colours::white.withAlpha (0.85f * life * depth * vis));
            g.fillEllipse (px - rr * 0.45f - sr * 0.5f, py - rr * 0.5f - sr * 0.5f, sr, sr);
        }
    }

    // Fade veil over the band knobs when All Mix Comp is OFF (smooth 0..1).
    if (bandFade < 0.99f && ! bandZone.isEmpty())
    {
        g.setColour (pal::bgBot.withAlpha ((1.0f - bandFade) * 0.55f));
        g.fillRect (bandZone);
    }

    drawGrGraph (g);
    drawMeter (g, rmsMeterBounds,  rmsShownDb,  "RMS",  6.0f, -60.0f, rmsPeakDb,  rmsOver);
    drawMeter (g, lufsMeterBounds, lufsShownDb, "LUFS", 6.0f, -60.0f, lufsPeakDb, lufsOver);
}

//==============================================================================
// Vertical bar meter with a dB scale and a numeric readout on top.
void HouseCompAudioProcessorEditor::drawMeter (juce::Graphics& g, juce::Rectangle<int> b,
                                               float valueDb, const juce::String& caption,
                                               float topDb, float botDb,
                                               float peakDb, bool over)
{
    if (b.isEmpty()) return;
    auto area = b.toFloat();

    // Caption ABOVE the meter, with a small OVER lamp to its right.
    auto cap = area.removeFromTop (14.0f);
    {
        auto lamp = cap.removeFromRight (12.0f).withSizeKeepingCentre (6.0f, 6.0f);
        g.setColour (over ? juce::Colour (0xffff5252) : pal::panelEdge);
        g.fillEllipse (lamp);
        if (over)
        {
            g.setColour (juce::Colour (0xffff5252).withAlpha (0.35f));
            g.fillEllipse (lamp.expanded (2.5f));
        }
    }
    g.setColour (pal::textDim);
    g.setFont (cyrLnf.withDruk (9.5f));
    g.drawText (caption, cap, juce::Justification::centred);

    // Numeric readout box (under the caption) — a recessed value well: dark fill,
    // soft top inner-shadow, crisp edge, cyan Druk value floating in it.
    auto head = area.removeFromTop (22.0f);
    g.setColour (juce::Colour (0xff141a21));
    g.fillRoundedRectangle (head, 4.0f);
    {
        juce::ColourGradient inner (juce::Colours::black.withAlpha (0.30f), head.getX(), head.getY(),
                                    juce::Colours::transparentBlack, head.getX(), head.getCentreY(), false);
        g.setGradientFill (inner);
        g.fillRoundedRectangle (head, 4.0f);
    }
    g.setColour (pal::panelEdge);
    g.drawRoundedRectangle (head.reduced (0.5f), 4.0f, 1.0f);
    g.setColour (pal::cyan);
    g.setFont (cyrLnf.withDruk (11.0f));
    g.drawText (juce::String (valueDb, 1), head, juce::Justification::centred);

    area.removeFromTop (3.0f);

    // Bar track = a recessed channel: dark bed + top inner-shadow so the level bar
    // reads as light sitting inside a sunk well.
    auto scaleCol = area.removeFromLeft (16.0f);
    auto track = area.reduced (0.0f, 2.0f);
    g.setColour (juce::Colour (0xff141a21));
    g.fillRoundedRectangle (track, 4.0f);
    {
        juce::ColourGradient inner (juce::Colours::black.withAlpha (0.32f), track.getX(), track.getY(),
                                    juce::Colours::transparentBlack, track.getX(), track.getY() + 10.0f, false);
        g.setGradientFill (inner);
        g.fillRoundedRectangle (track, 4.0f);
    }
    g.setColour (pal::panelEdge);
    g.drawRoundedRectangle (track.reduced (0.5f), 4.0f, 1.0f);

    auto dbToY = [&] (float dB)
    { return juce::jmap (juce::jlimit (botDb, topDb, dB), botDb, topDb, track.getBottom(), track.getY()); };

    // dB scale ticks down the left side (0, -12, -24, -36, -48, -60).
    g.setFont (cyrLnf.withDruk (7.5f));
    for (int dB = 0; dB >= -60; dB -= 12)
    {
        const float y = dbToY ((float) dB);
        g.setColour (pal::textDim.withAlpha (0.55f));
        g.drawText (juce::String (dB), scaleCol.withY (y - 6.0f).withHeight (12.0f).reduced (1, 0),
                    juce::Justification::centredRight);
        g.setColour (pal::panelEdge.withAlpha (0.7f));
        g.drawLine (track.getX(), y, track.getRight(), y, 0.5f);
    }

    // Level fill: neon cyan gradient, brighter toward the top.
    const float yVal = dbToY (valueDb);
    if (yVal < track.getBottom() - 1.0f)
    {
        auto fill = juce::Rectangle<float> (track.getX() + 1.5f, yVal,
                                            track.getWidth() - 3.0f, track.getBottom() - yVal - 1.0f);
        juce::ColourGradient grad (pal::cyan, fill.getX(), track.getY(),
                                   pal::cyanDim, fill.getX(), track.getBottom(), false);
        g.setGradientFill (grad);
        g.fillRoundedRectangle (fill, 1.5f);
    }

    // Peak-hold marker.
    const float yPk = dbToY (peakDb);
    g.setColour (over ? juce::Colour (0xffff5252) : juce::Colours::white.withAlpha (0.9f));
    g.fillRect (juce::Rectangle<float> (track.getX() + 1.5f, yPk - 0.5f, track.getWidth() - 3.0f, 1.5f));
}

//==============================================================================
void HouseCompAudioProcessorEditor::drawGrGraph (juce::Graphics& g)
{
    auto area = grGraphBounds.toFloat();
    if (area.isEmpty()) return;

    // Raised card treatment (same shadow as the other panels) so the graph matches
    // the card system instead of sitting flat.
    {
        juce::Path sh; sh.addRoundedRectangle (area, 6.0f);   // 6.0 to match the card system
        melatonin::DropShadow { juce::Colours::black.withAlpha (0.55f), 14, { 0, 4 } }.render (g, sh);
    }
    g.setColour (pal::panel);
    g.fillRoundedRectangle (area, 6.0f);
    g.setColour (pal::panelEdge);
    g.drawRoundedRectangle (area.reduced (0.5f), 6.0f, 1.0f);

    // CLIP everything below to the graph frame so waveform/GR peaks and the clip
    // bar can NEVER spill above or below the box onto the artwork or the panel.
    juce::Graphics::ScopedSaveState clipToFrame (g);
    g.reduceClipRegion (area.getSmallestIntegerContainer().reduced (1));

    const float fullDb = 24.0f;
    g.setColour (pal::panelEdge.withAlpha (0.55f));
    for (float dB = 6.0f; dB < fullDb; dB += 6.0f)
    {
        const float y = area.getY() + area.getHeight() * (dB / fullDb);
        g.drawHorizontalLine ((int) y, area.getX(), area.getRight());
    }

    // (Input-waveform scope removed — the blue mirror waveform was visual clutter.)

    // GR curves — TWO layers so you honestly see who's pulling down:
    //   cyan fill  = compressor-only reduction
    //   amber line = TOTAL reduction (comp + limiter); the gap between them is the
    //                limiter's contribution.
    //   amber ticks= clip activity along the bottom (brighter = harder slicing).
    const int   n  = HouseCompAudioProcessor::kHistorySize;
    const int   wp = proc.historyWritePos.load();

    auto buildPath = [&] (const std::array<std::atomic<float>, HouseCompAudioProcessor::kHistorySize>& hist)
    {
        juce::Path p; bool st = false;
        for (int i = 0; i < n; ++i)
        {
            const int idx = (wp + i) % n;
            const float gr = hist[(size_t) idx].load();
            const float t  = (float) i / (float) (n - 1);
            const float x  = area.getX() + t * area.getWidth();
            const float y  = area.getY() + juce::jlimit (0.0f, 1.0f, -gr / fullDb) * area.getHeight();
            if (! st) { p.startNewSubPath (x, y); st = true; }
            else       p.lineTo (x, y);
        }
        return p;
    };

    // Compressor-only, filled CYAN (the primary accent — this is the main meter).
    juce::Path compPath = buildPath (proc.grHistoryComp);
    juce::Path compFill = compPath;
    compFill.lineTo (area.getRight(), area.getY());
    compFill.lineTo (area.getX(),     area.getY());
    compFill.closeSubPath();
    g.setColour (pal::cyan.withAlpha (0.18f));
    g.fillPath (compFill);
    g.setColour (pal::cyan);
    g.strokePath (compPath, juce::PathStrokeType (1.5f));

    // Total line on top (gap above cyan = limiter action) — the ONE warm accent.
    juce::Path totalPath = buildPath (proc.grHistory);
    g.setColour (pal::amber);
    g.strokePath (totalPath, juce::PathStrokeType (1.5f));

    // Clip activity: amber ticks along the bottom (same warm accent, not a new red).
    {
        const float baseY = area.getBottom() - 3.0f;
        for (int i = 0; i < n; ++i)
        {
            const int idx = (wp + i) % n;
            const float act = juce::jlimit (0.0f, 1.0f, proc.clipHistory[(size_t) idx].load());
            if (act <= 0.001f) continue;
            const float t = (float) i / (float) (n - 1);
            const float x = area.getX() + t * area.getWidth();
            g.setColour (pal::amber.withAlpha (0.35f + 0.65f * act));
            g.fillRect (juce::Rectangle<float> (x, baseY - 3.0f * act, area.getWidth() / (float) n + 1.0f, 3.0f + 3.0f * act));
        }
    }

    // Legend + total readout.
    g.setFont (cyrLnf.withDruk (9.5f));
    g.setColour (pal::cyan);
    g.drawText ("COMP " + juce::String (compGrShownDb, 1),
                grGraphBounds.reduced (6, 3).removeFromTop (14), juce::Justification::topLeft);
    g.setColour (pal::amber);
    g.drawText ("LIM " + juce::String (limGrShownDb, 1),
                grGraphBounds.reduced (6, 3).removeFromTop (14), juce::Justification::topRight);
}

//==============================================================================
// resized() runs on EVERY pixel of a drag. To keep the drag buttery-smooth we do
// NOT re-run the (heavy) child layout each time — the children live in fixed base
// 740x500 coordinates under an AffineTransform, so we lay them out ONCE and then
// only update the scale transform as the window grows. That removes the jitter
// (recomputing dozens of setBounds + re-triggering resized() every pixel).
void HouseCompAudioProcessorEditor::resized()
{
    if (! laidOut) { layoutControls(); laidOut = true; }

    // The editor's own size is whatever the host says — it is never written back here,
    // so there is no loop to guard against and no epsilon test to tune. getWidth() is
    // now a plain, trustworthy input.
    const float scale = juce::jlimit (0.5f, 3.0f, (float) getWidth() / 740.0f);

    // The container always holds the base grid and carries the whole scale.
    content.setBounds (0, 0, 740, 500);
    content.setTransform (juce::AffineTransform::scale (scale));

    if (std::abs (scale - appliedScale) > 0.005f)
    {
        appliedScale = scale;
        // Chrome is cached art re-rendered at the current scale so it stays sharp rather
        // than being blitted up soft. Far too expensive to redo per drag pixel, so it
        // waits for the drag to settle.
        scheduleChromeRender();
    }

    saveWindowSize (getWidth());
}

// Coalesces chrome re-renders: each call restarts the clock, so a continuous drag only
// pays for ONE render, once the user stops moving.
void HouseCompAudioProcessorEditor::scheduleChromeRender()
{
    chromeTimer.owner = this;
    chromeTimer.startTimer (120);
}

void HouseCompAudioProcessorEditor::ChromeTimer::timerCallback()
{
    stopTimer();
    if (owner != nullptr) owner->renderChrome();
}

void HouseCompAudioProcessorEditor::layoutControls()
{
    auto r = juce::Rectangle<int> (0, 0, 740, 500);   // base layout size
    linksOverlay.setBounds (r);   // full-editor overlay for the animated links card

    // ---- HEADER: badge | title + byline (once) | A = B | language ---------
    auto titleRow = r.removeFromTop (48);
    langButton.setBounds  (740 - 46, titleRow.getY() + 13, 36, 22);
    // Title CENTRED, byline stacked directly underneath it.
    // 24 px of height would crop a 28 px face — the row has to clear the type, and the
    // byline moves down to match so the two do not collide.
    titleLabel.setBounds  (0, titleRow.getY() + 2,  740, 32);
    bylineLabel.setBounds (0, titleRow.getY() + 32, 740, 15);
    {
        auto ab = juce::Rectangle<int> (740 - 178, titleRow.getY() + 13, 124, 22);
        abAButton.setBounds     (ab.removeFromLeft (44));
        abMatchButton.setBounds (ab.removeFromLeft (32).reduced (2, 0));
        abBButton.setBounds     (ab.removeFromLeft (44));
    }

    r.removeFromTop (6);

    // Top toggles (SPECTRAL removed from the UI — it's always on internally).
    auto toggles = r.removeFromTop (24);
    allMixButton.setBounds    (toggles.removeFromLeft (140).reduced (10, 0));
    bypassButton.setBounds    (toggles.removeFromLeft (95).reduced (6, 0));
    lookaheadButton.setBounds (toggles.removeFromLeft (118).reduced (6, 0));
    // ENGINE: CLASSIC | MODERN | FUTURE — the sound-character switch, right of the toggles.
    engineSwitch.setBounds    (toggles.removeFromLeft (140).withSizeKeepingCentre (132, 20));
    // FREE | BOND belongs to FUTURE, so it sits immediately beside the engine switch.

    // ---- RIGHT: RMS + LUFS meters (fill the full column height) -----------
    auto meters = r.removeFromRight (124).withTrimmedTop (2).withTrimmedBottom (6);
    rmsMeterBounds  = meters.removeFromLeft (62).reduced (8, 2);
    lufsMeterBounds = meters.reduced (8, 2);

    // ---- LEFT: 4 band knobs (2x2) then INPUT/OUTPUT, filling the WHOLE column ----
    // Split the left column proportionally so there is NO dead gap: the band grid
    // takes the upper ~62%, the IO card the lower ~34%, with one small gutter.
    auto left = r.removeFromLeft (212);
    {
        const int osH   = 30;                               // OVERSAMPLE strip height
        const int ioH   = 118;                              // input/output card height
        const int gutter = 8;
        // Bottom of the left column, top→bottom: [ IO card ] [gutter] [ OVERSAMPLE ].
        auto osRow  = left.removeFromBottom (osH);          // OVERSAMPLE pinned to very bottom
        left.removeFromBottom (gutter);
        auto ioRow  = left.removeFromBottom (ioH);          // IO card above it
        left.removeFromBottom (gutter);                     // gutter to the band grid
        auto grid = left.reduced (8, 0);                    // band grid fills the REST
        bandZone = grid;                                    // remember for the fade veil
        auto rowTop = grid.removeFromTop (grid.getHeight() / 2);
        auto rowBot = grid;
        juce::Rectangle<int> cells[4] = {
            rowTop.removeFromLeft (rowTop.getWidth()/2), rowTop,
            rowBot.removeFromLeft (rowBot.getWidth()/2), rowBot
        };
        for (int b = 0; b < 4; ++b)
        {
            auto cell = cells[b].reduced (3);
            bandCards[b] = cell;                 // card panel is drawn in paint()
            auto inner = cell.reduced (4);
            bandLabel[b].setBounds (inner.removeFromTop (13));
            bandKnob[b].setBounds (inner.withSizeKeepingCentre (72, 72));
        }

        // INPUT + OUTPUT side by side in one card, pinned to the column bottom.
        auto bottom = ioRow.reduced (8, 4);
        ioCard = bottom;                          // card panel is drawn in paint()
        auto inner  = bottom.reduced (5);
        auto inCol  = inner.removeFromLeft (inner.getWidth() / 2);
        auto outCol = inner;
        gainLabel.setBounds (inCol.removeFromTop (14));
        inGainKnob.setBounds (inCol.withSizeKeepingCentre (62, 62));   // a touch smaller
        outLabel.setBounds (outCol.removeFromTop (14));
        outGainKnob.setBounds (outCol.withSizeKeepingCentre (62, 62));

        // OVERSAMPLE relocated here (under INPUT/OUTPUT): label left, combo right.
        auto os = osRow.reduced (8, 4);
        osLabel.setBounds (os.removeFromLeft (78).withSizeKeepingCentre (78, 16));
        osBox.setBounds   (os.removeFromLeft (64).withSizeKeepingCentre (64, 22));
    }

    // ---- CENTRE: AMOUNT with ATTACK (left) and RELEASE (right) ----
    auto centre = r;  // remaining middle column
    // The centre card (orb + knobs + the control rows) ends just below the last row; a
    // small gap separates it from the GR graph card, which keeps its generous height
    // (all the leftover space below the gap).
    // kCtrlRows MUST match the number of rows actually laid out below — adding a row
    // without bumping it pushes that row outside the card and over the graph.
    const int kCtrlRows = 3;                              // UPWARD, CHARACTER, CLIP SHAPE
    const int centreH = 224 + kCtrlRows * 28 + 8;
    const int gap     = 8;                                // breathing gap (like left column)
    centreZone = { centre.getX() - 2, centre.getY(),
                   centre.getWidth() + 4, centreH };
    auto grRow = centre.withTrimmedTop (centreH + gap);   // GR graph = everything below the gap
    auto knobZone = centre.removeFromTop (224);           // orb + big knob + ATK/REL

    // AMOUNT knob centred on the ORB (= geometric centre of the centre card, which is
    // where the gif is centred). Gif itself is NOT moved — only the knob is re-centred.
    const int orbCentreY = centreZone.getY() + centreZone.getHeight() / 2;
    auto amt = knobZone.withSizeKeepingCentre (186, 186);
    amt.setCentre (knobZone.getCentreX(), orbCentreY);
    amountKnob.setBounds (amt);
    // COMPRESSION caption above the knob.
    amountLabel.setBounds (amt.getX() - 20, amt.getY() - 50, amt.getWidth() + 40, 16);

    // ATTACK left of the sphere, RELEASE right of it — vertically centred on the orb.
    auto aCol = juce::Rectangle<int> (knobZone.getX() + 2, orbCentreY - 46, 92, 92);
    atkLabel.setBounds (aCol.removeFromTop (13));
    atkKnob.setBounds (aCol.withSizeKeepingCentre (76, 76));
    atkCycKnob.setBounds (atkKnob.getBounds());   // stacked; visibility picks which shows
    auto rCol = juce::Rectangle<int> (knobZone.getRight() - 94, orbCentreY - 46, 92, 92);
    relLabel.setBounds (rCol.removeFromTop (13));
    relKnob.setBounds (rCol.withSizeKeepingCentre (76, 76));
    relNoteKnob.setBounds (relKnob.getBounds());   // stacked; visibility picks which shows
    // The AUTO letter sits INSIDE the RELEASE knob, low enough to stay clear of the
    // value readout in the middle. Small hit area, but the glow makes it findable.
    // ON the knob's face, in its LOWER third — the centre belongs to the value readout,
    // and sitting there pushed the value out from under the knob entirely.
    relAutoButton.setBounds (juce::Rectangle<int> (34, 12)
                                 .withCentre ({ relKnob.getBounds().getCentreX(),
                                                relKnob.getBounds().getCentreY()
                                                    + relKnob.getHeight() / 4 }));

    // Three aligned control rows in fixed columns. Left = a labelled slider, right = a
    // segmented mode-switch. All three left sliders share one width and one baseline,
    // so UPWARD lines up exactly with CHARACTER and CLIP SHAPE:
    //   UPWARD     [====]   [ HI-Q  | ACR   ]
    //   CHARACTER  [====]   [ SMART | IRC IV]
    //   CLIP SHAPE [====]   [ BEAT  | SMART ]
    const int colA = 150;                   // label + its control (combo / slider)
    const int labelW = 78;                  // shared left-label width -> controls align
    const int swPadR = 8;                   // gap from the right edge of the centre

    // Switch sits flush to the RIGHT of the centre column. Slightly smaller than
    // before (108x20) and vertically centred on the SAME row rect the left control
    // uses — so every right switch lines up exactly with its left counterpart.
    const int swWs = 94;    // smaller — to match the compact scale of the left column
    const int swHs = 18;
    auto placeSwitch = [&] (juce::Rectangle<int> row, SegmentedSwitch& a)
    {
        auto cell = row.removeFromRight (swWs + swPadR);
        cell.removeFromRight (swPadR);
        a.setBounds (cell.withSizeKeepingCentre (swWs, swHs));
    };

    // All three rows are the SAME height so the left controls and right switches
    // share one baseline grid (frontend-design: alignment is structure).
    const int rowH = 28;

    auto row1 = centre.removeFromTop (rowH);
    {
        // Left column of row 1 was empty; UPWARD lives there now, mirroring the
        // CHARACTER / CLIP SHAPE sliders in the rows below.
        auto la = row1.removeFromLeft (colA);
        upLabel.setBounds (la.removeFromLeft (labelW).withSizeKeepingCentre (labelW, 14));
        upKnob.setBounds  (la.reduced (2, 0).withSizeKeepingCentre (la.getWidth() - 4, swHs));
        placeSwitch (row1, clipSwitch);
    }

    auto row2 = centre.removeFromTop (rowH);
    {
        auto la = row2.removeFromLeft (colA);
        charLabel.setBounds (la.removeFromLeft (labelW).withSizeKeepingCentre (labelW, 14));
        charKnob.setBounds  (la.reduced (2, 0).withSizeKeepingCentre (la.getWidth() - 4, swHs));
        placeSwitch (row2, limSwitch);
    }

    auto row3 = centre.removeFromTop (rowH);
    {
        auto la = row3.removeFromLeft (colA);
        clipShapeLabel.setBounds (la.removeFromLeft (labelW).withSizeKeepingCentre (labelW, 14));
        clipShapeKnob.setBounds  (la.reduced (2, 0).withSizeKeepingCentre (la.getWidth() - 4, swHs));
        // BEAT | SMART takes the slot AUTO | LUFS used to occupy. It belongs in the
        // switch column rather than under the RELEASE knob — that space is the UPWARD
        // slider's, and putting it there made the two overlap.
        placeSwitch (row3, relTypeSwitch);
    }

    // GR graph = the strip carved off the bottom earlier (grRow), as its own card.
    grGraphBounds = grRow.reduced (2, 0);

    // Now that all card rects are known, (re)build the static-chrome cache image.
    renderChrome();
}
