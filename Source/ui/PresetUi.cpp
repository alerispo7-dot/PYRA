#include "PresetUi.h"
#include "PyraLookAndFeel.h"
#include "../Lang.h"
#include "../PluginProcessor.h"

namespace pyra
{
using lang::tr;

namespace
{
    juce::Path starPath (juce::Rectangle<float> r)
    {
        juce::Path p;
        p.addStar (r.getCentre(), 5, r.getWidth() * 0.22f, r.getWidth() * 0.5f, 0.0f);
        return p;
    }

    void drawStar (juce::Graphics& g, juce::Rectangle<float> r, bool on)
    {
        const auto p = starPath (r);
        if (on) { g.setColour (colours::accent2); g.fillPath (p); }
        else { g.setColour (colours::textDim.withAlpha (0.7f)); g.strokePath (p, juce::PathStrokeType (1.3f)); }
    }

    /** Applies the filter and returns the matching presets (in library order). */
    juce::Array<const PresetInfo*> visiblePresets (const PresetManager& pm, PresetFilter& f)
    {
        juce::Array<const PresetInfo*> out;
        const auto words = juce::StringArray::fromTokens (f.search.toLowerCase(), " ", "");
        for (auto& p : pm.getAll())
        {
            if (f.category == "*fav" && ! pm.isFavourite (p.id)) continue;
            if (f.category == "*user" && p.factory) continue;
            if (f.category.startsWith ("#") && ! pm.getPersonalTags (p.id).contains (f.category.substring (1))) continue;
            if (f.category.isNotEmpty() && ! f.category.startsWith ("*") && ! f.category.startsWith ("#") && p.category != f.category) continue;
            if (f.subCategory.isNotEmpty() && p.subCategory != f.subCategory) continue;

            bool ok = true;
            const auto hay = (p.name + " " + p.category + " " + p.subCategory + " " + p.tags.joinIntoString (" ") + " "
                              + pm.getPersonalTags (p.id).joinIntoString (" ")).toLowerCase();
            for (auto& w : words) if (w.isNotEmpty() && ! hay.contains (w)) { ok = false; break; }
            if (ok) out.add (&p);
        }
        f.visibleIds.clearQuick();
        for (auto* p : out) f.visibleIds.add (p->id);
        return out;
    }

    juce::String catLabel (const juce::String& c)
    {
        if (c == "Brass & Winds") return tr ("Brass & Winds");
        return tr (c);
    }
}

//==============================================================================
PresetBar::PresetBar (PyraAudioProcessor& p, PresetFilter& f) : proc (p), pm (p.getPresetManager()), filter (f)
{
    pm.addChangeListener (this);
    for (auto* b : { &prev, &next, &save, &init }) addAndMakeVisible (b);
    save.setButtonText (tr ("SAVE"));
    init.setButtonText ("INIT");
    prev.setTooltip (tr ("Previous preset"));
    next.setTooltip (tr ("Next preset"));
    save.setTooltip (tr ("Save the current sound as your own preset"));
    init.setTooltip (tr ("Start from the default sound"));
    prev.onClick = [this] { step (-1); };
    next.onClick = [this] { step (1); };
    save.onClick = [this] { if (onSave) onSave(); };
    init.onClick = [this] { pm.loadInit(); };
    startTimerHz (4);
}

PresetBar::~PresetBar() { pm.removeChangeListener (this); }

void PresetBar::timerCallback()
{
    const bool m = pm.isModified();
    if (m != modified) { modified = m; repaint(); }
}

void PresetBar::step (int delta)
{
    if (filter.visibleIds.isEmpty()) visiblePresets (pm, filter);
    const auto& ids = filter.visibleIds;
    if (ids.isEmpty()) return;
    int idx = ids.indexOf (pm.getCurrentId());
    idx = idx < 0 ? (delta > 0 ? 0 : ids.size() - 1) : (idx + delta + ids.size()) % ids.size();
    pm.load (ids[idx]);
}

juce::Rectangle<int> PresetBar::nameArea() const
{
    return { 40, 3, prev.getX() - 46, getHeight() - 6 };
}

void PresetBar::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (colours::panel);
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (colours::edge);
    g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);

    const auto id = pm.getCurrentId();
    drawStar (g, starArea().toFloat().withSizeKeepingCentre (18.0f, 18.0f), pm.isFavourite (id));

    auto na = nameArea();
    g.setColour (colours::knobBody);
    g.fillRoundedRectangle (na.toFloat(), 4.0f);
    const auto* info = pm.find (id);
    auto textArea = na.reduced (10, 0);
    g.setColour (colours::text);
    g.setFont (PyraLookAndFeel::font (15.0f, true));
    const auto name = pm.getCurrentName() + (modified ? " *" : "");
    g.drawText (name, textArea, juce::Justification::centredLeft, true);
    if (info != nullptr)
    {
        g.setColour (colours::textDim);
        g.setFont (PyraLookAndFeel::font (12.0f));
        g.drawText (catLabel (info->category) + (info->subCategory.isNotEmpty() ? "  /  " + info->subCategory : juce::String())
                        + (info->factory ? juce::String() : "  " + tr ("(user)")),
                    textArea, juce::Justification::centredRight, true);
    }
}

