#include "Presets.h"
#include "PluginProcessor.h"
#include "BinaryData.h"
#include <map>

namespace pyra
{
//==============================================================================
bool isLayerParam (const juce::String& id) { return id.startsWith ("layer"); }

namespace presetdata
{
    juce::var capture (PyraAudioProcessor& p)
    {
        auto* params = new juce::DynamicObject();
        for (auto* ap : p.getParameters())
            if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (ap))
            {
                if (isLayerParam (rp->getParameterID())) continue;   // the LAYERS mix belongs to the instance, not to a sound
                const float norm = rp->getValue();
                if (std::abs (norm - rp->getDefaultValue()) > 1.0e-5f)
                    params->setProperty (rp->getParameterID(), rp->convertFrom0to1 (norm));
            }

        auto* obj = new juce::DynamicObject();
        obj->setProperty ("params", juce::var (params));

        bool patternUsed = false;
        juce::Array<juce::var> steps;
        for (auto& s : p.arpPattern.steps)
        {
            juce::Array<juce::var> st { s.degree.load(), s.velocity.load(), s.gate.load(), s.on.load() ? 1 : 0, s.tie.load() ? 1 : 0 };
            if (s.degree.load() != 0 || std::abs (s.velocity.load() - 0.8f) > 0.001f || std::abs (s.gate.load() - 1.0f) > 0.001f
                || ! s.on.load() || s.tie.load())
                patternUsed = true;
            steps.add (st);
        }
        if (patternUsed) obj->setProperty ("arp", steps);
        if (! p.chordPattern.isDefault()) obj->setProperty ("chords", p.chordPattern.toVar());
        if (! p.glitchPattern.isEmpty()) obj->setProperty ("glitch", p.glitchPattern.toVar());
        if (! p.lfoDrawn.isDefault()) obj->setProperty ("lfoDraw", p.lfoDrawn.toVar());
        return juce::var (obj);
    }

    void apply (PyraAudioProcessor& p, const juce::var& data)
    {
        const auto* params = data["params"].getDynamicObject();
        for (auto* ap : p.getParameters())
            if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (ap))
            {
                if (isLayerParam (rp->getParameterID())) continue;
                float norm = rp->getDefaultValue();
                if (params != nullptr && params->hasProperty (rp->getParameterID()))
                    norm = rp->convertTo0to1 ((float) params->getProperty (rp->getParameterID()));
                if (std::abs (rp->getValue() - norm) > 1.0e-6f)
                    rp->setValueNotifyingHost (norm);
            }

        auto& pat = p.arpPattern;
        if (auto* arr = data["arp"].getArray())
        {
            for (int k = 0; k < kMaxArpSteps; ++k)
            {
                auto& s = pat.steps[(size_t) k];
                const auto st = k < arr->size() ? arr->getReference (k) : juce::var();
                s.degree = st.isArray() ? (int) st[0] : 0;
                s.velocity = st.isArray() ? (float) st[1] : 0.8f;
                s.gate = st.isArray() ? (float) st[2] : 1.0f;
                s.on = st.isArray() ? (int) st[3] != 0 : true;
                s.tie = st.isArray() ? (int) st[4] != 0 : false;
                s.lock = false;
            }
            ++pat.changeCounter;
        }
        else
            pat.clear();

        if (data.hasProperty ("chords")) p.chordPattern.fromVar (data["chords"]); else p.chordPattern.clear();
        p.glitchPattern.fromVar (data["glitch"]);
        p.lfoDrawn.fromVar (data["lfoDraw"]);
    }
}

namespace
{
    int sampleOf (const juce::var& preset)
    {
        const auto params = preset["params"];
        if ((float) params.getProperty ("smp_on", 0.0f) < 0.5f) return 0;
        return juce::jmax (0, (int) std::lround ((float) params.getProperty ("smp_inst", 0.0f)));
    }
}

