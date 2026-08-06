#include "Component/SynthLfoSection.h"

#include "AppLocalisation.h"
#include "Component/SynthLayoutConstants.h"
#include "Parameters.h"

namespace component
{

SynthLfoSection::SynthLfoSection(const std::string& identifier, ndsp::ParameterManager& parameterManager):
    SynthSection(identifier, parameterManager),
    _shapeCycler(parameterManager, Parameters::LFO_SHAPE_ID),
    _lfoCurve(parameterManager, identifier + "-curve", Parameters::LFO_SHAPE_ID, Parameters::LFO_SMOOTH_PERCENT_ID),
    _modeToggle(parameterManager, Parameters::LFO_MODE_ID, nui::Icons::getBoxes()), // placeholder icon, to be replaced
    _syncToggle(parameterManager, Parameters::LFO_SYNC_ENABLED_ID, nui::Icons::getNote()),
    _syncDivisionDial(parameterManager, Parameters::LFO_SYNC_DIVISION_ID),
    _rateDial(parameterManager, Parameters::LFO_RATE_HZ_ID, "Hz"),
    _smoothDial(parameterManager, Parameters::LFO_SMOOTH_PERCENT_ID),
    _delayDial(parameterManager, Parameters::LFO_DELAY_MS_ID),
    _stereoDial(parameterManager, Parameters::LFO_STEREO_PERCENT_ID),
    _depthDial(parameterManager, Parameters::LFO_DEPTH_PERCENT_ID)
{
    addAndMakeVisible(_shapeCycler);
    addAndMakeVisible(_lfoCurve);
    addAndMakeVisible(_modeToggle);
    addAndMakeVisible(_syncToggle);
    addAndMakeVisible(_syncDivisionDial);
    addAndMakeVisible(_rateDial);
    addAndMakeVisible(_smoothDial);
    addAndMakeVisible(_delayDial);
    addAndMakeVisible(_stereoDial);
    addAndMakeVisible(_depthDial);

    _syncDivisionDial.setHeightType(nui::Theme::DialHeightType::MEDIUM);
    _rateDial.setHeightType(nui::Theme::DialHeightType::MEDIUM);
    _smoothDial.setHeightType(nui::Theme::DialHeightType::MEDIUM);
    _delayDial.setHeightType(nui::Theme::DialHeightType::MEDIUM);
    _stereoDial.setHeightType(nui::Theme::DialHeightType::MEDIUM);
    _depthDial.setHeightType(nui::Theme::DialHeightType::MEDIUM);

    _syncDivisionDial.setLabelGap(8.f);
    _rateDial.setLabelGap(8.f);
    _smoothDial.setLabelGap(8.f);
    _delayDial.setLabelGap(8.f);
    _stereoDial.setLabelGap(8.f);
    _depthDial.setLabelGap(8.f);

    _lfoCurve.displayBackground(nui::Theme::ThemeColor::BACKGROUND, nui::Theme::getBorderRadius());
    _shapeCycler.setBackgroundColour(nui::Theme::newColor(nui::Theme::ThemeColor::BACKGROUND).asJuce());

    _syncToggle.addOnValueChangedListener(this);
    _syncToggle.setIconSize(14.f);
    _modeToggle.setIconSize(14.f);

    setGap(8.f);
    setLayoutMargin(kSynthSectionPadding);
    getLayout().setDisplayGrid(false);
    // Row 2/3 together replace the old single 90px knob row - split in two so the mode/sync
    // toggles can stack in one narrow column (col 0) while every dial spans both sub-rows (height
    // 2) to stay the same full 90px tall it always was. Frees up the column the mode dial used to
    // occupy for the rest of the row.
    getLayout().init({ 1, 3, 1, 1 }, { 1, 2, 2, 2, 2, 2 });
    getLayout().setFixedRowHeight(0, 28.f);
    getLayout().setFixedRowHeight(2, 37.5f);
    getLayout().setFixedRowHeight(3, 37.5f);

    getLayout().addComponent(_shapeCycler, 0, 0, 6, 1);
    getLayout().addComponent(_lfoCurve, 1, 0, 6, 1);
    getLayout().addComponent(_modeToggle, 2, 0, 1, 1);
    getLayout().addComponent(_syncToggle, 3, 0, 1, 1);
    // Both occupy the same cell, keyed by their own distinct component IDs - GridLayout stores
    // items by ID, not by (row, column), so this is safe and gives the visibility-toggle its
    // "stacked, swap which one shows" behavior for free.
    getLayout().addComponent(_syncDivisionDial, 2, 1, 1, 2);
    getLayout().addComponent(_rateDial, 2, 1, 1, 2);
    getLayout().addComponent(_smoothDial, 2, 2, 1, 2);
    getLayout().addComponent(_delayDial, 2, 3, 1, 2);
    getLayout().addComponent(_stereoDial, 2, 4, 1, 2);
    getLayout().addComponent(_depthDial, 2, 5, 1, 2);

    // Must come after addComponent(), which calls addAndMakeVisible() internally - setting this
    // any earlier gets silently overridden back to visible. Initial visibility must match the
    // parameter's actual current value (not just its default), so a saved session that had sync
    // enabled shows the correct control on first paint.
    const auto isSyncEnabled = parameterManager.getState().getRawParameterValue(Parameters::LFO_SYNC_ENABLED_ID)->load() > 0.5f;
    _syncDivisionDial.setVisible(isSyncEnabled);
    _rateDial.setVisible(!isSyncEnabled);

    initSection();
}

SynthLfoSection::~SynthLfoSection()
{
    _syncToggle.removeListener(this);
}

std::string SynthLfoSection::getSectionName()
{
    return juce::translate("synth_lfo_section_title").toStdString();
}

void SynthLfoSection::onToggleValueChanged(const std::string& componentID, bool isOn)
{
    Section::onToggleValueChanged(componentID, isOn);

    if (componentID != _syncToggle.getID())
        return;

    _syncDivisionDial.setVisible(isOn);
    _rateDial.setVisible(!isOn);
}

void SynthLfoSection::refreshLabels()
{
    _syncDivisionDial.setDisplayLabel(juce::translate("synth_lfo_sync_division_label"));
    _rateDial.setDisplayLabel(juce::translate("synth_lfo_rate_label"));
    _smoothDial.setDisplayLabel(juce::translate("synth_lfo_smooth_label"));
    _delayDial.setDisplayLabel(juce::translate("synth_lfo_delay_label"));
    _stereoDial.setDisplayLabel(juce::translate("synth_lfo_stereo_label"));
    _depthDial.setDisplayLabel(juce::translate("synth_lfo_depth_label"));
}

}