void PresetBar::resized()
{
    auto r = getLocalBounds().reduced (4, 3);
    init.setBounds (r.removeFromRight (56));
    r.removeFromRight (6);
    save.setBounds (r.removeFromRight (72));
    r.removeFromRight (10);
    next.setBounds (r.removeFromRight (30));
    r.removeFromRight (4);
    prev.setBounds (r.removeFromRight (30));
}

void PresetBar::mouseUp (const juce::MouseEvent& e)
{
    if (starArea().contains (e.getPosition()))
    {
        const auto id = pm.getCurrentId();
        if (id.isNotEmpty()) pm.setFavourite (id, ! pm.isFavourite (id));
    }
    else if (nameArea().contains (e.getPosition()) && onOpenBrowser)
        onOpenBrowser();
}

//==============================================================================
SaveDialog::SaveDialog (PyraAudioProcessor& p, std::function<void()> onClose) : proc (p), close (std::move (onClose))
{
    auto& pm = proc.getPresetManager();
    for (auto* c : { (juce::Component*) &name, (juce::Component*) &tags, (juce::Component*) &category,
                     (juce::Component*) &saveBtn, (juce::Component*) &overwriteBtn, (juce::Component*) &cancelBtn })
        addAndMakeVisible (c);

    const auto* current = pm.find (pm.getCurrentId());
    name.setText (current != nullptr && ! current->factory ? current->name
                  : (pm.getCurrentName() == "Init" ? juce::String ("My Sound") : pm.getCurrentName() + " " + tr ("(edit)")));
    name.setFont (PyraLookAndFeel::font (16.0f));
    name.setSelectAllWhenFocused (true);
    tags.setTextToShowWhenEmpty (tr ("e.g. dark, trap, my song"), colours::textDim);
    tags.setFont (PyraLookAndFeel::font (14.0f));
    if (current != nullptr && ! current->factory) tags.setText (current->tags.joinIntoString (", "));

    int id = 1;
    category.addItem (tr ("User"), id++);
    for (auto& c : PresetManager::getCategories()) category.addItem (catLabel (c), id++);
    const auto cats = PresetManager::getCategories();
    const int catIdx = current != nullptr ? cats.indexOf (current->category) : -1;
    category.setSelectedId (catIdx >= 0 ? catIdx + 2 : 1, juce::dontSendNotification);

    saveBtn.setButtonText (tr ("SAVE AS NEW"));
    overwriteBtn.setButtonText (tr ("OVERWRITE"));
    cancelBtn.setButtonText (tr ("CANCEL"));
    saveBtn.setColour (juce::TextButton::buttonColourId, colours::accent.darker (0.2f));
    if (current != nullptr && ! current->factory) overwriteId = current->id;
    overwriteBtn.setVisible (overwriteId.isNotEmpty());
    if (overwriteId.isEmpty()) saveBtn.setButtonText (tr ("SAVE"));

    saveBtn.onClick = [this] { doSave (false); };
    overwriteBtn.onClick = [this] { doSave (true); };
    cancelBtn.onClick = [this] { if (close) close(); };
    name.onReturnKey = [this] { doSave (false); };
}

void SaveDialog::doSave (bool overwrite)
{
    auto& pm = proc.getPresetManager();
    const int sel = category.getSelectedId();
    const auto cat = sel <= 1 ? juce::String ("User") : PresetManager::getCategories()[sel - 2];
    juce::StringArray t;
    t.addTokens (tags.getText(), ",", "");
    t.trim(); t.removeEmptyStrings();
    pm.saveUser (name.getText(), cat, t, overwrite ? overwriteId : juce::String());
    if (close) close();
}

