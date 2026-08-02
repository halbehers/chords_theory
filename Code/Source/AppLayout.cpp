#include "AppLayout.h"

#include "AppLocalisation.h"
#include "Theory/ChordDatabase.h"
#include "Theory/MidiExporter.h"
#include "Theory/NoteConvertor.h"
#include "Theory/ResolvedProgressionSlot.h"
#include "Theory/SessionStateSerializer.h"

AppLayout::AppLayout(ndsp::ParameterManager& parameterManager, audio::ChordSynthEngine& synthEngine):
    nlayout::AppLayout(parameterManager),
    _synthEngine(synthEngine),
    _settings("settings", nui::Icons::getGear()),
    _keyScaleSelector("key-scale-selector"),
    _chordBrowser("chord-degree-browser"),
    _progressionSequencer("progression-sequencer", [this](const theory::ProgressionSlot& slot) { return _chordBrowser.resolveSlot(slot); }),
    _synthEditor(parameterManager),
    _mainSection("main-section", parameterManager),
    _windowsManager(*this)
{
    _settings.setIconSize(24.f);
    _settings.addOnClickListener(this);
    _windowsManager.createWindow(std::make_unique<component::SettingsWindow>("settings", _windowsManager));

    _keyScaleSelector.addListener(this);

    _chordBrowser.addListener(this);
    _chordBrowser.setKeyAndScale(_keyScaleSelector.getKey(), _keyScaleSelector.getScale());

    _progressionSequencer.addListener(this);
    _progressionSequencer.setScale(_keyScaleSelector.getScale());

    AppLocalisation::getChangeBroadcaster().addChangeListener(this);

    _mainSection.setTabsBackgroundColour(nui::Theme::newColor(nui::Theme::BACKGROUND).asJuce());
    _mainSection.setTabsSelectedBackgroundColour(nui::Theme::newColor(nui::Theme::ACCENT).asJuce().withAlpha(.2f));
    _mainSection.setTabsBorderColour(juce::Colours::transparentBlack);
    _mainSection.setTabsSelectedBorderColour(nui::Theme::newColor(nui::Theme::ACCENT).asJuce());
    _mainSection.setTabsSelectedTextColour(nui::Theme::newColor(nui::Theme::INVERTED_TEXT).asJuce());
    _mainSection.setTabsFontSize(nui::Theme::PARAGRAPH);
    _mainSection.setTabsHeightType(nui::Theme::HeightType::THIN);

    _mainSection.addOnPanelChangedListener(this);

    _mainSection.setPanelName(MAIN_PANEL_ID, juce::translate("chords_tab_label").toStdString());
    _mainSection.addPanel("synth-tab", juce::translate("synth_tab_label").toStdString());

    _mainSection.getLayout().setDisplayGrid(false);
    _mainSection.getLayout().init({ 1, 1, 1 }, { 1 });
    _mainSection.getLayout().setFixedRowHeight(0, 80.f);
    _mainSection.getLayout().setFixedRowHeight(1, 80.f);
    _mainSection.getLayout().addComponent(_chordBrowser, 0, 0, 1, 1);
    _mainSection.getLayout().addComponent(_voicingSelector, 1, 0, 1, 1);
    _mainSection.getLayout().addComponent(_progressionSequencer, 2, 0, 1, 1);

    _mainSection.getLayout("synth-tab").setDisplayGrid(false);
    _mainSection.getLayout("synth-tab").init({ 1 }, { 1 });
    _mainSection.getLayout("synth-tab").addComponent(_synthEditor, 0, 0, 1, 1);

    getLayout().setGap(16.f);
    getLayout().setDisplayGrid(false);
    getLayout().setResizableLineConfiguration({ .displayLine = false });

#if JUCE_MAC
    getLayout().setMargin(24.f, 24.f + 16.f, 24.f, 24.f);
#else
    getLayout().setMargin(24.f, 0.f, 24.f, 24.f);
#endif

    getLayout().init({ 1, 1 }, { 1, 1 }); // row0: settings/key-scale (fixed height), row1: _mainSection (flexible)

    getLayout().setFixedRowHeight(0, 60.f);
    getLayout().setFixedColumnWidth(0, 32.f);

    getLayout().addComponent(_settings, 0, 0, 1, 1);
    getLayout().addComponent(_keyScaleSelector, 0, 1, 1, 1);
    getLayout().addComponent(_mainSection, 1, 0, 2, 1);

    _voicingSelector.setVisible(false); // must come after addComponent(), which calls addAndMakeVisible() internally

    restoreStateFromValueTree();
}

AppLayout::~AppLayout()
{
    AppLocalisation::getChangeBroadcaster().removeChangeListener(this);
    _mainSection.removeListener(this);

    _settings.removeListener(this);
    _keyScaleSelector.removeListener(this);
    _chordBrowser.removeListener(this);
    _progressionSequencer.removeListener(this);
}

void AppLayout::changeListenerCallback(juce::ChangeBroadcaster* source)
{
    nlayout::AppLayout::changeListenerCallback(source);

    if (source != &AppLocalisation::getChangeBroadcaster())
        return;

    _mainSection.setPanelName(MAIN_PANEL_ID, juce::translate("chords_tab_label").toStdString());
    _mainSection.setPanelName("synth-tab", juce::translate("synth_tab_label").toStdString());
}

void AppLayout::onPanelChanged(const std::string& newPanelID)
{
    if (newPanelID != MAIN_PANEL_ID)
        return;

    _voicingSelector.setVisible(_openVoicingDegree.has_value());
    updateVoicingSelectorArrow();
}

void AppLayout::resized()
{
    nlayout::AppLayout::resized();

    _windowsManager.setBounds(getLocalBounds());

    updateVoicingSelectorArrow();
}

