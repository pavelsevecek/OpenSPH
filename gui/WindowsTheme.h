#pragma once

#ifdef __WXMSW__

#include <wx/msw/wrapwin.h>
#include <wx/settings.h>
#include <wx/utils.h>
#include <delayimp.h>

NAMESPACE_SPH_BEGIN
namespace DarkTheme {

using OpenScrollTheme = HANDLE(WINAPI*)(HWND, LPCWSTR);

inline OpenScrollTheme& originalScrollTheme() {
    static OpenScrollTheme open = nullptr;
    return open;
}

inline HANDLE WINAPI openDarkScrollTheme(HWND window, LPCWSTR classes) {
    // Common controls open non-client scrollbars without consulting their
    // window theme. Request the dark Explorer scrollbar resource explicitly.
    if (classes && lstrcmpW(classes, L"ScrollBar") == 0) {
        return originalScrollTheme()(nullptr, L"Explorer::ScrollBar");
    }
    return originalScrollTheme()(window, classes);
}

inline void enableDarkScrollbars(HMODULE themeModule) {
    originalScrollTheme() = reinterpret_cast<OpenScrollTheme>(
        GetProcAddress(themeModule, MAKEINTRESOURCEA(49)));
    HMODULE controls = LoadLibraryExW(L"comctl32.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!originalScrollTheme() || !controls) {
        return;
    }
    // Redirect only comctl32's delay import of OpenNcThemeData, in this process.
    // If the OS changes this import layout, leave its native implementation alone.
    auto* base = reinterpret_cast<unsigned char*>(controls);
    auto* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) {
        return;
    }
    auto* pe = reinterpret_cast<IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
    if (pe->Signature != IMAGE_NT_SIGNATURE) {
        return;
    }
    const auto& directory = pe->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_DELAY_IMPORT];
    if (!directory.VirtualAddress || directory.Size < sizeof(ImgDelayDescr)) {
        return;
    }
    auto* imports = reinterpret_cast<ImgDelayDescr*>(base + directory.VirtualAddress);
    for (Size i = 0; i < directory.Size / sizeof(ImgDelayDescr) && imports[i].rvaDLLName; ++i) {
        const auto& entry = imports[i];
        if (!(entry.grAttrs & dlattrRva) || !entry.rvaINT || !entry.rvaIAT ||
            lstrcmpiA(reinterpret_cast<const char*>(base + entry.rvaDLLName), "uxtheme.dll") != 0) {
            continue;
        }
        auto* names = reinterpret_cast<IMAGE_THUNK_DATA*>(base + entry.rvaINT);
        auto* addresses = reinterpret_cast<IMAGE_THUNK_DATA*>(base + entry.rvaIAT);
        for (Size j = 0; names[j].u1.Ordinal; ++j) {
            if (!IMAGE_SNAP_BY_ORDINAL(names[j].u1.Ordinal) ||
                IMAGE_ORDINAL(names[j].u1.Ordinal) != 49) {
                continue;
            }
            DWORD protection;
            if (VirtualProtect(&addresses[j], sizeof(addresses[j]), PAGE_READWRITE, &protection)) {
                addresses[j].u1.Function = reinterpret_cast<ULONG_PTR>(&openDarkScrollTheme);
                DWORD ignored;
                VirtualProtect(&addresses[j], sizeof(addresses[j]), protection, &ignored);
            }
            return;
        }
    }
}

// wxWidgets 3.2 predates native dark mode. Resolve the Windows 10 dark-mode
// entry points dynamically, and only on versions with the expected signatures.
struct WindowsTheme {
    using AllowWindow = bool(WINAPI*)(HWND, bool);
    using AppMode = DWORD(WINAPI*)(DWORD);
    using WindowTheme = HRESULT(WINAPI*)(HWND, LPCWSTR, LPCWSTR);
    using WindowAttribute = HRESULT(WINAPI*)(HWND, DWORD, LPCVOID, DWORD);
    AllowWindow allowWindow = nullptr;
    WindowTheme windowTheme = nullptr;
    WindowAttribute windowAttribute = nullptr;