void SaveDialog::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black.withAlpha (0.55f));
    auto box = getLocalBounds().withSizeKeepingCentre (520, 290).toFloat();
    g.setColour (colours::panelHi);
    g.fillRoundedRectangle (box, 10.0f);
    g.setColour (colours::accent);
    g.drawRoundedRectangle (box, 10.0f, 1.2f);
    g.setFont (PyraLookAndFeel::font (18.0f, true));
    g.drawText (tr ("SAVE PRESET"), box.reduced (24, 16).removeFromTop (26), juce::Justification::centredLeft);
    g.setColour (colours::textDim);
    g.setFont (PyraLookAndFeel::font (11.5f, true));
    auto b = box.toNearestInt();
    g.drawText (tr ("NAME"), b.getX() + 24, b.getY() + 54, 200, 16, juce::Justification::centredLeft);
    g.drawText (tr ("CATEGORY"), b.getX() + 24, b.getY() + 116, 200, 16, juce::Justification::centredLeft);
    g.drawText (tr ("TAGS (separated by commas)"), b.getX() + 220, b.getY() + 116, 280, 16, juce::Justification::centredLeft);
    g.setFont (PyraLookAndFeel::font (11.0f));
    g.drawText (tr ("Saved in Documents/PYRA/Presets - it appears in the browser right away."),
                b.getX() + 24, b.getBottom() - 30, 480, 16, juce::Justification::centredLeft);
}

void SaveDialog::resized()
{
    auto b = getLocalBounds().withSizeKeepingCentre (520, 290);
    name.setBounds (b.getX() + 24, b.getY() + 72, 472, 32);
    category.setBounds (b.getX() + 24, b.getY() + 134, 180, 30);
    tags.setBounds (b.getX() + 220, b.getY() + 134, 276, 30);
    cancelBtn.setBounds (b.getX() + 24, b.getY() + 190, 110, 36);
    overwriteBtn.setBounds (b.getRight() - 24 - 150 - 10 - 130, b.getY() + 190, 130, 36);
    saveBtn.setBounds (b.getRight() - 24 - 150, b.getY() + 190, 150, 36);
}

//==============================================================================
namespace
{
    class PresetPage : public Page, private juce::ChangeListener, private juce::ListBoxModel
    {
    public:
        PresetPage (UiContext& ui, PresetFilter& f, std::function<void()> onSaveFn)
            : proc (ui.proc), pm (ui.proc.getPresetManager()), filter (f), onSave (std::move (onSaveFn))
        {
            pm.addChangeListener (this);
            addAndMakeVisible (search);
            search.setTextToShowWhenEmpty (tr ("Search presets, categories, tags..."), colours::textDim);
            search.setFont (PyraLookAndFeel::font (15.0f));
            search.setIndents (12, 8);
            search.onTextChange = [this] { filter.search = search.getText(); refresh(); };
            search.setText (filter.search, juce::dontSendNotification);

            addAndMakeVisible (list);
            list.setModel (this);
            list.setRowHeight (30);
            list.setColour (juce::ListBox::backgroundColourId, colours::panel);
            list.setColour (juce::ListBox::outlineColourId, colours::edge);

            for (auto* b : { &addTag, &rename, &remove, &saveBtn, &openFolder, &rescan }) addAndMakeVisible (b);
            addTag.setButtonText (tr ("TAGS..."));
            rename.setButtonText (tr ("RENAME"));
            remove.setButtonText (tr ("DELETE"));
            saveBtn.setButtonText (tr ("SAVE CURRENT SOUND"));
            openFolder.setButtonText (tr ("OPEN FOLDER"));
            rescan.setButtonText (tr ("REFRESH"));
            saveBtn.setColour (juce::TextButton::buttonColourId, colours::accent.darker (0.2f));
            addTag.onClick = [this] { editTags(); };
            rename.onClick = [this] { renameSelected(); };
            remove.onClick = [this] { deleteSelected(); };
            saveBtn.onClick = [this] { if (onSave) onSave(); };
            openFolder.onClick = [] { auto d = PresetManager::getUserFolder().getChildFile ("Presets"); d.createDirectory(); d.startAsProcess(); };
            rescan.onClick = [this] { pm.rescan(); };
            refresh();
        }

        ~PresetPage() override { pm.removeChangeListener (this); }

        // ---- left column (categories)
        struct SideItem { juce::String key, label; int count; bool header = false; };