void PresetManager::restoreCurrent (const juce::String& id, const juce::String& name, const std::vector<float>& snapshot)
{
    proc.presetId = id;
    proc.presetName = name;
    loadedSnapshot = snapshot;
    sendChangeMessage();
}

//==============================================================================
PresetManager::PresetManager (PyraAudioProcessor& p) : proc (p)
{
    loadFactory();
    loadUserData();
    scanUser();
}

juce::StringArray PresetManager::getCategories()
{
    return { "Bass", "Synth", "Pad", "Keyboard", "Organs", "Bells", "Guitar", "Strings", "Brass", "Winds", "Vox",
             "Arps", "Drums", "FX" };
}

juce::File PresetManager::getUserFolder()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("PYRA");
}

void PresetManager::loadFactory()
{
    presets.clear();
    int size = 0;
    if (auto* raw = BinaryData::getNamedResource ("FactoryPresets_json_gz", size))
    {
        // parsed once and shared by every PYRA sound alive (the layers each have a preset manager)
        if (sharedBank->bank.isVoid())
        {
            juce::MemoryInputStream mis (raw, (size_t) size, false);
            juce::GZIPDecompressorInputStream gz (&mis, false, juce::GZIPDecompressorInputStream::gzipFormat);
            sharedBank->bank = juce::JSON::parse (gz.readEntireStreamAsString());
        }
        factoryBank = sharedBank->bank;
    }

    if (auto* arr = factoryBank.getArray())
        for (int i = 0; i < arr->size(); ++i)
        {
            const auto& v = arr->getReference (i);
            PresetInfo info;
            info.id = v["id"].toString();
            info.name = v["name"].toString();
            info.category = v["category"].toString();
            info.subCategory = v["sub"].toString();
            info.author = "PYRA";
            if (auto* t = v["tags"].getArray()) for (auto& tag : *t) info.tags.add (tag.toString());
            info.factory = true;
            info.factoryIndex = i;
            info.sampleInstrument = sampleOf (v);
            presets.add (info);
        }
}

juce::var PresetManager::factoryData (int index) const
{
    if (auto* arr = factoryBank.getArray())
        if (juce::isPositiveAndBelow (index, arr->size()))
            return arr->getReference (index);
    return {};
}

void PresetManager::scanUser()
{
    presets.removeIf ([] (const PresetInfo& p) { return ! p.factory; });
    auto folder = getUserFolder().getChildFile ("Presets");
    for (const auto& entry : juce::RangedDirectoryIterator (folder, true, "*.pyrapreset"))
    {
        const auto v = juce::JSON::parse (entry.getFile());
        if (! v.isObject()) continue;
        PresetInfo info;
        info.id = v["id"].toString();
        if (info.id.isEmpty()) info.id = "user:" + entry.getFile().getFileNameWithoutExtension();
        info.name = v["name"].toString().isNotEmpty() ? v["name"].toString() : entry.getFile().getFileNameWithoutExtension();
        info.category = v["category"].toString().isNotEmpty() ? v["category"].toString() : "User";
        // presets saved before 0.9 used the old categories
        static const std::map<juce::String, juce::String> moved { { "Lead", "Synth" }, { "Keys", "Keyboard" }, { "Pluck", "Synth" },
            { "Brass & Winds", "Brass" }, { "Arp", "Arps" }, { "Sequence", "Arps" }, { "Chords", "Synth" }, { "Wind", "Winds" } };
        if (auto it = moved.find (info.category); it != moved.end()) info.category = it->second;
        info.subCategory = v["sub"].toString();
        info.author = v["author"].toString();
        if (auto* t = v["tags"].getArray()) for (auto& tag : *t) info.tags.add (tag.toString());
        info.factory = false;
        info.file = entry.getFile();
        info.sampleInstrument = sampleOf (v);
        presets.add (info);
    }
}

void PresetManager::rescan()
{
    scanUser();
    sendChangeMessage();
}

