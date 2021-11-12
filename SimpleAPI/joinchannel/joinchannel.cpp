/* Copyright (c) 2014-2018 by Mercer Road Corp
*
* Permission to use, copy, modify or distribute this software in binary or source form
* for any purpose is allowed only under explicit prior consent in writing from Mercer Road Corp
*
* THE SOFTWARE IS PROVIDED "AS IS" AND MERCER ROAD CORP DISCLAIMS
* ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
* WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL MERCER ROAD CORP
* BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
* DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
* PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
* ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
* SOFTWARE.
*/

#include "joinchannel.h"
#include "joinchannelapp.h"
#include <sstream>

#define MAX_LOADSTRING 100

// Global Variables:
HINSTANCE hInst;                                // current instance
TCHAR szTitle[MAX_LOADSTRING];                  // The title bar text
TCHAR szWindowClass[MAX_LOADSTRING];            // the main window class name
HWND hwndMainDialog;
HWND hwndAudioSetup;

// BEGINVIVOX: It's helpful to have a global instance of your application object
JoinChannelApp *vivoxJoinChannelApp = NULL;
// ENDVIVOX

// Forward declarations of functions included in this code module:
BOOL InitInstance(HINSTANCE, int);
INT_PTR CALLBACK MainDialog(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK AudioDialog(HWND, UINT, WPARAM, LPARAM);

int APIENTRY _tWinMain(
        _In_ HINSTANCE hInstance,
        _In_opt_ HINSTANCE hPrevInstance,
        _In_ LPTSTR lpCmdLine,
        _In_ int nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    MSG msg;
    HACCEL hAccelTable;

    // Initialize global strings
    LoadString(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadString(hInstance, IDC_JOINCHANNEL, szWindowClass, MAX_LOADSTRING);

    // Perform application initialization:
    if (!InitInstance(hInstance, nCmdShow)) {
        return FALSE;
    }

    hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_JOINCHANNEL));

    // BEGINVIVOX: Create your global app object
    vivoxJoinChannelApp = new JoinChannelApp(hInstance, hwndMainDialog, hwndAudioSetup);
    VCSStatus vcs;
#ifdef _DEBUG
    vcs = vivoxJoinChannelApp->Initialize(IClientApiEventHandler::LogLevelWarning);
#else
    vcs = vivoxJoinChannelApp->Initialize(IClientApiEventHandler::LogLevelWarning);
#endif
    if (vcs != 0) {
        MessageBox(NULL, VivoxClientApi::GetErrorString(vcs), "Initialize() Error", MB_ICONERROR | MB_OK);
        return -1;
    }


    vcs = vivoxJoinChannelApp->Connect("https://vdx5.www.vivox.com/api2/");
    if (vcs != 0) {
        MessageBox(NULL, VivoxClientApi::GetErrorString(vcs), "Connect() Error", MB_ICONERROR | MB_OK);
        return -1;
    }
    // ENDVIVOX

    // Main message loop:
    while (GetMessage(&msg, NULL, 0, 0)) {
        if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    // BEGINVIVOX: Cleanup
    vivoxJoinChannelApp->Uninitialize();
    delete vivoxJoinChannelApp;
    vivoxJoinChannelApp = NULL;
    // ENDVIVOX
    return (int) msg.wParam;
}

//
//   FUNCTION: InitInstance(HINSTANCE, int)
//
//   PURPOSE: Saves instance handle and creates main window
//
//   COMMENTS:
//
//        In this function, we save the instance handle in a global variable and
//        create and display the main program window.
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
    hInst = hInstance; // Store instance handle in our global variable

    hwndMainDialog = CreateDialog(hInst, MAKEINTRESOURCE(IDD_MAINDIALOG), NULL, MainDialog);
    hwndAudioSetup = CreateDialog(hInst, MAKEINTRESOURCE(IDD_AUDIO_SETUP), NULL, AudioDialog);

    ShowWindow(hwndMainDialog, nCmdShow);

    return TRUE;
}

INT_PTR CALLBACK MainDialog(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (message == WM_INITDIALOG) {
        RECT rc;

        hwndMainDialog = hDlg;
        GetWindowRect(hDlg, &rc);

        int xPos = (GetSystemMetrics(SM_CXSCREEN) - rc.right) / 2;
        int yPos = (GetSystemMetrics(SM_CYSCREEN) - rc.bottom) / 2;

        SetWindowPos(hDlg, 0, xPos, yPos, 0, 0, SWP_NOZORDER | SWP_NOSIZE);

        SetMenu(hDlg, LoadMenu(hInst, MAKEINTRESOURCE(IDC_JOINCHANNEL)));
    } else if (vivoxJoinChannelApp != NULL) {
        return vivoxJoinChannelApp->MainDialog(hDlg, message, wParam, lParam);
    }
    return FALSE;
}

INT_PTR CALLBACK AudioDialog(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (message == WM_INITDIALOG) {
        RECT rc;
        GetWindowRect(hDlg, &rc);

        int xPos = (GetSystemMetrics(SM_CXSCREEN) - rc.right) / 2;
        int yPos = (GetSystemMetrics(SM_CYSCREEN) - rc.bottom) / 2;

        SetWindowPos(hDlg, 0, xPos, yPos, 0, 0, SWP_NOZORDER | SWP_NOSIZE);
    } else if (vivoxJoinChannelApp != NULL) {
        return vivoxJoinChannelApp->AudioDialog(hDlg, message, wParam, lParam);
    }
    return (INT_PTR)FALSE;
}