        std::vector<SideItem> sideItems() const
        {
            std::vector<SideItem> items;
            int fav = 0, user = 0;
            for (auto& p : pm.getAll()) { if (pm.isFavourite (p.id)) ++fav; if (! p.factory) ++user; }
            items.push_back ({ "", tr ("All Presets"), pm.getAll().size() });
            items.push_back ({ "*fav", juce::String (juce::CharPointer_UTF8 ("\xe2\x98\x85 ")) + tr ("Favourites"), fav });
            items.push_back ({ "*user", tr ("My Presets"), user });
            items.push_back ({ "-", tr ("CATEGORIES"), 0, true });
            for (auto& c : PresetManager::getCategories())
            {
                int n = 0;
                for (auto& p : pm.getAll()) if (p.category == c) ++n;
                items.push_back ({ c, catLabel (c), n });
            }
            const auto tagsList = pm.getAllPersonalTags();
            if (! tagsList.isEmpty())
            {
                items.push_back ({ "-", tr ("MY TAGS"), 0, true });
                for (auto& t : tagsList)
                {
                    int n = 0;
                    for (auto& p : pm.getAll()) if (pm.getPersonalTags (p.id).contains (t)) ++n;
                    items.push_back ({ "#" + t, "# " + t, n });
                }
            }
            return items;
        }

        juce::Rectangle<int> sideArea() const { return { 8, 8, 230, getHeight() - 16 }; }

        void paint (juce::Graphics& g) override
        {
            auto side = sideArea();
            g.setColour (colours::panel);
            g.fillRoundedRectangle (side.toFloat(), 7.0f);
            g.setColour (colours::edge);
            g.drawRoundedRectangle (side.toFloat().reduced (0.5f), 7.0f, 1.0f);

            int y = side.getY() + 10;
            for (auto& it : sideItems())
            {
                if (it.header)
                {
                    y += 8;
                    g.setColour (colours::textDim);
                    g.setFont (PyraLookAndFeel::font (10.5f, true));
                    g.drawText (it.label, side.getX() + 14, y, side.getWidth() - 28, 18, juce::Justification::centredLeft);
                    y += 20;
                    continue;
                }
                const bool sel = filter.category == it.key;
                auto row = juce::Rectangle<int> (side.getX() + 6, y, side.getWidth() - 12, 26);
                if (sel) { g.setColour (colours::accent.withAlpha (0.25f)); g.fillRoundedRectangle (row.toFloat(), 4.0f); }
                g.setColour (sel ? colours::text : colours::text.withAlpha (0.8f));
                g.setFont (PyraLookAndFeel::font (13.5f, sel));
                g.drawText (it.label, row.reduced (10, 0), juce::Justification::centredLeft);
                g.setColour (colours::textDim);
                g.setFont (PyraLookAndFeel::font (11.5f));
                g.drawText (juce::String (it.count), row.reduced (10, 0), juce::Justification::centredRight);
                y += 27;
            }

            // info panel for the selected preset
            auto info = infoArea();
            g.setColour (colours::panel);
            g.fillRoundedRectangle (info.toFloat(), 7.0f);
            g.setColour (colours::edge);
            g.drawRoundedRectangle (info.toFloat().reduced (0.5f), 7.0f, 1.0f);
            if (auto* p = selectedPreset())
            {
                auto r = info.reduced (16, 14);
                g.setColour (colours::accent);
                g.setFont (PyraLookAndFeel::font (19.0f, true));
                g.drawFittedText (p->name, r.removeFromTop (28), juce::Justification::centredLeft, 1);
                g.setColour (colours::text);
                g.setFont (PyraLookAndFeel::font (13.5f));
                g.drawText (catLabel (p->category) + (p->subCategory.isNotEmpty() ? "  /  " + p->subCategory : juce::String()),
                            r.removeFromTop (22), juce::Justification::centredLeft);
                g.setColour (colours::textDim);
                g.drawText ((p->factory ? tr ("Factory preset") : tr ("Your preset")) + (p->author.isNotEmpty() ? "  -  " + p->author : juce::String()),
                            r.removeFromTop (22), juce::Justification::centredLeft);
                r.removeFromTop (10);
                auto drawTags = [&] (const juce::StringArray& tagList, juce::Colour c, const juce::String& title)
                {
                    if (tagList.isEmpty()) return;
                    g.setColour (colours::textDim);
                    g.setFont (PyraLookAndFeel::font (10.5f, true));
                    g.drawText (title, r.removeFromTop (18), juce::Justification::centredLeft);
                    int x = r.getX(), yy = r.getY();
                    g.setFont (PyraLookAndFeel::font (12.0f));
                    for (auto& t : tagList)
                    {
                        const int w = juce::GlyphArrangement::getStringWidthInt (g.getCurrentFont(), t) + 16;
                        if (x + w > r.getRight()) { x = r.getX(); yy += 26; }
                        g.setColour (c.withAlpha (0.2f));
                        g.fillRoundedRectangle ((float) x, (float) yy, (float) w, 21.0f, 10.0f);
                        g.setColour (c);
                        g.drawText (t, x, yy, w, 21, juce::Justification::centred);
                        x += w + 6;
                    }
                    r.removeFromTop (yy - r.getY() + 30);
                };
                drawTags (p->tags, colours::accent2, tr ("TAGS"));
                drawTags (pm.getPersonalTags (p->id), colours::mod, tr ("MY TAGS"));
            }
            else
            {
                g.setColour (colours::textDim);
                g.setFont (PyraLookAndFeel::font (13.0f));
                g.drawFittedText (tr ("Click a preset to load it. Click the star to add it to your favourites."),
                                  info.reduced (16), juce::Justification::topLeft, 4);
            }
        }