const PresetInfo* PresetManager::find (const juce::String& id) const
{
    for (auto& p : presets)
        if (p.id == id) return &p;
    return nullptr;
}

void PresetManager::markLoaded (const juce::String& id, const juce::String& name)
{
    proc.presetId = id;
    proc.presetName = name;
    loadedSnapshot.clear();
    for (auto* ap : proc.getParameters()) loadedSnapshot.push_back (ap->getValue());
    sendChangeMessage();
}

bool PresetManager::load (const juce::String& id)
{
    const auto* info = find (id);
    if (info == nullptr) return false;

    juce::var data;
    if (info->factory) data = factoryData (info->factoryIndex);
    else data = juce::JSON::parse (info->file);
    if (! data.isObject()) return false;

    presetdata::apply (proc, data);
    markLoaded (info->id, info->name);
    addRecent (info->id);
    return true;
}

void PresetManager::addRecent (const juce::String& id)
{
    if (id.isEmpty()) return;
    // other PYRAs (other tracks, other projects) add to the same list: start from what is on disk
    const auto v = juce::JSON::parse (getUserFolder().getChildFile ("UserData.json"));
    if (auto* r = v["recent"].getArray())
    {
        recent.clear();
        for (auto& x : *r) recent.add (x.toString());
    }
    recent.removeString (id);
    recent.insert (0, id);
    while (recent.size() > maxRecent) recent.remove (recent.size() - 1);
    saveUserData();
}

void PresetManager::clearRecent()
{
    recent.clear();
    saveUserData();
    sendChangeMessage();
}

void PresetManager::loadInit()
{
    presetdata::apply (proc, juce::var (new juce::DynamicObject()));
    markLoaded ({}, "Init");
}

juce::String PresetManager::getCurrentId() const { return proc.presetId; }

juce::String PresetManager::getCurrentName() const
{
    // show the current name even if the preset was renamed after this project was saved
    if (auto* p = find (proc.presetId)) return p->name;
    return proc.presetName.isNotEmpty() ? proc.presetName : "Init";
}

bool PresetManager::isModified() const
{
    const auto& params = proc.getParameters();
    if (loadedSnapshot.size() != (size_t) params.size()) return false;
    for (int i = 0; i < params.size(); ++i)
        if (std::abs (params[i]->getValue() - loadedSnapshot[(size_t) i]) > 1.0e-4f)
            if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (params[i]); rp == nullptr || ! isLayerParam (rp->getParameterID()))
                return true;
    return false;
}

juce::String PresetManager::saveUser (const juce::String& name, const juce::String& category, const juce::StringArray& tags,
                                      const juce::String& overwriteId)
{
    const auto cleanName = name.trim().isEmpty() ? juce::String ("My Sound") : name.trim();
    auto folder = getUserFolder().getChildFile ("Presets");
    folder.createDirectory();

    juce::File file;
    juce::String id;
    if (auto* existing = find (overwriteId); existing != nullptr && ! existing->factory)
    {
        file = existing->file;
        id = existing->id;
    }
    else
    {
        id = "user:" + juce::Uuid().toString();
        const auto safe = juce::File::createLegalFileName (cleanName);
        file = folder.getNonexistentChildFile (safe.isNotEmpty() ? safe : "Preset", ".pyrapreset", false);
    }

    auto data = presetdata::capture (proc);
    auto* obj = data.getDynamicObject();
    obj->setProperty ("id", id);
    obj->setProperty ("name", cleanName);
    obj->setProperty ("category", category.isNotEmpty() ? category : "User");
    obj->setProperty ("author", juce::SystemStats::getFullUserName());
    juce::Array<juce::var> t;
    for (auto& s : tags) t.add (s);
    obj->setProperty ("tags", t);
    obj->setProperty ("format", 1);

    if (! file.replaceWithText (juce::JSON::toString (data, false)))
        return {};

    scanUser();
    markLoaded (id, cleanName);
    return id;
}

