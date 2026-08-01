#include "Theory/ProgressionPresetLibrary.h"

#include <algorithm>

#include "AppSettings.h"
#include "Theory/BuiltInProgressionPresets.h"

namespace theory
{

namespace
{
    constexpr const char* kUserPresetsPropertyKey = "userProgressionPresets";
    constexpr const char* kRootTag = "UserProgressionPresets";
    constexpr const char* kPresetTag = "Preset";
    constexpr const char* kSlotTag = "Slot";

    juce::File getDefaultUserPresetsFile()
    {
        return AppSettings::getAppSupportDirectory().getChildFile("user-presets.xml");
    }

    juce::PropertiesFile::Options makeOptions(juce::InterProcessLock& processLock)
    {
        juce::PropertiesFile::Options options;
        options.storageFormat = juce::PropertiesFile::storeAsXML;
        options.processLock = &processLock;
        return options;
    }

    std::unique_ptr<juce::XmlElement> presetsToXml(const std::vector<ProgressionPreset>& presets)
    {
        auto root = std::make_unique<juce::XmlElement>(kRootTag);

        for (const auto& preset : presets)
        {
            auto* presetElement = root->createNewChildElement(kPresetTag);
            presetElement->setAttribute("id", juce::String(preset.id));
            presetElement->setAttribute("displayName", juce::String(preset.displayName));

            // Only ever empty or {MinorBlues} for user presets (see
            // ProgressionPresetFactory::createFromSlots), but stored generically as a
            // space-separated list of scale names in case that ever changes.
            juce::StringArray applicableScaleNames;
            for (const auto scale : preset.applicableScales)
                applicableScaleNames.add(getScaleJsonKey(scale));
            presetElement->setAttribute("applicableScales", applicableScaleNames.joinIntoString("|"));

            for (const auto& slot : preset.slots)
            {
                auto* slotElement = presetElement->createNewChildElement(kSlotTag);
                slotElement->setAttribute("degree", juce::String(getDegreeLabel(slot.degree)));
                slotElement->setAttribute("popularityOrder", slot.popularityOrder);
            }
        }

        return root;
    }

    std::vector<ProgressionPreset> presetsFromXml(const juce::XmlElement* root)
    {
        std::vector<ProgressionPreset> presets;

        if (root == nullptr)
            return presets;

        for (auto* presetElement : root->getChildWithTagNameIterator(kPresetTag))
        {
            ProgressionPreset preset;
            preset.id = presetElement->getStringAttribute("id").toStdString();
            preset.displayName = presetElement->getStringAttribute("displayName").toStdString();

            const auto applicableScaleNames = juce::StringArray::fromTokens(presetElement->getStringAttribute("applicableScales"), "|", "");
            for (const auto& scaleName : applicableScaleNames)
            {
                if (scaleName.isEmpty())
                    continue;

                try
                {
                    preset.applicableScales.push_back(parseScale(scaleName.toStdString()));
                }
                catch (const std::invalid_argument&)
                {
                    // Skips an unrecognized scale name rather than dropping the whole preset.
                }
            }

            for (auto* slotElement : presetElement->getChildWithTagNameIterator(kSlotTag))
            {
                try
                {
                    preset.slots.push_back(ProgressionSlot {
                        parseDegree(slotElement->getStringAttribute("degree").toStdString()),
                        slotElement->getIntAttribute("popularityOrder", 0) });
                }
                catch (const std::invalid_argument&)
                {
                    // Skips a malformed slot rather than dropping the whole preset - defensive
                    // against hand-edited or future-version XML.
                }
            }

            if (!preset.id.empty())
                presets.push_back(std::move(preset));
        }

        return presets;
    }
}

ProgressionPresetLibrary::ProgressionPresetLibrary(const juce::File& userPresetsFile):
    _propertiesFile(userPresetsFile, makeOptions(_processLock))
{
    loadUserPresets();
}

ProgressionPresetLibrary& ProgressionPresetLibrary::getInstance()
{
    static ProgressionPresetLibrary& instance = *new ProgressionPresetLibrary(getDefaultUserPresetsFile());
    return instance;
}

std::vector<ProgressionPreset> ProgressionPresetLibrary::getAllPresets() const
{
    auto presets = getBuiltInProgressionPresets();
    presets.insert(presets.end(), _userPresets.begin(), _userPresets.end());
    return presets;
}

void ProgressionPresetLibrary::savePreset(const ProgressionPreset& preset)
{
    if (!preset.isUserPreset())
        return;

    const auto it = std::find_if(_userPresets.begin(), _userPresets.end(),
        [&preset](const ProgressionPreset& existing) { return existing.id == preset.id; });

    if (it != _userPresets.end())
        *it = preset;
    else
        _userPresets.push_back(preset);

    persistUserPresets();
}

void ProgressionPresetLibrary::deletePreset(const std::string& id)
{
    const auto sizeBefore = _userPresets.size();

    _userPresets.erase(std::remove_if(_userPresets.begin(), _userPresets.end(),
        [&id](const ProgressionPreset& preset) { return preset.id == id; }), _userPresets.end());

    if (_userPresets.size() != sizeBefore)
        persistUserPresets();
}

void ProgressionPresetLibrary::loadUserPresets()
{
    _userPresets = presetsFromXml(_propertiesFile.getXmlValue(kUserPresetsPropertyKey).get());
}

void ProgressionPresetLibrary::persistUserPresets()
{
    _propertiesFile.setValue(kUserPresetsPropertyKey, presetsToXml(_userPresets).get());
    _propertiesFile.save();
}

}
