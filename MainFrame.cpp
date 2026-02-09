#include "MainFrame.h"

#include <wx/wx.h>
#include <wx/statline.h>
#include <chrono>
#include <fstream>
#include <vector>
#include <string>
#include <algorithm>
#include <cctype>
#include <thread>
#include <Windows.h>

#include "AudioSessions.h"

#define MENU_EXIT_OPTION_ID 100

static std::string utf8_from_wstring(const std::wstring& w) {
if (w.empty()) return std::string();
int size_needed = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), NULL, 0, NULL, NULL);
std::string strTo(size_needed, 0);
WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), &strTo[0], size_needed, NULL, NULL);
return strTo;
}

static std::string to_lowercase(std::string value) {
std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) { return (char)std::tolower(c); });
return value;
}

static std::vector<std::string> read_whitelist_file() {
std::vector<std::string> wl;
std::ifstream in("whitelist.txt");
if (in.is_open()) {
std::string line;
while (std::getline(in, line)) {
line = to_lowercase(line);
if (!line.empty()) wl.push_back(line);
}
in.close();
}
// remove duplicates
std::sort(wl.begin(), wl.end());
wl.erase(std::unique(wl.begin(), wl.end()), wl.end());
return wl;
}

static void save_whitelist_file(const std::vector<std::string>& wl) {
std::ofstream out("whitelist.txt");
if (out.is_open()) {
for (auto& s : wl) out << s << "\n";
out.close();
}
}

struct WindowChoiceItem { std::string label; std::string exe; };
struct CollectWindowsData { std::vector<WindowChoiceItem>* items; };

static BOOL CALLBACK EnumWindowsCollectProc(HWND hwnd, LPARAM lParam) {
CollectWindowsData* data = (CollectWindowsData*)lParam;
if (!IsWindowVisible(hwnd)) return TRUE;

int length = GetWindowTextLengthW(hwnd);
if (length == 0) return TRUE;

std::wstring wtitle(length + 1, L'\0');
GetWindowTextW(hwnd, &wtitle[0], length + 1);
std::string title = utf8_from_wstring(wtitle);
if (title.empty()) return TRUE;

DWORD pid = 0;
GetWindowThreadProcessId(hwnd, &pid);
if (pid <= 4) return TRUE;

std::wstring wexe = AudioSessions::GetProcessImageNameLower(pid);
std::string exe = AudioSessions::WStringToUtf8(wexe);
exe = to_lowercase(exe);
if (exe.empty()) return TRUE;

WindowChoiceItem item;
item.exe = exe;
item.label = exe + " - " + title;
data->items->push_back(std::move(item));
return TRUE;
}

static std::vector<WindowChoiceItem> get_visible_window_items() {
std::vector<WindowChoiceItem> items;
CollectWindowsData data{ &items };
EnumWindows(EnumWindowsCollectProc, (LPARAM)&data);
std::sort(items.begin(), items.end(), [](const WindowChoiceItem& a, const WindowChoiceItem& b) { return a.label < b.label; });
items.erase(std::unique(items.begin(), items.end(), [](const WindowChoiceItem& a, const WindowChoiceItem& b) { return a.label == b.label; }), items.end());
return items;
}


