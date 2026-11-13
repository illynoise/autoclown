#include "GlobalSettings.h"

using namespace juce;

GlobalSettings::GlobalSettings()
{
    PropertiesFile::Options o;
    o.applicationName = "Auto Clown";
    o.filenameSuffix = ".settings";
    o.folderName = "Auto Clown";
    o.osxLibrarySubFolder = "Application Support";
    o.storageFormat = PropertiesFile::storeAsXML;
    o.millisecondsBeforeSaving = 500;
    props = std::make_unique<PropertiesFile> (o);
}

float GlobalSettings::getDefaultScale() const
{
    return jlimit (0.5f, 2.0f, (float) props->getDoubleValue ("defaultScale", 1.0));
}

void GlobalSettings::setDefaultScale (float s)
{
    props->setValue ("defaultScale", jlimit (0.5, 2.0, (double) s));
    props->saveIfNeeded();
}
