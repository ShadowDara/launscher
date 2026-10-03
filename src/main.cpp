// App Launcher - wxWidgets + nlohmann/json
// Karten mit optionalem Bild, Start-Batch-Script, Hover-Beschreibung, Suche.
// Daten:   launcher.json  (neben der .exe)
// Scripts: scripts\*.bat  (neben der .exe, werden bei neuen Karten automatisch
//                          angelegt)
//
// Hinweis: Quelldatei ist UTF-8. Mit MSVC bitte /utf-8 verwenden.
// tinyfiledialogs wird nicht mehr benoetigt (wxMessageBox / wxFileDialog).

#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #define NOMINMAX
#endif

#include <wx/dcbuffer.h>
#include <wx/image.h>
#include <wx/log.h>
#include <wx/scrolwin.h>
#include <wx/stdpaths.h>
#include <wx/tooltip.h>
#include <wx/wx.h>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include <more/json.hpp>

#ifdef _WIN32
    #include <shellapi.h>
    #include <windows.h>
#endif

namespace fs = std::filesystem;
using json = nlohmann::json;

// ----------------------------------------------------------------------------
// Datenmodell
// ----------------------------------------------------------------------------
struct Entry
{
    std::string name;
    std::string description;
    std::string image;  // optional, relativ zur exe oder absolut
    std::string script; // Start-Batch, relativ zur exe oder absolut
};

static std::vector<Entry> g_entries;
static fs::path g_base; // Ordner der exe

// ----------------------------------------------------------------------------
// Pfad-/String-Helfer
// ----------------------------------------------------------------------------
static wxString W(const std::string &s)
{
    return wxString::FromUTF8(s.c_str());
}
static std::string S(const wxString &w) { return w.utf8_string(); }

static std::string toUtf8(const fs::path &p)
{
    auto u = p.u8string();
    return std::string(u.begin(), u.end());
}

static fs::path fromUtf8(const std::string &s) { return fs::u8path(s); }

static fs::path exeDir()
{
#ifdef _WIN32
    wchar_t buf[MAX_PATH * 4];
    DWORD n =
        GetModuleFileNameW(nullptr, buf, (DWORD)(sizeof(buf) / sizeof(buf[0])));
    return fs::path(std::wstring(buf, n)).parent_path();
#else
    return fromUtf8(S(wxStandardPaths::Get().GetExecutablePath()))
        .parent_path();
#endif
}

static fs::path resolve(const std::string &s)
{
    fs::path p = fromUtf8(s);
    return p.is_relative() ? g_base / p : p;
}

// Pfad relativ zur exe speichern, wenn er darunter liegt
static std::string storePath(const fs::path &p)
{
    std::error_code ec;
    fs::path rel = fs::relative(p, g_base, ec);
    if (!ec && !rel.empty() && *rel.begin() != "..")
    {
        auto u = rel.generic_u8string();
        return std::string(u.begin(), u.end());
    }
    return toUtf8(p);
}

static std::string lower(std::string s)
{
    for (auto &c : s)
        if ((unsigned char)c < 128)
            c = (char)std::tolower((unsigned char)c);
    return s;
}

// ----------------------------------------------------------------------------
// Persistenz
// ----------------------------------------------------------------------------
static fs::path dataFile() { return g_base / "launcher.json"; }

static void saveEntries()
{
    json arr = json::array();
    for (auto &e : g_entries)
        arr.push_back({{"name", e.name},
                       {"description", e.description},
                       {"image", e.image},
                       {"script", e.script}});
    json j;
    j["apps"] = arr;
    std::ofstream f(dataFile(), std::ios::binary);
    f << j.dump(2);
}

static void loadEntries()
{
    try
    {
        std::ifstream f(dataFile(), std::ios::binary);
        if (!f)
            return;
        json j = json::parse(f, nullptr, true, true);
        for (auto &a : j.value("apps", json::array()))
        {
            Entry e;
            e.name = a.value("name", std::string());
            e.description = a.value("description", std::string());
            e.image = a.value("image", std::string());
            e.script = a.value("script", std::string());
            g_entries.push_back(std::move(e));
        }
    }
    catch (const std::exception &ex)
    {
        std::string msg =
            std::string("launcher.json konnte nicht gelesen werden:\n") +
            ex.what();
        wxMessageBox(W(msg), "Fehler", wxOK | wxICON_ERROR);
    }
}

