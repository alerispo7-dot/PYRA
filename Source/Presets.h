#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

class PyraAudioProcessor;

namespace pyra
{
/** One entry of the preset library (factory or user). */
struct PresetInfo
{
    juce::String id;          // stable: never changes, even if the preset is renamed
    juce::String name, category, subCategory, author;
    juce::StringArray tags;   // tags written in the preset itself
    bool factory = true;
    int factoryIndex = -1;    // position in the built-in bank
    juce::File file;          // user presets only
};

/** Reads/writes the sound of the plugin as a small JSON object:
    { "params": { "paramID": value, ... }, "arp": [[degree, vel, gate, on, tie], ...] }
    Only values that differ from the default are stored. */
namespace presetdata
{
    juce::var capture (PyraAudioProcessor&);
    void apply (PyraAudioProcessor&, const juce::var& data);
}

class PresetManager : public juce::ChangeBroadcaster
{
public:
    explicit PresetManager (PyraAudioProcessor&);

    void rescan();
    const juce::Array<PresetInfo>& getAll() const { return presets; }
    const PresetInfo* find (const juce::String& id) const;

    bool load (const juce::String& id);
    void loadInit();

    /** Saves the current sound as a user preset. If `overwriteId` is a user preset it is replaced. Returns the id. */
    juce::String saveUser (const juce::String& name, const juce::String& category, const juce::StringArray& tags,
                           const juce::String& overwriteId = {});
    bool renameUser (const juce::String& id, const juce::String& newName);
    bool deleteUser (const juce::String& id);

    // favourites and personal tags live in UserData.json, keyed by preset id
    bool isFavourite (const juce::String& id) const { return favourites.contains (id); }
    void setFavourite (const juce::String& id, bool fav);
    juce::StringArray getPersonalTags (const juce::String& id) const;
    void setPersonalTags (const juce::String& id, const juce::StringArray& tags);
    juce::StringArray getAllPersonalTags() const;

    juce::String getCurrentId() const;
    juce::String getCurrentName() const;
    bool isModified() const;

    static juce::File getUserFolder();
    static juce::StringArray getCategories();

private:
    void loadFactory();
    void scanUser();
    void loadUserData();
    void saveUserData();
    juce::var factoryData (int index) const;
    void markLoaded (const juce::String& id, const juce::String& name);

    PyraAudioProcessor& proc;
    juce::Array<PresetInfo> presets;
    juce::var factoryBank;   // array of preset objects
    juce::StringArray favourites;
    std::map<juce::String, juce::StringArray> personalTags;
    std::vector<float> loadedSnapshot;
};

} // namespace pyra
