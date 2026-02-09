#pragma once

#include <wx/wx.h>
#include <thread>
#include <atomic>
#include <mutex>
#include <vector>
#include <string>
#include <wx/taskbar.h>
#include <wx/menu.h>

class TaskBarIcon;

class MainFrame : public wxFrame {
public:
MainFrame(const wxString& title);
void OnMenuEvent(wxCommandEvent& event);

private:
wxButton* autostart_button = nullptr;
wxChoice* window_choice = nullptr;
wxButton* add_whitelist_button = nullptr;
wxListBox* whitelist_list = nullptr;
wxButton* remove_whitelist_button = nullptr;
TaskBarIcon* task_bar_icon = nullptr;

std::thread monitor_thread;
std::atomic<bool> terminate_monitor{ false };
std::mutex whitelist_mutex;
std::vector<std::string> whitelist_entries; // exe names, lowercase, e.g. "chrome.exe"
std::vector<std::string> window_choice_exes; // matches window_choice items
bool allow_close = false;

void OnAddWhitelistClicked(wxCommandEvent& event);
void OnRemoveWhitelistClicked(wxCommandEvent& event);
void populate_window_choices();
void reload_whitelist();
void autostart_button_clicked(wxCommandEvent& event);
void start_monitor_thread();
void stop_monitor_thread();
void OnClose(wxCloseEvent& event);
~MainFrame();
};


class TaskBarIcon : public wxTaskBarIcon {
public:
TaskBarIcon(MainFrame* parentFrame);

private:
MainFrame* main_frame;
void left_button_click(wxTaskBarIconEvent&);
void right_button_click(wxTaskBarIconEvent&);
};