// ----------------------------------------------------------------------------
// Scripts anlegen / starten / bearbeiten
// ----------------------------------------------------------------------------
static void writeTemplate(const fs::path &p, const std::string &name)
{
    std::ofstream f(p, std::ios::binary);
    f << "@echo off\r\n"
      << "cd /d \"%~dp0\"\r\n"
      << "rem === Start-Script fuer: " << name << " ===\r\n"
      << "rem Trage hier den Startbefehl ein, z.B.:\r\n"
      << "rem start \"\" \"C:\\Programme\\MeineApp\\app.exe\"\r\n";
}

static std::string slug(const std::string &name)
{
    std::string s;
    for (unsigned char c : name)
    {
        if (c < 128 && std::isalnum(c))
            s += (char)c;
        else if (!s.empty() && s.back() != '_')
            s += '_';
    }
    while (!s.empty() && s.back() == '_')
        s.pop_back();
    return s.empty() ? "app" : s;
}

// legt scripts/<name>.bat an und liefert den (relativen) Pfad
static std::string createScriptFile(const std::string &name)
{
    std::error_code ec;
    fs::path dir = g_base / "scripts";
    fs::create_directories(dir, ec);
    std::string stem = slug(name);
    fs::path p = dir / fromUtf8(stem + ".bat");
    for (int n = 2; fs::exists(p, ec); ++n)
        p = dir / fromUtf8(stem + "_" + std::to_string(n) + ".bat");
    writeTemplate(p, name);
    return storePath(p);
}

// true, wenn ein neues Script angelegt wurde (Pfad war leer)
static bool ensureScript(Entry &e)
{
    if (e.script.empty())
    {
        e.script = createScriptFile(e.name);
        return true;
    }
    std::error_code ec;
    fs::path p = resolve(e.script);
    if (!fs::exists(p, ec))
    {
        fs::create_directories(p.parent_path(), ec);
        writeTemplate(p, e.name);
    }
    return false;
}

#ifdef _WIN32
static std::wstring q(const fs::path &p) { return L"\"" + p.wstring() + L"\""; }

static bool shellOpen(const wchar_t *verb, const fs::path &file)
{
    HINSTANCE r =
        ShellExecuteW(nullptr, verb, file.wstring().c_str(), nullptr,
                      file.parent_path().wstring().c_str(), SW_SHOWNORMAL);
    return (INT_PTR)r > 32;
}

static void runFile(const fs::path &p)
{
    if (!shellOpen(L"open", p))
        wxMessageBox("Script konnte nicht gestartet werden.", "Fehler",
                     wxOK | wxICON_ERROR);
}

// Standard-Editor fuer .bat (Verb "edit"), Fallback Notepad
static void editFile(const fs::path &p)
{
    if (shellOpen(L"edit", p))
        return;
    ShellExecuteW(nullptr, L"open", L"notepad.exe", q(p).c_str(), nullptr,
                  SW_SHOWNORMAL);
}

static void showInFolder(const fs::path &p)
{
    std::wstring arg = L"/select," + q(p);
    ShellExecuteW(nullptr, L"open", L"explorer.exe", arg.c_str(), nullptr,
                  SW_SHOWNORMAL);
}
#else
static void runFile(const fs::path &p)
{
    std::system(("\"" + p.string() + "\" &").c_str());
}
static void editFile(const fs::path &p)
{
    std::system(("xdg-open \"" + p.string() + "\" &").c_str());
}
static void showInFolder(const fs::path &p)
{
    std::system(("xdg-open \"" + p.parent_path().string() + "\" &").c_str());
}
#endif

// ----------------------------------------------------------------------------
// Dialog: Karte anlegen / bearbeiten
// ----------------------------------------------------------------------------
class EntryDialog : public wxDialog
{
  public:
    wxTextCtrl *name, *desc, *image, *script;

