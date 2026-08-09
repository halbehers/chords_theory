#include "AppLayout.h"

#include "AppLocalisation.h"
#include "Theory/ChordDatabase.h"
#include "Theory/MidiExporter.h"
#include "Theory/NoteConvertor.h"
#include "Theory/SessionStateSerializer.h"

AppLayout::AppLayout(ndsp::ParameterManager& parameterManager, PluginAudioProcessor& audioProcessor):
    nlayout::AppLayout(parameterManager),
    _audioProcessor(audioProcessor),
    _settings("settings", nui::Icons::getGear()),
    _keyScaleSelector("key-scale-selector"),
    _chordBrowser("chord-degree-browser"),
    _progressionEditor("progression-sequencer", [this](const theory::ProgressionSlot& slot) { return _chordBrowser.resolveSlot(slot); }, &audioProcessor.getSynthEngine().getProgressionPlayer()),
    _synthEditor(parameterManager, &audioProcessor.getSynthEngine().getLeftWaveformFifo(), &audioProcessor.getSynthEngine().getRightWaveformFifo()),
    _mainSection("main-section", parameterManager),
    _windowsManager(*this)
{
    _settings.setIconSize(24.f);
    _settings.addOnClickListener(this);
    _windowsManager.createWindow(std::make_unique<component::SettingsWindow>("settings", _windowsManager));

    _keyScaleSelector.addListener(this);

    _chordBrowser.addListener(this);
    _chordBrowser.setKeyAndScale(_keyScaleSelector.getKey(), _keyScaleSelector.getScale());

    _progressionEditor.addListener(this);
    _progressionEditor.setKeyAndScale(_keyScaleSelector.getKey(), _keyScaleSelector.getScale());

    _voicingSelector.addListener(this);
    _voicingSelector.setDismissExemptComponent(&_chordBrowser);

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
    _mainSection.getLayout().init({ 1, 1, 1, 1, 1 }, { 1, 1, 1, 1, 1, 1, 1, 1, 1 });

    _mainSection.getLayout().setFixedColumnWidth(0, 24.f);
    _mainSection.getLayout().setFixedColumnWidth(8, 24.f);
    _mainSection.getLayout().setFixedColumnWidth(1, 32.f);
    _mainSection.getLayout().setFixedColumnWidth(7, 32.f);
    _mainSection.getLayout().setFixedColumnWidth(4, 450.f);
    _mainSection.getLayout().setFixedRowHeight(0, 60.f);
    _mainSection.getLayout().setFixedRowHeight(1, 64.f);
    _mainSection.getLayout().setFixedRowHeight(2, 70.f);
    _mainSection.getLayout().setFixedRowHeight(3, 12.f);

    _mainSection.getLayout().addComponent(_settings, 0, 1, 1, 1);
    _mainSection.getLayout().addComponent(_keyScaleSelector, 0, 4, 1, 1);
    _mainSection.getLayout().addComponent(_chordBrowser, 1, 3, 3, 1);
    _mainSection.getLayout().addComponent(_voicingSelector, 2, 0, 9, 1);
    _mainSection.getLayout().addComponent(_progressionEditor, 4, 1, 7, 1);

    _mainSection.getLayout("synth-tab").setDisplayGrid(false);
    _mainSection.getLayout("synth-tab").init({ 1 }, { 1 });
    _mainSection.getLayout("synth-tab").addComponent(_synthEditor, 0, 0, 1, 1);

    getLayout().setGap(16.f);
    getLayout().setDisplayGrid(false);
    getLayout().setResizableLineConfiguration({ .displayLine = false });

    if (audioProcessor.isAudioUnit() || audioProcessor.isVst3())
    {
        getLayout().setMargin(0.f, 0.f, 24.f, 0.f);
    }
#if JUCE_MAC
    else
    {
        getLayout().setMargin(0.f, 24.f + 16.f, 0.f, 0.f);
    }
#else
    else
    {
        getLayout().setMargin(0.f, 0.f, 0.f, 0.f);
    }
#endif

    getLayout().init({ 1 }, { 1 }); // row0: settings/key-scale (fixed height), row1: _mainSection (flexible)

    getLayout().addComponent(_mainSection, 0, 0, 1, 1);

    setVoicingVisibility(false);

    restoreStateFromValueTree();
}

