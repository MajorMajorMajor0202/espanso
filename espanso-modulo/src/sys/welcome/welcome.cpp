/*
 * This file is part of modulo.
 *
 * Copyright (C) 2020-2021 Federico Terzi
 *
 * modulo is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * modulo is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with modulo.  If not, see <https://www.gnu.org/licenses/>.
 */

#define _UNICODE

#include "../common/common.h"
#include "../interop/interop.h"
#include "./welcome_gui.h"
#include <wchar.h>

#include <memory>
#include <unordered_map>
#include <vector>

WelcomeMetadata *welcome_metadata = nullptr;

// App Code

class WelcomeApp : public wxApp {
  public:
    virtual bool OnInit();
};

class DerivedWelcomeFrame : public WelcomeFrame {
  protected:
    void on_dont_show_change(wxCommandEvent &event);
    void on_complete(wxCommandEvent &event);

    // React to WM_SETTINGCHANGE to re-theme live (wx 3.1.5 lacks dark-mode events).
    WXLRESULT MSWWindowProc(WXUINT message, WXWPARAM wParam, WXLPARAM lParam);
    void ApplyTheme();

  public:
    DerivedWelcomeFrame(wxWindow *parent);
};

DerivedWelcomeFrame::DerivedWelcomeFrame(wxWindow *parent)
    : WelcomeFrame(parent) {
    // Welcome images

    if (welcome_metadata->tray_image_path) {
        wxBitmap trayBitmap =
            wxBitmap(wxString::FromUTF8(welcome_metadata->tray_image_path),
                     wxBITMAP_TYPE_PNG);
        this->tray_bitmap->SetBitmap(trayBitmap);
#ifdef __WXOSX__
        this->tray_info_label->SetLabel(
            "You should see the espanso icon on the status bar:");
#endif
    } else {
        this->tray_info_label->Hide();
    }

    this->dont_show_checkbox->Hide();

    if (welcome_metadata->already_running) {
        this->title_label->SetLabel("Espanso is already running!");
    }

#ifdef __WXMSW__
    // Apply dark mode up-front; live changes handled in MSWWindowProc.
    applyDarkModeToWindow(GetHandle(), isSystemDark());
    applyThemeColors(this, isSystemDark());
#endif
}

void DerivedWelcomeFrame::ApplyTheme() {
#ifdef __WXMSW__
    bool dark = isSystemDark();
    applyDarkModeToWindow(GetHandle(), dark);
    applyThemeColors(this, dark);
    this->Refresh();
#endif
}

WXLRESULT DerivedWelcomeFrame::MSWWindowProc(WXUINT message, WXWPARAM wParam,
                                             WXLPARAM lParam) {
#ifdef __WXMSW__
    // Windows broadcasts WM_SETTINGCHANGE ("ImmersiveColorSet") on theme
    // changes, including PowerToys Light Switch scheduled switches.
    if (message == WM_SETTINGCHANGE && lParam != 0 &&
        wcscmp((LPCWSTR)lParam, L"ImmersiveColorSet") == 0) {
        ApplyTheme();
    }
#endif
    return WelcomeFrame::MSWWindowProc(message, wParam, lParam);
}

void DerivedWelcomeFrame::on_dont_show_change(wxCommandEvent &event) {
    if (welcome_metadata->dont_show_again_changed) {
        int value = this->dont_show_checkbox->IsChecked() ? 1 : 0;
        welcome_metadata->dont_show_again_changed(value);
    }
}

void DerivedWelcomeFrame::on_complete(wxCommandEvent &event) { Close(true); }

bool WelcomeApp::OnInit() {
#ifdef __WXMSW__
    // Opt into Windows immersive dark mode before any window is created.
    enableAppDarkMode();
#endif
    wxInitAllImageHandlers();
    DerivedWelcomeFrame *frame = new DerivedWelcomeFrame(NULL);

    if (welcome_metadata->window_icon_path) {
        setFrameIcon(wxString::FromUTF8(welcome_metadata->window_icon_path),
                     frame);
    }

    frame->Show(true);

    Activate(frame);

    return true;
}

extern "C" void interop_show_welcome(WelcomeMetadata *_metadata) {
// Setup high DPI support on Windows
#ifdef __WXMSW__
    SetProcessDPIAware();
#endif

    welcome_metadata = _metadata;

    wxApp::SetInstance(new WelcomeApp());
    int argc = 0;
    wxEntry(argc, (char **)nullptr);
}