    EntryDialog(const Entry &e, bool isNew)
        : wxDialog(nullptr, wxID_ANY, isNew ? "Neue App" : "App bearbeiten",
                   wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE)
    {
        auto *top = new wxBoxSizer(wxVERTICAL);
        auto *grid = new wxFlexGridSizer(2, 8, 8);
        grid->AddGrowableCol(1);

        name = new wxTextCtrl(this, wxID_ANY, W(e.name));
        desc =
            new wxTextCtrl(this, wxID_ANY, W(e.description), wxDefaultPosition,
                           wxSize(370, 90), wxTE_MULTILINE | wxTE_WORDWRAP);
        image = new wxTextCtrl(this, wxID_ANY, W(e.image));
        script = new wxTextCtrl(this, wxID_ANY, W(e.script));
        auto *bImg = new wxButton(this, wxID_ANY, W("Wählen"));
        auto *bScr = new wxButton(this, wxID_ANY, W("Wählen"));

        auto *rowImg = new wxBoxSizer(wxHORIZONTAL);
        rowImg->Add(image, 1, wxEXPAND | wxRIGHT, 5);
        rowImg->Add(bImg, 0, wxEXPAND);
        auto *rowScr = new wxBoxSizer(wxHORIZONTAL);
        rowScr->Add(script, 1, wxEXPAND | wxRIGHT, 5);
        rowScr->Add(bScr, 0, wxEXPAND);

        grid->Add(new wxStaticText(this, wxID_ANY, "Name:"), 0,
                  wxALIGN_CENTER_VERTICAL);
        grid->Add(name, 1, wxEXPAND);
        grid->Add(new wxStaticText(this, wxID_ANY, "Beschreibung:"), 0,
                  wxALIGN_TOP);
        grid->Add(desc, 1, wxEXPAND);
        grid->Add(new wxStaticText(this, wxID_ANY, "Bild (optional):"), 0,
                  wxALIGN_CENTER_VERTICAL);
        grid->Add(rowImg, 1, wxEXPAND);
        grid->Add(new wxStaticText(this, wxID_ANY, "Start-Script:"), 0,
                  wxALIGN_CENTER_VERTICAL);
        grid->Add(rowScr, 1, wxEXPAND);
        grid->AddSpacer(0);
        auto *hint = new wxStaticText(
            this, wxID_ANY,
            W("Script leer lassen: es wird automatisch ein neues "
              "Batch-Script angelegt und im Standard-Editor geöffnet."));
        hint->Wrap(370);
        grid->Add(hint, 1, wxEXPAND);

        auto *btns = new wxBoxSizer(wxHORIZONTAL);
        auto *bOk = new wxButton(this, wxID_OK, "Speichern");
        auto *bCancel = new wxButton(this, wxID_CANCEL, "Abbrechen");
        btns->AddStretchSpacer(1);
        btns->Add(bOk, 0, wxRIGHT, 10);
        btns->Add(bCancel, 0);

        top->Add(grid, 1, wxEXPAND | wxALL, 12);
        top->Add(btns, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 12);
        SetSizerAndFit(top);
        SetMinSize(GetSize());
        bOk->SetDefault();
        CentreOnScreen();

        bImg->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { browseImage(); });
        bScr->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { browseScript(); });
        Bind(wxEVT_BUTTON, &EntryDialog::onOk, this, wxID_OK);
    }

  private:
    void browseImage()
    {
        wxFileDialog dlg(this, W("Bild auswählen"), "", "",
                         "Bilder|*.png;*.jpg;*.jpeg;*.gif;*.bmp",
                         wxFD_OPEN | wxFD_FILE_MUST_EXIST);
        if (dlg.ShowModal() == wxID_OK)
            image->SetValue(W(storePath(fromUtf8(S(dlg.GetPath())))));
    }

    void browseScript()
    {
        wxFileDialog dlg(
            this, W("Start-Script auswählen"), W(toUtf8(g_base / "scripts")),
            "", "Batch-Dateien|*.bat;*.cmd", wxFD_OPEN | wxFD_FILE_MUST_EXIST);
        if (dlg.ShowModal() == wxID_OK)
            script->SetValue(W(storePath(fromUtf8(S(dlg.GetPath())))));
    }

    void onOk(wxCommandEvent &ev)
    {
        if (S(name->GetValue()).empty())
        {
            wxMessageBox("Bitte einen Namen eingeben.", "Name fehlt",
                         wxOK | wxICON_INFORMATION, this);
            return; // Dialog bleibt offen
        }
        ev.Skip(); // Standardverhalten: Dialog mit wxID_OK schliessen
    }
};

static bool editEntryDialog(Entry &e, bool isNew)
{
    EntryDialog d(e, isNew);
    if (d.ShowModal() != wxID_OK)
        return false;
    e.name = S(d.name->GetValue());
    e.description = S(d.desc->GetValue());
    e.image = S(d.image->GetValue());
    e.script = S(d.script->GetValue());
    return true;
}