AppLayout::~AppLayout()
{
    AppLocalisation::getChangeBroadcaster().removeChangeListener(this);
    _mainSection.removeListener(this);

    _settings.removeListener(this);
    _keyScaleSelector.removeListener(this);
    _chordBrowser.removeListener(this);
    _progressionEditor.removeListener(this);
    _voicingSelector.removeListener(this);
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

    const bool isVisible = _openVoicingDegree.has_value();
    setVoicingVisibility(isVisible);
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
    // degree that doesn't exist in the new scale (e.g. Minor Blues only has I/IV/V). close()'s own
    // onVoicingSelectorClosed() notification (see below) handles resetting visibility/row height/
    // _openVoicingDegree uniformly, the same as if the user had closed it via its own close button.
    _voicingSelector.close();

    _chordBrowser.setKeyAndScale(key, scale);
    _progressionEditor.setKeyAndScale(key, scale);

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

void AppLayout::previewChord(const theory::Chord& chord)
{
    _audioProcessor.getSynthEngine().previewChord(theory::NoteConvertor::voiceChordCloseToMiddleC(chord));
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
    setVoicingVisibility(true);
}

void AppLayout::onVoicingSelectorClosed()
{
    setVoicingVisibility(false);
    _openVoicingDegree.reset();
}

void AppLayout::setVoicingVisibility(bool isVisible)
{
    _voicingSelector.setVisible(isVisible);
    _mainSection.getLayout().setFixedRowHeight(2, isVisible ? 80.f : 0.f);

    // setFixedRowHeight only updates the fixed-height map - it takes effect on the next time
    // _mainSection's own GridLayout::resized() actually runs. That normally only happens as a
    // side effect of _mainSection's outer bounds changing (JUCE's Component::setBounds() skips
    // the resized() callback when the bounds are unchanged), which never happens here since
    // _mainSection's own size on screen never changes - only one of its internal rows does. Force
    // it directly rather than relying on the outer resized() cascade below to trigger it.
    _mainSection.resized();
    resized();
}

void AppLayout::updateVoicingSelectorArrow()
{
    if (!_openVoicingDegree || !_voicingSelector.isVisible())
        return;

    if (auto* card = _chordBrowser.getCard(*_openVoicingDegree))
        _voicingSelector.setArrowTargetX(_voicingSelector.getLocalPoint(card, card->getLocalBounds().getCentre()).x);
}

void AppLayout::onChordFileDropped(double startBeat, const juce::String& filePath)
{
    const auto it = _inFlightChordDrags.find(filePath);
    if (it == _inFlightChordDrags.end())
        return;

    if (const auto* chord = _chordBrowser.resolveSlot(theory::ProgressionSlot { it->second, 0 }))
        _progressionEditor.addChordAtBeat(startBeat, *chord);

    _inFlightChordDrags.erase(it);
}

void AppLayout::onProgressionDragStarted()
{
    const auto state = _progressionEditor.getMidiEditorState();
    if (state.notes.empty())
        return;

    const auto midiFile = theory::MidiExporter::writeMidiEditorContentFile(state.notes);

    if (auto* dragContainer = findParentComponentOfClass<juce::DragAndDropContainer>())
        dragContainer->performExternalDragDropOfFiles({ midiFile.getFullPathName() }, false);
}

void AppLayout::onContentChanged()
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

    state.progressionEditorState = _progressionEditor.getMidiEditorState();

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
    _progressionEditor.setKeyAndScale(state.key, state.scale);

    for (const auto& [degree, chordSymbol] : state.degreeVoicings)
        _chordBrowser.setDegreeVoicing(degree, chordSymbol);

    _progressionEditor.restoreMidiEditorState(state.progressionEditorState);
}