    WindowsTheme() {
        if (!wxCheckOsVersion(10, 0, 19041)) {
            return;
        }
        HMODULE compositor = LoadLibraryExW(L"dwmapi.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
        if (compositor) {
            windowAttribute = reinterpret_cast<WindowAttribute>(
                GetProcAddress(compositor, "DwmSetWindowAttribute"));
        }
        // Keep this module loaded for the lifetime of these function pointers.
        HMODULE module = LoadLibraryExW(L"uxtheme.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
        if (!module) {
            return;
        }
        auto appMode = reinterpret_cast<AppMode>(GetProcAddress(module, MAKEINTRESOURCEA(135)));
        allowWindow = reinterpret_cast<AllowWindow>(GetProcAddress(module, MAKEINTRESOURCEA(133)));
        windowTheme = reinterpret_cast<WindowTheme>(GetProcAddress(module, "SetWindowTheme"));
        if (appMode && allowWindow && windowTheme) {
            appMode(2); // ForceDark, including popup menus when Windows is light.
            using FlushMenus = void(WINAPI*)();
            auto flushMenus = reinterpret_cast<FlushMenus>(GetProcAddress(module, MAKEINTRESOURCEA(136)));
            if (flushMenus) {
                flushMenus();
            }
            enableDarkScrollbars(module);
        }
    }
};

inline WindowsTheme& windowsTheme() {
    static WindowsTheme theme;
    return theme;
}

inline void applyNative(wxWindow* window) {
    auto& theme = windowsTheme();
    HWND handle = static_cast<HWND>(window->GetHandle());
    if (!handle || !theme.allowWindow || !theme.windowTheme) {
        return;
    }
    // SetWindowTheme sends WM_THEMECHANGED; don't repeat it on every Show().
    if (GetPropW(handle, L"OpenSPH.DarkTheme")) {
        return;
    }
    SetPropW(handle, L"OpenSPH.DarkTheme", reinterpret_cast<HANDLE>(1));
    if (window->IsTopLevel() && theme.windowAttribute) {
        // DWMWA_USE_IMMERSIVE_DARK_MODE: let Windows draw the dark caption
        // and its native minimize, maximize and close buttons.
        const BOOL dark = TRUE;
        theme.windowAttribute(handle, 20, &dark, sizeof(dark));
    }
    theme.allowWindow(handle, true);
    const bool combo = dynamic_cast<wxComboBox*>(window) != nullptr;
    theme.windowTheme(handle, combo ? L"DarkMode_CFD" : L"DarkMode_Explorer", nullptr);
    if (combo) {
        COMBOBOXINFO info = {};
        info.cbSize = sizeof(info);
        if (GetComboBoxInfo(handle, &info)) {
            theme.allowWindow(info.hwndList, true);
            theme.windowTheme(info.hwndList, L"DarkMode_Explorer", nullptr);
        }
    }
}

// Windows uses these UAH messages to draw a themed native menu bar. Keep its
// normal menu navigation, accelerators and accessibility; replace only paint.
inline bool drawMenuBar(HWND window, UINT message, LPARAM parameter) {
    struct MenuPaint {
        HMENU menu;
        HDC dc;
        DWORD flags;
    };
    struct ItemPaint {
        DRAWITEMSTRUCT item;
        MenuPaint menu;
        int position;
    };
    if (message != 0x0091 && message != 0x0092) {
        return false;
    }
    if (!parameter) {
        return false;
    }
    HDC dc;
    RECT rect;
    wxColour fill = background();
    ItemPaint* item = nullptr;
    if (message == 0x0091) { // WM_UAHDRAWMENU
        auto* paint = reinterpret_cast<MenuPaint*>(parameter);
        MENUBARINFO info = {};
        info.cbSize = sizeof(info);
        RECT bounds;
        if (!GetMenuBarInfo(window, OBJID_MENU, 0, &info) || !GetWindowRect(window, &bounds)) {
            return false;
        }
        rect = info.rcBar;
        OffsetRect(&rect, -bounds.left, -bounds.top);
        dc = paint->dc;
    } else { // WM_UAHDRAWMENUITEM
        item = reinterpret_cast<ItemPaint*>(parameter);
        dc = item->item.hDC;
        rect = item->item.rcItem;
        if (item->item.itemState & (ODS_HOTLIGHT | ODS_SELECTED)) {
            fill = selection();
        }
    }
    HBRUSH brush = CreateSolidBrush(RGB(fill.Red(), fill.Green(), fill.Blue()));
    FillRect(dc, &rect, brush);
    DeleteObject(brush);
    if (item) {
        wchar_t label[256] = {};
        GetMenuStringW(item->menu.menu, item->position, label, 256, MF_BYPOSITION);
        const int saved = SaveDC(dc);
        wxFont font = wxSystemSettings::GetFont(wxSYS_DEFAULT_GUI_FONT);
        SelectObject(dc, static_cast<HFONT>(font.GetResourceHandle()));
        SetBkMode(dc, TRANSPARENT);
        const wxColour foreground = (item->item.itemState & (ODS_DISABLED | ODS_GRAYED))
                                        ? wxColour(145, 149, 157)
                                        : text();
        SetTextColor(dc, RGB(foreground.Red(), foreground.Green(), foreground.Blue()));
        UINT flags = DT_CENTER | DT_VCENTER | DT_SINGLELINE;
        if (item->item.itemState & ODS_NOACCEL) {
            flags |= DT_HIDEPREFIX;
        }
        DrawTextW(dc, label, -1, &rect, flags);
        RestoreDC(dc, saved);
    }
    return true;
}

} // namespace DarkTheme
NAMESPACE_SPH_END

#endif