// ----------------------------------------------------------------------------
// Aktionen (von den Karten aufgerufen)
// ----------------------------------------------------------------------------
static void refresh();

static bool validIdx(int i) { return i >= 0 && i < (int)g_entries.size(); }

static void actionStart(int idx)
{
    if (!validIdx(idx))
        return;
    auto &e = g_entries[idx];
    std::error_code ec;
    if (e.script.empty() || !fs::exists(resolve(e.script), ec))
    {
        wxMessageBox(W("Für diese App gibt es noch kein Start-Script.\nMit "
                       "dem Button \"Script\" kann es angelegt "
                       "und bearbeitet werden."),
                     "Script fehlt", wxOK | wxICON_WARNING);
        return;
    }
    runFile(resolve(e.script));
}

static void actionEditScript(int idx)
{
    if (!validIdx(idx))
        return;
    auto &e = g_entries[idx];
    if (ensureScript(e))
    {
        saveEntries();
        wxTheApp->CallAfter([] { refresh(); });
    }
    editFile(resolve(e.script));
}

static void cbProps(int idx)
{
    if (!validIdx(idx))
        return;
    Entry copy = g_entries[idx];
    if (editEntryDialog(copy, false))
    {
        g_entries[idx] = copy;
        saveEntries();
        refresh();
    }
}

static void cbDelete(int idx)
{
    if (!validIdx(idx))
        return;
    std::string msg =
        "" + g_entries[idx].name +
        " aus dem Launcher entfernen?\n(Die Script-Datei bleibt erhalten.)";
    if (wxMessageBox(W(msg), "Entfernen", wxYES_NO | wxICON_QUESTION) == wxYES)
    {
        g_entries.erase(g_entries.begin() + idx);
        saveEntries();
        refresh();
    }
}

static void actionFolder(int idx)
{
    if (!validIdx(idx) || g_entries[idx].script.empty())
        return;
    showInFolder(resolve(g_entries[idx].script));
}

// ----------------------------------------------------------------------------
// Karte
// ----------------------------------------------------------------------------
static wxColour avgColour(const wxColour &a, const wxColour &b, double wa)
{
    auto mix = [&](int x, int y)
    { return (unsigned char)(x * wa + y * (1.0 - wa) + 0.5); };
    return wxColour(mix(a.Red(), b.Red()), mix(a.Green(), b.Green()),
                    mix(a.Blue(), b.Blue()));
}

// Mausrad an den Eltern-Scroller durchreichen
static void forwardWheel(wxWindow *w)
{
    w->Bind(wxEVT_MOUSEWHEEL,
            [](wxMouseEvent &ev)
            {
                ev.ResumePropagation(wxEVENT_PROPAGATE_MAX);
                ev.Skip();
            });
}

// Bildflaeche: Bild oder Platzhalter-Buchstabe, Klick = Start
class ImageBox : public wxPanel
{
  public:
    ImageBox(wxWindow *parent, const wxPoint &pos, const wxSize &size)
        : wxPanel(parent, wxID_ANY, pos, size)
    {
        SetBackgroundStyle(wxBG_STYLE_PAINT);
        Bind(wxEVT_PAINT, &ImageBox::onPaint, this);
    }

    void setBitmap(const wxBitmap &b) { bmp_ = b; }
    void setLetter(const wxString &l) { letter_ = l; }

  private:
    wxBitmap bmp_;
    wxString letter_;

    void onPaint(wxPaintEvent &)
    {
        wxAutoBufferedPaintDC dc(this);
        wxColour bg =
            avgColour(wxSystemSettings::GetColour(wxSYS_COLOUR_BTNFACE),
                      wxSystemSettings::GetColour(wxSYS_COLOUR_LISTBOX), 0.6);
        dc.SetBackground(wxBrush(bg));
        dc.Clear();
        wxSize cs = GetClientSize();

        if (bmp_.IsOk())
        {
            dc.DrawBitmap(bmp_, (cs.x - bmp_.GetWidth()) / 2,
                          (cs.y - bmp_.GetHeight()) / 2, true);
            return;
        }
        wxFont f = dc.GetFont();
        f.SetWeight(wxFONTWEIGHT_BOLD);
        f.SetPixelSize(wxSize(0, 52));
        dc.SetFont(f);
        dc.SetTextForeground(
            avgColour(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT),
                      wxSystemSettings::GetColour(wxSYS_COLOUR_BTNFACE), 0.35));
        wxSize te = dc.GetTextExtent(letter_);
        dc.DrawText(letter_, (cs.x - te.x) / 2, (cs.y - te.y) / 2);
    }
};

