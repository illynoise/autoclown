// Per-user settings shared by all instances (e.g. default window size for new instances).
#pragma once

#include <juce_data_structures/juce_data_structures.h>

class GlobalSettings
{
public:
    GlobalSettings();
    float getDefaultScale() const;
    void setDefaultScale (float scale);

private:
    std::unique_ptr<juce::PropertiesFile> props;
};