        juce::Rectangle<int> infoArea() const { return { getWidth() - 8 - 300, 8, 300, getHeight() - 16 - 150 }; }

        void resized() override
        {
            const int x = 230 + 16, right = getWidth() - 8 - 300 - 8;
            search.setBounds (x, 8, right - x, 34);
            list.setBounds (x, 50, right - x, getHeight() - 58);
            auto info = infoArea();
            auto bottom = juce::Rectangle<int> (info.getX(), info.getBottom() + 8, info.getWidth(), getHeight() - info.getBottom() - 16);
            auto row1 = bottom.removeFromTop (34);
            const int w = (row1.getWidth() - 12) / 3;
            addTag.setBounds (row1.removeFromLeft (w)); row1.removeFromLeft (6);
            rename.setBounds (row1.removeFromLeft (w)); row1.removeFromLeft (6);
            remove.setBounds (row1);
            bottom.removeFromTop (8);
            saveBtn.setBounds (bottom.removeFromTop (38));
            bottom.removeFromTop (8);
            auto row3 = bottom.removeFromTop (30);
            openFolder.setBounds (row3.removeFromLeft (row3.getWidth() / 2 - 3));
            row3.removeFromLeft (6);
            rescan.setBounds (row3);
        }

        void mouseUp (const juce::MouseEvent& e) override
        {
            if (! sideArea().contains (e.getPosition())) return;
            int y = sideArea().getY() + 10;
            for (auto& it : sideItems())
            {
                if (it.header) { y += 28; continue; }
                if (e.y >= y && e.y < y + 27) { filter.category = it.key; filter.subCategory = {}; refresh(); repaint(); return; }
                y += 27;
            }
        }

        // ---- list
        int getNumRows() override { return rows.size(); }

        void paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool selected) override
        {
            if (! juce::isPositiveAndBelow (row, rows.size())) return;
            auto* p = rows[row];
            const bool current = p->id == pm.getCurrentId();
            if (selected) g.fillAll (colours::accent.withAlpha (0.22f));
            else if (row % 2) g.fillAll (colours::panelHi.withAlpha (0.5f));
            drawStar (g, juce::Rectangle<float> (8.0f, (float) h * 0.5f - 8.0f, 16.0f, 16.0f), pm.isFavourite (p->id));
            g.setColour (current ? colours::accent2 : colours::text);
            g.setFont (PyraLookAndFeel::font (14.0f, current));
            g.drawText (p->name, 34, 0, w / 2 - 34, h, juce::Justification::centredLeft, true);
            g.setColour (colours::textDim);
            g.setFont (PyraLookAndFeel::font (12.0f));
            g.drawText (catLabel (p->category) + "  /  " + (p->subCategory.isNotEmpty() ? p->subCategory : tr ("User")),
                        w / 2, 0, w / 2 - 10, h, juce::Justification::centredLeft, true);
            if (! p->factory)
            {
                g.setColour (colours::mod);
                g.drawText (tr ("user"), 0, 0, w - 10, h, juce::Justification::centredRight);
            }
        }

        void listBoxItemClicked (int row, const juce::MouseEvent& e) override
        {
            if (! juce::isPositiveAndBelow (row, rows.size())) return;
            auto* p = rows[row];
            if (e.x < 30) { pm.setFavourite (p->id, ! pm.isFavourite (p->id)); return; }
            selectedId = p->id;
            pm.load (p->id);
            repaint();
        }