class Card : public wxPanel
{
  public:
    static constexpr int W_ = 200, H_ = 194;

    Card(wxWindow *parent, int idx, const Entry &e)
        : wxPanel(parent, wxID_ANY, wxDefaultPosition, wxSize(W_, H_),
                  wxBORDER_NONE),
          idx_(idx)
    {
        SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_LISTBOX));
        Bind(wxEVT_PAINT, &Card::onPaint, this);

        name_ = e.name;
        tip_ = name_;
        if (!e.description.empty())
            tip_ += "\n\n" + e.description;
        tip_ += "\n\nScript: " +
                (e.script.empty() ? std::string("(noch keins)") : e.script);

        img_ = new ImageBox(this, wxPoint(8, 8), wxSize(W_ - 16, 104));
        setupImage(e);
        img_->Bind(wxEVT_LEFT_UP,
                   [this](wxMouseEvent &) { actionStart(idx_); });

        auto *title =
            new wxStaticText(this, wxID_ANY, W(name_), wxPoint(8, 116),
                             wxSize(W_ - 16, 28), wxST_ELLIPSIZE_END);
        wxFont tf = title->GetFont();
        tf.MakeBold();
        title->SetFont(tf);

        auto *bStart = new wxButton(this, wxID_ANY, "Start",
                                    wxPoint(8, H_ - 38), wxSize(68, 28));
        bStart->Bind(wxEVT_BUTTON,
                     [this](wxCommandEvent &) { actionStart(idx_); });
        auto *bEdit = new wxButton(this, wxID_ANY, "Script",
                                   wxPoint(80, H_ - 38), wxSize(68, 28));
        bEdit->Bind(wxEVT_BUTTON,
                    [this](wxCommandEvent &) { actionEditScript(idx_); });
        menuBtn_ = new wxButton(this, wxID_ANY, "...", wxPoint(152, H_ - 38),
                                wxSize(40, 28));
        menuBtn_->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { onMenu(); });

        // Tooltip (Hover-Beschreibung) auf Karte und alle Kinder, Mausrad
        // weiterreichen
        SetToolTip(W(tip_));
        forwardWheel(this);
        for (auto *c : GetChildren())
        {
            c->SetToolTip(W(tip_));
            forwardWheel(c);
        }
    }

    int idx() const { return idx_; }

  private:
    int idx_;
    std::string name_, tip_, letter_;
    ImageBox *img_ = nullptr;
    wxButton *menuBtn_ = nullptr;

    void onPaint(wxPaintEvent &)
    {
        wxPaintDC dc(this);
        dc.SetBrush(*wxTRANSPARENT_BRUSH);
        dc.SetPen(wxPen(wxSystemSettings::GetColour(wxSYS_COLOUR_3DSHADOW)));
        wxSize cs = GetClientSize();
        dc.DrawRectangle(0, 0, cs.x, cs.y);
    }

    void setupImage(const Entry &e)
    {
        if (!e.image.empty())
        {
            wxLogNull noLog;
            wxImage im;
            if (im.LoadFile(W(toUtf8(resolve(e.image)))) && im.IsOk() &&
                im.GetWidth() > 0 && im.GetHeight() > 0)
            {
                double sc =
                    std::min(184.0 / im.GetWidth(), 104.0 / im.GetHeight());
                sc = std::min(sc, 2.0);
                int nw = std::max(1, (int)(im.GetWidth() * sc)),
                    nh = std::max(1, (int)(im.GetHeight() * sc));
                im.Rescale(nw, nh, wxIMAGE_QUALITY_HIGH);
                img_->setBitmap(wxBitmap(im));
                return;
            }
        }
        // Platzhalter: erster Buchstabe
        unsigned char c = e.name.empty() ? '?' : (unsigned char)e.name[0];
        size_t n = c >= 0xF0 ? 4 : c >= 0xE0 ? 3 : c >= 0xC0 ? 2 : 1;
        letter_ = e.name.empty() ? "?" : e.name.substr(0, n);
        if (n == 1)
            letter_[0] = (char)std::toupper((unsigned char)letter_[0]);
        img_->setLetter(W(letter_));
    }

    void onMenu()
    {
        enum
        {
            ID_PROPS = 1,
            ID_FOLDER,
            ID_DELETE
        };
        wxMenu m;
        m.Append(ID_PROPS, "Eigenschaften bearbeiten…");
        m.Append(ID_FOLDER, "Script-Ordner öffnen");
        m.Append(ID_DELETE, "Entfernen");
        int id = GetPopupMenuSelectionFromUser(
            m, menuBtn_->GetPosition() + wxPoint(0, menuBtn_->GetSize().y));
        int i = idx_;
        switch (id)
        {
        case ID_PROPS:
            wxTheApp->CallAfter([i] { cbProps(i); });
            break;
        case ID_FOLDER:
            actionFolder(i);
            break;
        case ID_DELETE:
            wxTheApp->CallAfter([i] { cbDelete(i); });
            break;
        }
    }
};