MainFrame::MainFrame(const wxString& title) : wxFrame(nullptr, wxID_ANY, title) {
task_bar_icon = new TaskBarIcon(this);
Bind(wxEVT_CLOSE_WINDOW, &MainFrame::OnClose, this);

SetIcon(wxICON(icon));

wxFont calibriFont(10, wxFONTFAMILY_SWISS, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL, false, wxT("Arial"));
this->SetFont(calibriFont);

wxPanel* panel = new wxPanel(this);

wxStaticText* info_text = new wxStaticText(panel, wxID_ANY,
"Whitelist processes stay unmuted in background.\n"
"(Whitelist is saved to whitelist.txt as lowercase exe names.)");

window_choice = new wxChoice(panel, wxID_ANY);
add_whitelist_button = new wxButton(panel, wxID_ANY, "Add to whitelist");
add_whitelist_button->Bind(wxEVT_BUTTON, &MainFrame::OnAddWhitelistClicked, this);

whitelist_list = new wxListBox(panel, wxID_ANY, wxDefaultPosition, wxSize(700, 220));

remove_whitelist_button = new wxButton(panel, wxID_ANY, "Remove from whitelist");
remove_whitelist_button->Bind(wxEVT_BUTTON, &MainFrame::OnRemoveWhitelistClicked, this);

autostart_button = new wxButton(panel, wxID_ANY, "Start the application automatically at system startup");
autostart_button->Bind(wxEVT_BUTTON, &MainFrame::autostart_button_clicked, this);

wxStaticLine* horizontal_line = new wxStaticLine(panel, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_HORIZONTAL);

wxBoxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);
mainSizer->AddSpacer(10);
mainSizer->Add(info_text, wxSizerFlags().CenterHorizontal());
mainSizer->AddSpacer(12);
mainSizer->Add(window_choice, wxSizerFlags().CenterHorizontal().Expand());
mainSizer->AddSpacer(5);
mainSizer->Add(add_whitelist_button, wxSizerFlags().CenterHorizontal());
mainSizer->AddSpacer(12);
mainSizer->Add(whitelist_list, wxSizerFlags().CenterHorizontal());
mainSizer->AddSpacer(5);
mainSizer->Add(remove_whitelist_button, wxSizerFlags().CenterHorizontal());
mainSizer->AddSpacer(16);
mainSizer->Add(horizontal_line, 0, wxEXPAND | wxALL, 10);
mainSizer->Add(autostart_button, wxSizerFlags().CenterHorizontal());
mainSizer->AddSpacer(16);

panel->SetSizer(mainSizer);
mainSizer->SetSizeHints(this);

populate_window_choices();
reload_whitelist();
start_monitor_thread();

CreateStatusBar();
}


void MainFrame::populate_window_choices() {
window_choice->Clear();
window_choice_exes.clear();

auto items = get_visible_window_items();
wxArrayString arr;
for (auto& item : items) {
arr.Add(wxString::FromUTF8(item.label.c_str()));
window_choice_exes.push_back(item.exe);
}
window_choice->Append(arr);
}


void MainFrame::reload_whitelist() {
auto entries = read_whitelist_file();
{
std::lock_guard<std::mutex> lock(whitelist_mutex);
whitelist_entries = entries;
}

whitelist_list->Clear();
for (auto& entry : entries) {
whitelist_list->Append(wxString::FromUTF8(entry.c_str()));
}
}


void MainFrame::start_monitor_thread() {
terminate_monitor.store(false);
monitor_thread = std::thread([this]() {
AudioSessions::ComInitGuard com;
if (!com.ok()) return;

std::vector<IAudioSessionManager2*> managers;
if (!AudioSessions::GetAllAudioSessionManagers(managers)) return;

while (!terminate_monitor.load()) {
DWORD fgPid = 0;
HWND fgHwnd = GetForegroundWindow();
if (fgHwnd) {
GetWindowThreadProcessId(fgHwnd, &fgPid);
}

std::string fgExe;
if (fgPid > 4) {
fgExe = AudioSessions::WStringToUtf8(AudioSessions::GetProcessImageNameLower(fgPid));
fgExe = to_lowercase(fgExe);
}

std::vector<std::string> entries;
{
std::lock_guard<std::mutex> lock(whitelist_mutex);
entries = whitelist_entries;
}

AudioSessions::EnumAllAudioSessions(managers, [fgExe, &entries](ISimpleAudioVolume* audioVolume, DWORD processId) {
if (!audioVolume) return;

std::wstring wexe = AudioSessions::GetProcessImageNameLower(processId);
std::string exe = AudioSessions::WStringToUtf8(wexe);
exe = to_lowercase(exe);
if (exe.empty()) return;
if (exe == "audiodg.exe") return;

bool inWhitelist = (std::find(entries.begin(), entries.end(), exe) != entries.end());
bool shouldMute = (fgExe.empty() ? !inWhitelist : (exe != fgExe && !inWhitelist));

BOOL curMuted = FALSE;
audioVolume->GetMute(&curMuted);
bool curMutedBool = (curMuted != FALSE);
if (curMutedBool == shouldMute) return;
audioVolume->SetMute(shouldMute ? TRUE : FALSE, nullptr);
});

std::this_thread::sleep_for(std::chrono::milliseconds(120));
}

AudioSessions::UnmuteAllSessions(managers);
AudioSessions::ReleaseAllAudioSessionManagers(managers);
});
}