bool PresetManager::renameUser (const juce::String& id, const juce::String& newName)
{
    const auto* info = find (id);
    if (info == nullptr || info->factory || newName.trim().isEmpty()) return false;
    auto data = juce::JSON::parse (info->file);
    if (! data.isObject()) return false;
    data.getDynamicObject()->setProperty ("name", newName.trim());   // the id stays the same
    const auto file = info->file;
    if (! file.replaceWithText (juce::JSON::toString (data, false))) return false;
    const auto target = file.getSiblingFile (juce::File::createLegalFileName (newName.trim()) + ".pyrapreset");
    if (! target.exists()) file.moveFileTo (target);
    if (proc.presetId == id) proc.presetName = newName.trim();
    rescan();
    return true;
}

bool PresetManager::deleteUser (const juce::String& id)
{
    const auto* info = find (id);
    if (info == nullptr || info->factory) return false;
    const bool ok = info->file.moveToTrash() || info->file.deleteFile();
    favourites.removeString (id);
    personalTags.erase (id);
    saveUserData();
    rescan();
    return ok;
}

//==============================================================================
void PresetManager::loadUserData()
{
    const auto v = juce::JSON::parse (getUserFolder().getChildFile ("UserData.json"));
    favourites.clear();
    personalTags.clear();
    if (auto* f = v["favourites"].getArray())
        for (auto& id : *f) favourites.addIfNotAlreadyThere (id.toString());
    recent.clear();
    if (auto* r = v["recent"].getArray())
        for (auto& id : *r) recent.addIfNotAlreadyThere (id.toString());
    if (auto* t = v["tags"].getDynamicObject())
        for (auto& prop : t->getProperties())
            if (auto* arr = prop.value.getArray())
            {
                juce::StringArray tags;
                for (auto& s : *arr) tags.add (s.toString());
                personalTags[prop.name.toString()] = tags;
            }
}

void PresetManager::saveUserData()
{
    auto* obj = new juce::DynamicObject();
    juce::Array<juce::var> fav;
    for (auto& f : favourites) fav.add (f);
    obj->setProperty ("favourites", fav);
    juce::Array<juce::var> rec;
    for (auto& r : recent) rec.add (r);
    obj->setProperty ("recent", rec);
    auto* tags = new juce::DynamicObject();
    for (auto& [id, list] : personalTags)
    {
        juce::Array<juce::var> arr;
        for (auto& s : list) arr.add (s);
        if (! arr.isEmpty()) tags->setProperty (id, arr);
    }
    obj->setProperty ("tags", juce::var (tags));
    getUserFolder().createDirectory();
    getUserFolder().getChildFile ("UserData.json").replaceWithText (juce::JSON::toString (juce::var (obj)));
}

void PresetManager::setFavourite (const juce::String& id, bool fav)
{
    if (id.isEmpty()) return;
    if (fav) favourites.addIfNotAlreadyThere (id);
    else favourites.removeString (id);
    saveUserData();
    sendChangeMessage();
}

juce::StringArray PresetManager::getPersonalTags (const juce::String& id) const
{
    auto it = personalTags.find (id);
    return it != personalTags.end() ? it->second : juce::StringArray();
}

void PresetManager::setPersonalTags (const juce::String& id, const juce::StringArray& tags)
{
    if (id.isEmpty()) return;
    juce::StringArray clean;
    for (auto t : tags) { t = t.trim(); if (t.isNotEmpty()) clean.addIfNotAlreadyThere (t); }
    personalTags[id] = clean;
    saveUserData();
    sendChangeMessage();
}

juce::StringArray PresetManager::getAllPersonalTags() const
{
    juce::StringArray all;
    for (auto& [id, list] : personalTags)
        for (auto& t : list) all.addIfNotAlreadyThere (t);
    all.sort (true);
    return all;
}

} // namespace pyra