// ----------------------------------------------------------------------------
// Scroll-Grid mit Karten
// ----------------------------------------------------------------------------
static bool matches(const Entry &e, const std::vector<std::string> &terms)
{
    std::string hay = lower(e.name + "\n" + e.description);
    for (auto &t : terms)
        if (hay.find(t) == std::string::npos)
            return false;
    return true;
}

static std::vector<std::string> splitTerms(const std::string &s)
{
    std::vector<std::string> out;
    std::istringstream is(lower(s));
    std::string t;
    while (is >> t)
        out.push_back(t);
    return out;
}

class Grid : public wxScrolledWindow
{
  public:
    static constexpr int GAP = 14;

    explicit Grid(wxWindow *parent)
        : wxScrolledWindow(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                           wxVSCROLL | wxBORDER_SUNKEN)
    {
        SetScrollRate(0, 20);
        SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_BTNFACE));
        Bind(wxEVT_SIZE,
             [this](wxSizeEvent &ev)
             {
                 relayout();
                 ev.Skip();
             });
    }

    int rebuild(const std::string &filter)
    {
        Freeze();
        for (auto *c : cards_)
            c->Destroy();
        cards_.clear();
        Scroll(0, 0);

        auto terms = splitTerms(filter);
        for (int i = 0; i < (int)g_entries.size(); i++)
            if (matches(g_entries[i], terms))
                cards_.push_back(new Card(this, i, g_entries[i]));

        relayout();
        Thaw();
        Refresh();
        return (int)cards_.size();
    }

    void relayout()
    {
        int sb = wxSystemSettings::GetMetric(wxSYS_VSCROLL_X);
        int total = GetClientSize().x + (HasScrollbar(wxVERTICAL) ? sb : 0);
        int usable = total - sb - GAP;
        int cols = std::max(1, usable / (Card::W_ + GAP));
        int n = (int)cards_.size();
        int rows = (n + cols - 1) / cols;
        int vh = n ? GAP + rows * (Card::H_ + GAP) : 0;

        SetVirtualSize(GetClientSize().x, vh);

        for (int i = 0; i < n; i++)
        {
            wxPoint logical(GAP + (i % cols) * (Card::W_ + GAP),
                            GAP + (i / cols) * (Card::H_ + GAP));
            cards_[i]->Move(CalcScrolledPosition(logical));
        }
    }

  private:
    std::vector<Card *> cards_;
};

// ----------------------------------------------------------------------------
// Hauptfenster
// ----------------------------------------------------------------------------
static Grid *g_grid = nullptr;
static wxTextCtrl *g_search = nullptr;
static wxStaticText *g_status = nullptr;
static std::string g_statusText;

static void refresh()
{
    int shown = g_grid->rebuild(S(g_search->GetValue()));
    std::ostringstream os;
    if (g_entries.empty())
        os << "Noch keine Apps vorhanden - mit \"+ Neue App\" die erste Karte "
              "anlegen.";
    else
        os << shown << " von " << g_entries.size() << " Einträgen";
    g_statusText = os.str();
    g_status->SetLabel(W(g_statusText));
}

static void onNew()
{
    Entry e;
    if (!editEntryDialog(e, true))
        return;
    bool created = ensureScript(e);
    g_entries.push_back(e);
    saveEntries();
    refresh();
    if (created)
        editFile(resolve(
            e.script)); // neues Script direkt im Standard-Editor oeffnen
}

