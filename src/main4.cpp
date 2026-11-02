#include <wx/wx.h>

enum
{
    ID_VIEW_HOME = wxID_HIGHEST + 1,
    ID_VIEW_SETTINGS,
    ID_VIEW_ABOUT,
    ID_VIEW_PROJMANAGER
};

// ---------------------------------------------------------------------------
// Content-Panels
// ---------------------------------------------------------------------------
class HomePanel : public wxPanel
{
public:
    HomePanel(wxWindow* parent) : wxPanel(parent)
    {
        wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);

        wxStaticText* title = new wxStaticText(this, wxID_ANY, "Startseite");
        wxFont font = title->GetFont();
        font.SetPointSize(font.GetPointSize() + 6);
        font.MakeBold();
        title->SetFont(font);

        sizer->Add(title, 0, wxALL, 15);
        sizer->Add(new wxStaticText(this, wxID_ANY, "Willkommen in der Anwendung."),
            0, wxLEFT | wxRIGHT, 15);
        sizer->Add(new wxButton(this, wxID_ANY, "Ein Button"), 0, wxALL, 15);

        SetSizer(sizer);
    }
};

class SettingsPanel : public wxPanel
{
public:
    SettingsPanel(wxWindow* parent) : wxPanel(parent)
    {
        wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);

        wxStaticText* title = new wxStaticText(this, wxID_ANY, "Einstellungen");
        wxFont font = title->GetFont();
        font.SetPointSize(font.GetPointSize() + 6);
        font.MakeBold();
        title->SetFont(font);

        sizer->Add(title, 0, wxALL, 15);
        sizer->Add(new wxCheckBox(this, wxID_ANY, "Option A aktivieren"), 0, wxALL, 10);
        sizer->Add(new wxCheckBox(this, wxID_ANY, "Option B aktivieren"), 0, wxALL, 10);

        wxBoxSizer* row = new wxBoxSizer(wxHORIZONTAL);
        row->Add(new wxStaticText(this, wxID_ANY, "Name:"), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);
        row->Add(new wxTextCtrl(this, wxID_ANY), 1);
        sizer->Add(row, 0, wxEXPAND | wxALL, 10);

        SetSizer(sizer);
    }
};

class AboutPanel : public wxPanel
{
public:
    AboutPanel(wxWindow* parent) : wxPanel(parent)
    {
        wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);

        wxStaticText* title = new wxStaticText(this, wxID_ANY, "Über");
        wxFont font = title->GetFont();
        font.SetPointSize(font.GetPointSize() + 6);
        font.MakeBold();
        title->SetFont(font);

        sizer->Add(title, 0, wxALL, 15);
        sizer->Add(new wxStaticText(this, wxID_ANY,
            "wxWidgets Beispiel\nMain Content per Menü wechseln."),
            0, wxLEFT | wxRIGHT, 15);

        SetSizer(sizer);
    }
};

class ProjManagerPanel : public wxPanel
{
    public:
    ProjManagerPanel(wxWindow* parent) : wxPanel(parent)
    {
        wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
        wxStaticText* title = new wxStaticText(this, wxID_ANY, "Projektmanager");
        wxFont font = title->GetFont();
        font.SetPointSize(font.GetPointSize() + 6);
        font.MakeBold();
        title->SetFont(font);
        sizer->Add(title, 0, wxALL, 15);
        sizer->Add(new wxStaticText(this, wxID_ANY,
            "Hier können Sie Ihre Projekte verwalten."),
            0, wxLEFT | wxRIGHT, 15);
        SetSizer(sizer);
	}
};

// ---------------------------------------------------------------------------
// Hauptfenster
// ---------------------------------------------------------------------------
class MainFrame : public wxFrame
{
public:
    MainFrame() : wxFrame(nullptr, wxID_ANY, "wxWidgets Menü-Navigation", wxDefaultPosition, wxSize(600, 400))
    {
        // Menüleiste
        wxMenu* fileMenu = new wxMenu;
        fileMenu->Append(wxID_EXIT, "&Beenden\tAlt+F4");

        wxMenu* viewMenu = new wxMenu;
        viewMenu->AppendRadioItem(ID_VIEW_HOME, "&Startseite\tCtrl+1");
        viewMenu->AppendRadioItem(ID_VIEW_SETTINGS, "&Einstellungen\tCtrl+2");
        viewMenu->AppendRadioItem(ID_VIEW_ABOUT, "&Über\tCtrl+3");
		viewMenu->AppendRadioItem(ID_VIEW_PROJMANAGER, "&Projektmanager\tCtrl+4");

        wxMenuBar* menuBar = new wxMenuBar;
        menuBar->Append(fileMenu, "&Datei");
        menuBar->Append(viewMenu, "&Ansicht");
        SetMenuBar(menuBar);

        CreateStatusBar();

        // Panels erzeugen
        m_home = new HomePanel(this);
        m_settings = new SettingsPanel(this);
        m_about = new AboutPanel(this);
		m_projecmanager = new ProjManagerPanel(this);

        m_sizer = new wxBoxSizer(wxVERTICAL);
        m_sizer->Add(m_home, 1, wxEXPAND);
        m_sizer->Add(m_settings, 1, wxEXPAND);
        m_sizer->Add(m_about, 1, wxEXPAND);
		m_sizer->Add(m_projecmanager, 1, wxEXPAND);
        SetSizer(m_sizer);

        // Events
        Bind(wxEVT_MENU, &MainFrame::OnExit, this, wxID_EXIT);
        Bind(wxEVT_MENU, &MainFrame::OnSwitchView, this, ID_VIEW_HOME);
        Bind(wxEVT_MENU, &MainFrame::OnSwitchView, this, ID_VIEW_SETTINGS);
        Bind(wxEVT_MENU, &MainFrame::OnSwitchView, this, ID_VIEW_ABOUT);
		Bind(wxEVT_MENU, &MainFrame::OnSwitchView, this, ID_VIEW_PROJMANAGER);

        ShowPanel(m_home, "Startseite");
    }

private:
    void ShowPanel(wxPanel* panel, const wxString& name)
    {
        m_home->Hide();
        m_settings->Hide();
        m_about->Hide();
		m_projecmanager->Hide();

        panel->Show();
        Layout();

        SetStatusText("Aktuelle Ansicht: " + name);
    }

    void OnSwitchView(wxCommandEvent& event)
    {
        switch (event.GetId())
        {
        case ID_VIEW_HOME:     ShowPanel(m_home, "Startseite");     break;
        case ID_VIEW_SETTINGS: ShowPanel(m_settings, "Einstellungen"); break;
        case ID_VIEW_ABOUT:    ShowPanel(m_about, "Über");          break;
		case ID_VIEW_PROJMANAGER: ShowPanel(m_projecmanager, "Projektmanager"); break;
        }
    }

    void OnExit(wxCommandEvent&)
    {
        Close(true);
    }

    wxBoxSizer* m_sizer = nullptr;
    HomePanel* m_home = nullptr;
    SettingsPanel* m_settings = nullptr;
    AboutPanel* m_about = nullptr;
	ProjManagerPanel* m_projecmanager = nullptr;
};

// ---------------------------------------------------------------------------
// App
// ---------------------------------------------------------------------------
class MyApp : public wxApp
{
public:
    bool OnInit() override
    {
        MainFrame* frame = new MainFrame;
        frame->Show();
        return true;
    }
};

wxIMPLEMENT_APP(MyApp);