void MainFrame::stop_monitor_thread() {
terminate_monitor.store(true);
if (monitor_thread.joinable()) {
monitor_thread.join();
}

AudioSessions::ComInitGuard com;
if (!com.ok()) return;
std::vector<IAudioSessionManager2*> managers;
if (!AudioSessions::GetAllAudioSessionManagers(managers)) return;
AudioSessions::UnmuteAllSessions(managers);
AudioSessions::ReleaseAllAudioSessionManagers(managers);
}


void MainFrame::OnAddWhitelistClicked(wxCommandEvent&) {
int sel = window_choice->GetSelection();
if (sel == wxNOT_FOUND) {
wxLogStatus("Select a window to add to whitelist.");
return;
}
if (sel < 0 || sel >= (int)window_choice_exes.size()) {
wxLogStatus("Please refresh the window list.");
return;
}

std::string exe = to_lowercase(window_choice_exes[sel]);
auto entries = read_whitelist_file();
if (std::find(entries.begin(), entries.end(), exe) == entries.end()) {
entries.push_back(exe);
std::sort(entries.begin(), entries.end());
entries.erase(std::unique(entries.begin(), entries.end()), entries.end());
save_whitelist_file(entries);
reload_whitelist();
wxLogStatus("Added to whitelist.");
} else {
wxLogStatus("Entry already in whitelist.");
}
}


void MainFrame::OnRemoveWhitelistClicked(wxCommandEvent&) {
int sel = whitelist_list->GetSelection();
if (sel == wxNOT_FOUND) {
wxLogStatus("Select a whitelist entry to remove.");
return;
}

wxString selStr = whitelist_list->GetString(sel);
std::string exe = to_lowercase(std::string(selStr.ToUTF8().data()));
auto entries = read_whitelist_file();
auto it = std::find(entries.begin(), entries.end(), exe);
if (it != entries.end()) {
entries.erase(it);
save_whitelist_file(entries);
reload_whitelist();
wxLogStatus("Removed from whitelist.");
} else {
wxLogStatus("Entry not found in whitelist file.");
}
}


void MainFrame::autostart_button_clicked(wxCommandEvent&) {
system("add_to_startup.bat");
}


void MainFrame::OnClose(wxCloseEvent& event) {
if (allow_close) {
event.Skip();
return;
}
Hide();
event.Veto();
}


void MainFrame::OnMenuEvent(wxCommandEvent& event) {
if (event.GetId() == MENU_EXIT_OPTION_ID) {
allow_close = true;
Close(true);
}
}


MainFrame::~MainFrame() {
stop_monitor_thread();

if (task_bar_icon) {
task_bar_icon->RemoveIcon();
delete task_bar_icon;
}
}


TaskBarIcon::TaskBarIcon(MainFrame* parentFrame) : wxTaskBarIcon(), main_frame(parentFrame) {
SetIcon(wxICON(icon));
Bind(wxEVT_TASKBAR_LEFT_DOWN, &TaskBarIcon::left_button_click, this);
Bind(wxEVT_TASKBAR_RIGHT_DOWN, &TaskBarIcon::right_button_click, this);
}

void TaskBarIcon::left_button_click(wxTaskBarIconEvent&) {
if (main_frame->IsIconized()) {
main_frame->Restore();
}
main_frame->Show();
}

void TaskBarIcon::right_button_click(wxTaskBarIconEvent&) {
wxMenu menu;
menu.Append(MENU_EXIT_OPTION_ID, "Exit");
Bind(wxEVT_COMMAND_MENU_SELECTED, &MainFrame::OnMenuEvent, main_frame, MENU_EXIT_OPTION_ID);
PopupMenu(&menu);
}