class MainWin : public wxFrame
{
  public:
    MainWin()
        : wxFrame(nullptr, wxID_ANY, "App Launcher", wxDefaultPosition,
                  wxSize(940, 640))
    {
            std::error_code ec;
            fs::path icoPath = g_base / "icon.ico";
            if (fs::exists(icoPath, ec))
            {
                wxIcon icon;
                wxLogNull
                    noLog; // keine Fehlerdialoge, falls die Datei kaputt ist
                if (icon.LoadFile(W(toUtf8(icoPath)), wxBITMAP_TYPE_ICO) &&
                    icon.IsOk())
                    SetIcon(icon);
            }

        // --- Menüleiste ---
            enum
            {
                ID_SCRIPTS_DIR = wxID_HIGHEST + 1,
                ID_FOCUS_SEARCH
            };

            auto *mFile = new wxMenu;
            mFile->Append(wxID_NEW, W("&Neue App\tCtrl+N"));
            mFile->Append(ID_SCRIPTS_DIR,
                          wxString(L"&Scripts-Ordner \u00f6ffnen"));
            mFile->AppendSeparator();
            mFile->Append(wxID_EXIT, W("&Beenden\tAlt+F4"));

            auto *mView = new wxMenu;
            mView->Append(ID_FOCUS_SEARCH, W("&Suche fokussieren\tCtrl+F"));
            mView->Append(wxID_REFRESH, W("&Neu laden\tF5"));

            auto *mHelp = new wxMenu;
            mHelp->Append(wxID_ABOUT, W("&Über App Launcher"));

            auto *mb = new wxMenuBar;
            mb->Append(mFile, W("&Datei"));
            mb->Append(mView, W("&Ansicht"));
            mb->Append(mHelp, W("&Hilfe"));
            SetMenuBar(mb);

            Bind(wxEVT_MENU, [](wxCommandEvent &) { onNew(); }, wxID_NEW);
            Bind(
                wxEVT_MENU,
                [](wxCommandEvent &)
                {
                    std::error_code ec;
                    fs::create_directories(g_base / "scripts", ec);
                    wxLaunchDefaultApplication(W(toUtf8(g_base / "scripts")));
                },
                ID_SCRIPTS_DIR);
            Bind(wxEVT_MENU, [this](wxCommandEvent &) { Close(); }, wxID_EXIT);
            Bind(
                wxEVT_MENU, [](wxCommandEvent &) { g_search->SetFocus(); },
                ID_FOCUS_SEARCH);
            Bind(wxEVT_MENU, [](wxCommandEvent &) { refresh(); }, wxID_REFRESH);
            Bind(
                wxEVT_MENU,
                [this](wxCommandEvent &)
                {
                    wxMessageBox(W("App Launcher\nKarten mit Start-Scripts."),
                                 W("Über"), wxOK | wxICON_INFORMATION, this);
                },
                wxID_ABOUT);

        auto *root = new wxPanel(this);
        auto *vbox = new wxBoxSizer(wxVERTICAL);
        auto *top = new wxBoxSizer(wxHORIZONTAL);

        g_search = new wxTextCtrl(root, wxID_ANY);
        auto *addBtn = new wxButton(root, wxID_ANY, "+ Neue App",
                                    wxDefaultPosition, wxSize(130, 28));
        top->Add(new wxStaticText(root, wxID_ANY, "Suche:"), 0,
                 wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);
        top->Add(g_search, 1, wxALIGN_CENTER_VERTICAL | wxRIGHT, 10);
        top->Add(addBtn, 0, wxALIGN_CENTER_VERTICAL);

        g_grid = new Grid(root);
        g_status = new wxStaticText(root, wxID_ANY, "", wxDefaultPosition,
                                    wxDefaultSize, wxST_ELLIPSIZE_END);

        vbox->Add(top, 0, wxEXPAND | wxALL, 10);
        vbox->Add(g_grid, 1, wxEXPAND);
        vbox->Add(g_status, 0, wxEXPAND | wxALL, 4);
        root->SetSizer(vbox);

        g_search->Bind(wxEVT_TEXT, [](wxCommandEvent &) { refresh(); });
        addBtn->Bind(wxEVT_BUTTON, [](wxCommandEvent &) { onNew(); });

        SetMinSize(wxSize(480, 300));
    }
};

class LauncherApp : public wxApp
{
  public:
    bool OnInit() override
    {
        g_base = exeDir();
        wxInitAllImageHandlers();
        wxToolTip::Enable(true);
        wxToolTip::SetDelay(300);
        wxToolTip::SetMaxWidth(420);

        loadEntries();

        auto *win = new MainWin;
        refresh();
        win->Show();
        return true;
    }
};

wxIMPLEMENT_APP(LauncherApp);