void AppLayout::onButtonClick(const std::string& componentID)
{
    if (componentID == _settings.getComponentID())
    {
        _windowsManager.showWindow("settings");
    }
}

void AppLayout::onKeyScaleChanged(theory::Key key, theory::Scale scale)
{
    // setKeyAndScale() destroys and rebuilds every ChordCard from scratch - close the voicing
    // selector first so it never ends up pointing at a destroyed card or showing voicings for a
    // degree that doesn't exist in the new scale (e.g. Minor Blues only has I/IV/V).
    _voicingSelector.close();
    _openVoicingDegree.reset();

    _chordBrowser.setKeyAndScale(key, scale);
    _progressionSequencer.setScale(scale);

    syncStateToValueTree();
}

void AppLayout::onChordChanged(theory::Degree degree, const theory::Chord& newChord)
{
    juce::ignoreUnused(degree, newChord);

    syncStateToValueTree();
}

void AppLayout::onChordDragStarted(theory::Degree degree, const theory::Chord& chord)
{
    const auto midiFile = theory::MidiExporter::writeSingleChordMidiFile(chord);
    _inFlightChordDrags[midiFile.getFullPathName()] = degree;

    if (auto* dragContainer = findParentComponentOfClass<juce::DragAndDropContainer>())
        dragContainer->performExternalDragDropOfFiles({ midiFile.getFullPathName() }, false);
}

void AppLayout::onChordPreviewRequested(theory::Degree degree, const theory::Chord& chord)
{
    juce::ignoreUnused(degree);

    previewChord(chord);
}

void AppLayout::onChordPreviewRequested(const theory::Chord& chord)
{
    previewChord(chord);
}

void AppLayout::previewChord(const theory::Chord& chord)
{
    _synthEngine.previewChord(theory::NoteConvertor::voiceChordCloseToMiddleC(chord));
}

void AppLayout::onVoicingSelectorRequested(theory::Degree degree, const std::vector<theory::Chord>& availableVoicings, const std::string& currentSymbol)
{
    _openVoicingDegree = degree;

    _voicingSelector.show(availableVoicings, currentSymbol,
        [this, degree](const theory::Chord& chosen)
        {
            _chordBrowser.selectVoicing(degree, chosen);
            previewChord(chosen);
        });

    updateVoicingSelectorArrow();
}

void AppLayout::updateVoicingSelectorArrow()
{
    if (!_openVoicingDegree || !_voicingSelector.isVisible())
        return;

    if (auto* card = _chordBrowser.getCard(*_openVoicingDegree))
        _voicingSelector.setArrowTargetX(_voicingSelector.getLocalPoint(card, card->getLocalBounds().getCentre()).x);
}

void AppLayout::onSlotFileDropped(int slotIndex, const juce::String& filePath)
{
    const auto it = _inFlightChordDrags.find(filePath);
    if (it == _inFlightChordDrags.end())
        return;

    _progressionSequencer.setSlotDegree(slotIndex, it->second);
    _inFlightChordDrags.erase(it);
}

void AppLayout::onProgressionDragStarted()
{
    const auto populatedSlots = _progressionSequencer.getPopulatedSlots();
    if (populatedSlots.empty())
        return;

    std::vector<theory::ResolvedProgressionSlot> resolvedSlots;
    resolvedSlots.reserve(populatedSlots.size());

    for (const auto& slot : populatedSlots)
    {
        if (const auto* chord = _chordBrowser.resolveSlot(slot))
            resolvedSlots.push_back(theory::ResolvedProgressionSlot { slot, *chord });
    }

    if (resolvedSlots.empty())
        return;

    const auto midiFile = theory::MidiExporter::writeProgressionMidiFile(resolvedSlots);

    if (auto* dragContainer = findParentComponentOfClass<juce::DragAndDropContainer>())
        dragContainer->performExternalDragDropOfFiles({ midiFile.getFullPathName() }, false);
}

void AppLayout::onSlotsChanged()
{
    syncStateToValueTree();
}

void AppLayout::syncStateToValueTree()
{
    const auto key = _keyScaleSelector.getKey();
    const auto scale = _keyScaleSelector.getScale();

    theory::SessionState state;
    state.key = key;
    state.scale = scale;

    for (const auto& degreeData : theory::ChordDatabase::getInstance().get(key, scale).degrees)
    {
        if (const auto* chord = _chordBrowser.getCurrentChord(degreeData.degree))
            state.degreeVoicings.emplace_back(degreeData.degree, chord->symbol);
    }

    for (int i = 0; i < _progressionSequencer.getSlotCount(); ++i)
    {
        if (const auto degree = _progressionSequencer.getSlotDegree(i))
            state.progressionSlots.emplace_back(i, *degree);
    }

    auto rootState = _parameterManager.getState().state;
    rootState.removeChild(rootState.getChildWithName(theory::SessionStateSerializer::kStateTag), nullptr);
    rootState.appendChild(theory::SessionStateSerializer::toValueTree(state), nullptr);
}

void AppLayout::restoreStateFromValueTree()
{
    const auto stateTree = _parameterManager.getState().state.getChildWithName(theory::SessionStateSerializer::kStateTag);
    if (!stateTree.isValid())
        return;

    const auto state = theory::SessionStateSerializer::fromValueTree(stateTree);

    _keyScaleSelector.setKeyAndScale(state.key, state.scale);
    _chordBrowser.setKeyAndScale(state.key, state.scale);
    _progressionSequencer.setScale(state.scale);

    for (const auto& [degree, chordSymbol] : state.degreeVoicings)
        _chordBrowser.setDegreeVoicing(degree, chordSymbol);

    for (const auto& [index, degree] : state.progressionSlots)
        _progressionSequencer.setSlotDegree(index, degree);
}