        void selectedRowsChanged (int row) override
        {
            if (juce::isPositiveAndBelow (row, rows.size()))
            {
                selectedId = rows[row]->id;
                repaint (infoArea());
                const bool user = ! rows[row]->factory;
                rename.setEnabled (user);
                remove.setEnabled (user);
            }
        }

        void returnKeyPressed (int row) override
        {
            if (juce::isPositiveAndBelow (row, rows.size())) pm.load (rows[row]->id);
        }

        const PresetInfo* selectedPreset() const
        {
            if (auto* p = pm.find (selectedId)) return p;
            return pm.find (pm.getCurrentId());
        }

        void refresh()
        {
            rows = visiblePresets (pm, filter);
            list.updateContent();
            list.repaint();
            int idx = -1;
            for (int i = 0; i < rows.size(); ++i) if (rows[i]->id == pm.getCurrentId()) idx = i;
            if (idx >= 0) { list.selectRow (idx, true, true); }
            const auto* sel = selectedPreset();
            rename.setEnabled (sel != nullptr && ! sel->factory);
            remove.setEnabled (sel != nullptr && ! sel->factory);
            addTag.setEnabled (sel != nullptr);
        }

        void changeListenerCallback (juce::ChangeBroadcaster*) override { refresh(); repaint(); }

        // ---- actions
        void editTags()
        {
            auto* p = selectedPreset();
            if (p == nullptr) return;
            const auto id = p->id;
            auto* w = new juce::AlertWindow (tr ("My tags"), tr ("Your own tags for") + " \"" + p->name + "\" "
                                             + tr ("(separated by commas). They never change the sound or your projects."),
                                             juce::MessageBoxIconType::NoIcon, this);
            w->addTextEditor ("tags", pm.getPersonalTags (id).joinIntoString (", "));
            w->addButton (tr ("OK"), 1, juce::KeyPress (juce::KeyPress::returnKey));
            w->addButton (tr ("CANCEL"), 0, juce::KeyPress (juce::KeyPress::escapeKey));
            juce::Component::SafePointer<PresetPage> safe (this);
            w->enterModalState (true, juce::ModalCallbackFunction::create ([safe, w, id] (int r)
            {
                if (safe == nullptr || r != 1) return;
                juce::StringArray t;
                t.addTokens (w->getTextEditorContents ("tags"), ",", "");
                t.trim(); t.removeEmptyStrings();
                safe->pm.setPersonalTags (id, t);
            }), true);
        }

        void renameSelected()
        {
            auto* p = selectedPreset();
            if (p == nullptr || p->factory) return;
            const auto id = p->id;
            auto* w = new juce::AlertWindow (tr ("Rename preset"), tr ("Projects that use this preset keep sounding exactly the same."),
                                             juce::MessageBoxIconType::NoIcon, this);
            w->addTextEditor ("name", p->name);
            w->addButton (tr ("OK"), 1, juce::KeyPress (juce::KeyPress::returnKey));
            w->addButton (tr ("CANCEL"), 0, juce::KeyPress (juce::KeyPress::escapeKey));
            juce::Component::SafePointer<PresetPage> safe (this);
            w->enterModalState (true, juce::ModalCallbackFunction::create ([safe, w, id] (int r)
            {
                if (safe != nullptr && r == 1) safe->pm.renameUser (id, w->getTextEditorContents ("name"));
            }), true);
        }

        void deleteSelected()
        {
            auto* p = selectedPreset();
            if (p == nullptr || p->factory) return;
            const auto id = p->id;
            juce::Component::SafePointer<PresetPage> safe (this);
            juce::AlertWindow::showOkCancelBox (juce::MessageBoxIconType::WarningIcon, tr ("Delete preset"),
                "\"" + p->name + "\" " + tr ("will be moved to the Trash."), tr ("DELETE"), tr ("CANCEL"), this,
                juce::ModalCallbackFunction::create ([safe, id] (int r) { if (safe != nullptr && r == 1) safe->pm.deleteUser (id); }));
        }

        PyraAudioProcessor& proc;
        PresetManager& pm;
        PresetFilter& filter;
        std::function<void()> onSave;
        juce::TextEditor search;
        juce::ListBox list;
        juce::TextButton addTag, rename, remove, saveBtn, openFolder, rescan;
        juce::Array<const PresetInfo*> rows;
        juce::String selectedId;
    };
}

std::unique_ptr<Page> createPresetPage (UiContext& ui, PresetFilter& f, std::function<void()> onSave)
{
    return std::make_unique<PresetPage> (ui, f, std::move (onSave));
}

} // namespace pyra
