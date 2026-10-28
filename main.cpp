#include <windows.h>
#include <commdlg.h>
#include <cstdio>
#include <string>

constexpr int NEW = 1, OPEN = 2, SAVE = 3, EXIT = 4, EDITOR = 5;
HWND editBox;
HFONT font;
wchar_t filePath[MAX_PATH]{};
bool changed = false, loading = false;

void SetTitle(HWND window)
{
    std::wstring title = L"Simple Notes - ";
    title += *filePath ? filePath : L"Untitled";
    if (changed) title += L" *";
    SetWindowTextW(window, title.c_str());
}

bool ChooseFile(HWND window, bool save, wchar_t* path)
{
    wchar_t name[MAX_PATH]{};
    if (save && *filePath) wcscpy_s(name, filePath);
    OPENFILENAMEW dialog{sizeof(dialog)};
    dialog.hwndOwner = window;
    dialog.lpstrFilter = L"Text files (*.txt)\0*.txt\0All files (*.*)\0*.*\0\0";
    dialog.lpstrFile = name;
    dialog.nMaxFile = MAX_PATH;
    dialog.lpstrDefExt = L"txt";
    dialog.Flags = OFN_EXPLORER | OFN_PATHMUSTEXIST |
        (save ? OFN_OVERWRITEPROMPT : OFN_FILEMUSTEXIST);
    if (!(save ? GetSaveFileNameW(&dialog) : GetOpenFileNameW(&dialog))) return false;
    wcscpy_s(path, MAX_PATH, name);
    return true;
}

bool SaveFile(HWND window)
{
    wchar_t path[MAX_PATH]{};
    wcscpy_s(path, filePath);
    if (!*path && !ChooseFile(window, true, path)) return false;
    FILE* file = _wfopen(path, L"wt, ccs=UTF-8");
    if (!file) return false;

    int length = GetWindowTextLengthW(editBox);
    std::wstring text(length + 1, L'\0');
    GetWindowTextW(editBox, text.data(), length + 1);
    text.resize(length);
    bool ok = text.empty() || fputws(text.c_str(), file) >= 0;
    if (fclose(file)) ok = false;
    if (!ok) MessageBoxW(window, L"Could not save the file.", L"Simple Notes", MB_OK | MB_ICONERROR);
    else { wcscpy_s(filePath, path); changed = false; SetTitle(window); }
    return ok;
}

bool SaveBeforeLeaving(HWND window)
{
    if (!changed) return true;
    int answer = MessageBoxW(window, L"Save your changes?", L"Simple Notes",
        MB_YESNOCANCEL | MB_ICONWARNING);
    return answer == IDYES ? SaveFile(window) : answer == IDNO;
}

void NewFile(HWND window)
{
    loading = true;
    SetWindowTextW(editBox, L"");
    loading = changed = false;
    filePath[0] = L'\0';
    SetTitle(window);
}

void OpenFile(HWND window)
{
    wchar_t path[MAX_PATH]{};
    if (!ChooseFile(window, false, path)) return;
    FILE* file = _wfopen(path, L"rt, ccs=UTF-8");
    if (!file)
    {
        MessageBoxW(window, L"Could not open the file.", L"Simple Notes", MB_OK | MB_ICONERROR);
        return;
    }

    std::wstring text;
    wchar_t buffer[4096];
    while (fgetws(buffer, 4096, file)) text += buffer;
    bool ok = !ferror(file);
    fclose(file);
    if (!ok)
    {
        MessageBoxW(window, L"Could not read the file.", L"Simple Notes", MB_OK | MB_ICONERROR);
        return;
    }
    loading = true;
    SetWindowTextW(editBox, text.c_str());
    loading = changed = false;
    wcscpy_s(filePath, path);
    SetTitle(window);
}

LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (message == WM_SIZE && editBox)
        MoveWindow(editBox, 0, 0, LOWORD(lParam), HIWORD(lParam), TRUE);

    if (message == WM_COMMAND)
    {
        int id = LOWORD(wParam);
        if (id == EDITOR && HIWORD(wParam) == EN_CHANGE && !loading)
        { changed = true; SetTitle(window); }
        else if (id == NEW && SaveBeforeLeaving(window)) NewFile(window);
        else if (id == OPEN && SaveBeforeLeaving(window)) OpenFile(window);
        else if (id == SAVE) SaveFile(window);
        else if (id == EXIT) SendMessageW(window, WM_CLOSE, 0, 0);
        return 0;
    }
    if (message == WM_CLOSE)
    { if (SaveBeforeLeaving(window)) DestroyWindow(window); return 0; }
    if (message == WM_DESTROY)
    { DeleteObject(font); PostQuitMessage(0); return 0; }
    return DefWindowProcW(window, message, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE instance, HINSTANCE, LPSTR, int show)
{
    constexpr wchar_t CLASS_NAME[] = L"SimpleNotesWindow";
    WNDCLASSW wc{};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = instance;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    if (!RegisterClassW(&wc)) return 1;

    HWND window = CreateWindowW(CLASS_NAME, L"Simple Notes - Untitled", WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 800, 600, nullptr, nullptr, instance, nullptr);
    if (!window) return 1;

    HMENU file = CreateMenu(), bar = CreateMenu();
    AppendMenuW(file, MF_STRING, NEW, L"&New");
    AppendMenuW(file, MF_STRING, OPEN, L"&Open...");
    AppendMenuW(file, MF_STRING, SAVE, L"&Save");
    AppendMenuW(file, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(file, MF_STRING, EXIT, L"E&xit");
    AppendMenuW(bar, MF_POPUP, reinterpret_cast<UINT_PTR>(file), L"&File");
    SetMenu(window, bar);

    editBox = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE |
        ES_AUTOVSCROLL, 0, 0, 0, 0, window,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(EDITOR)), instance, nullptr);
    if (!editBox) { DestroyWindow(window); return 1; }

    font = CreateFontW(-18, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN, L"Consolas");
    SendMessageW(editBox, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
    RECT area{};
    GetClientRect(window, &area);
    MoveWindow(editBox, 0, 0, area.right, area.bottom, TRUE);
    SetTitle(window);
    ShowWindow(window, show);

    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0)
    { TranslateMessage(&msg); DispatchMessageW(&msg); }
    return 0;
}
