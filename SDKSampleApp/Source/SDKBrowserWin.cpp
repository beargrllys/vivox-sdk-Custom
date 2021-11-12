/* Copyright (c) 2018 by Mercer Road Corp
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
#include "SDKBrowserWin.h"
#include "VxcErrors.h"
#include <assert.h>
#include <vector>

using namespace std;
using namespace vxplatform;

SDKBrowserWin *SDKBrowserWin::s_pInstance = NULL;

#define WM_EXIT_BROWSER (WM_USER + 1)

SDKBrowserWin::SDKBrowserWin() :
    m_lock(NULL),
    // m_windowThreadTerminatedEvent(NULL),
    m_windowThread(NULL),
    m_hWnd(NULL),
    m_hTreeView(NULL)
{
    assert(!s_pInstance);
    s_pInstance = this;
    m_lock = new vxplatform::Lock();
    vxplatform::Locker locker(m_lock);
    // create_event(&m_windowThreadTerminatedEvent);
    create_thread(&SDKBrowserWin::WindowThread, this, &m_windowThread);
}

SDKBrowserWin::~SDKBrowserWin()
{
    Close();
    m_lock->Take();
    if (m_windowThread) {
        // PostMessage()
        vxplatform::os_thread_handle threadHandle = m_windowThread;
        m_lock->Release();
        // Request thread to exit
        if (m_hWnd) {
            if (!SendMessage(m_hWnd, WM_EXIT_BROWSER, 0, 0L)) { // !DestroyWindow(m_hWnd)
                assert(false);
            }
            assert(!m_hWnd);
            assert(!m_hTreeView);
        }
        do {
            m_lock->Take();
            threadHandle = m_windowThread;
            m_lock->Release();
            if (threadHandle) {
                Sleep(100);
            } else {
                break;
            }
        } while (true);
        // join_thread(threadHandle);
        m_lock->Take();
        assert(NULL == m_windowThread);
    }
    m_lock->Release();

    // delete_event(m_windowThreadTerminatedEvent);
}

vxplatform::os_error_t SDKBrowserWin::WindowThread(void *arg)
{
    ((SDKBrowserWin *)arg)->WindowThread();
    return 0;
}

bool SDKBrowserWin::CreateBrowserWindow()
{
    assert(!m_hWnd);
    assert(!m_hTreeView);

    InitCommonControls();

    static const wchar_t s_szClassName[] = L"Vivox.SDKSampleAppToolWindowClass";

    WNDCLASSEX wc;
    HINSTANCE hInstance = GetModuleHandle(NULL);

    // Step 1: Registering the Window Class
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = 0;
    wc.lpfnWndProc = SDKBrowserWndProc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = hInstance;
    wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszMenuName = NULL;
    wc.lpszClassName = s_szClassName;
    wc.hIconSm = LoadIcon(NULL, IDI_APPLICATION);

    if (!RegisterClassEx(&wc)) {
        MessageBox(
                NULL,
                L"Window Registration Failed!",
                L"Error",
                MB_ICONERROR | MB_OK);
        return false;
    }

    m_hWnd = CreateWindowEx(
            /* WS_EX_CLIENTEDGE | */ WS_EX_TOOLWINDOW | WS_EX_TOPMOST,
            s_szClassName,
            L"SDKSampleApp Browser",
            WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            640,
            240,
            NULL,
            NULL,
            hInstance,
            NULL);

    if (m_hWnd != NULL) {
        ShowWindow(m_hWnd, SW_SHOW);
        UpdateWindow(m_hWnd);

        RECT rcClient;
        GetClientRect(m_hWnd, &rcClient);
        m_hTreeView = CreateWindowEx(
                0,
                WC_TREEVIEW,
                TEXT("Tree View"),
                WS_VISIBLE | WS_CHILD | WS_BORDER | TVS_DISABLEDRAGDROP | TVS_HASBUTTONS | TVS_LINESATROOT | TVS_HASLINES | TVS_SHOWSELALWAYS,
                0,
                0,
                rcClient.right,
                rcClient.bottom,
                m_hWnd,
                (HMENU)NULL,
                hInstance,
                NULL
                );
        if (!m_hTreeView) {
            MessageBox(NULL, L"Failed to create TreeView!", L"Error", MB_ICONERROR | MB_OK);
            return false;
        }
    } else {
        MessageBox(NULL, L"Window Creation Failed!", L"Error", MB_ICONERROR | MB_OK);
        return false;
    }

    return true;
}

LRESULT CALLBACK SDKBrowserWin::SDKBrowserWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
        case WM_SIZE:
            assert(hwnd == GetInstance()->m_hWnd);
            if (GetInstance()->m_hTreeView) {
                MoveWindow(GetInstance()->m_hTreeView, 0, 0, LOWORD(lParam), HIWORD(lParam), TRUE);
            }
            break;
        case WM_EXIT_BROWSER:
            assert(hwnd == GetInstance()->m_hWnd);
            DestroyWindow(hwnd);
            assert(GetInstance()->m_hWnd == NULL);
            assert(GetInstance()->m_hTreeView == NULL);
            return 1;
        case WM_CLOSE:
            DestroyWindow(hwnd);
            break;
        case WM_DESTROY:
            PostQuitMessage(0);
            GetInstance()->m_hWnd = NULL;
            GetInstance()->m_hTreeView = NULL;
            break;
        default:
            return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}

void SDKBrowserWin::WindowThread()
{
    assert(GetInstance() == this);
    {
        vxplatform::Locker locker(m_lock);
        // Init
        CreateBrowserWindow(); // TODO: error handling
    }

    MSG Msg;

    while (GetMessage(&Msg, NULL, 0, 0) > 0) {
        TranslateMessage(&Msg);
        DispatchMessage(&Msg);
        if (!m_hWnd) {
            break;
        }
    }

    if (m_hWnd) {
        DestroyWindow(m_hWnd);
        assert(!m_hWnd);
    }

    // job complete
    {
        vxplatform::Locker locker(m_lock);
        close_thread_handle(m_windowThread);
        m_windowThread = NULL;
    }
}

wstring toUnicode(const string &str)
{
    wstring result;

    int size = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, NULL, 0);
    if (0 >= size) {
        return result;
    }
    std::vector<wchar_t> tmp(size + 1);
    LPWSTR tmpBuf = (LPWSTR)tmp.data();
    size = MultiByteToWideChar(CP_UTF8, 0, str.data(), -1, tmpBuf, size);
    if (0 >= size) {
        return result;
    }
    tmp[size] = 0;
    result = tmpBuf;
    return result;
}

HTREEITEM SDKBrowserWin::AddItem(HTREEITEM hParentItem, const string &str, bool bExpandParent)
{
    wstring wtext = toUnicode(str);
    TVINSERTSTRUCT insertItem;
    insertItem.hParent = hParentItem;
    insertItem.hInsertAfter = TVI_LAST;
    insertItem.item.mask = TVIF_TEXT;
    insertItem.item.pszText = (LPWSTR)wtext.c_str();
    HTREEITEM hTreeItem = (HTREEITEM)SendMessage(m_hTreeView, TVM_INSERTITEM, 0, (LPARAM)(LPTVINSERTSTRUCT)&insertItem);
    assert(hTreeItem);
    if (hTreeItem && bExpandParent && (hParentItem != TVI_ROOT)) {
        SendMessage(m_hTreeView, TVM_EXPAND, TVE_EXPAND, (LPARAM)hParentItem);
    }
    return hTreeItem;
}

void SDKBrowserWin::SetItemText(HTREEITEM hItem, const string &str)
{
    wstring wtext = toUnicode(str);
    TVITEM item;
    item.mask = TVIF_HANDLE | TVIF_TEXT;
    item.hItem = hItem;
    item.pszText = (LPWSTR)wtext.c_str();
    SendMessage(m_hTreeView, TVM_SETITEM, 0, (LPARAM)(LPTVITEM)&item);
}

void SDKBrowserWin::OnConnectorCreated(SDKConnector *pConnector)
{
    if (!m_hTreeView) {
        return;
    }
    assert(m_Connectors.find(pConnector) == m_Connectors.end());
    string text = string_format("Connector: %s", pConnector->m_connectorHandle.c_str());
    HTREEITEM hTreeItem = AddItem(TVI_ROOT, text, false);
    if (hTreeItem) {
        m_Connectors[pConnector] = hTreeItem;
    }
}

void SDKBrowserWin::OnConnectorDestroyed(SDKConnector *pConnector)
{
    if (!m_hTreeView) {
        return;
    }
    const auto &it = m_Connectors.find(pConnector);
    assert(it != m_Connectors.end());
    SendMessage(m_hTreeView, TVM_DELETEITEM, 0, (LPARAM)it->second);
    m_Connectors.erase(it);
}

static string format_optional_display_name(const char *displayname)
{
    if (NULL == displayname || !(*displayname)) {
        return "";
    }
    return string_format("\"%s\" ", displayname);
}

string SDKBrowserWin::GetLoginItemText(SDKLogin *pLogin)
{
    return string_format("Login: %shandle=%s, uri=%s [%s]", format_optional_display_name(pLogin->m_displayName.c_str()).c_str(), pLogin->m_accountHandle.c_str(), pLogin->m_uri.c_str(), vx_get_login_state_string(pLogin->m_state));
}

void SDKBrowserWin::OnLoginCreated(SDKLogin *pLogin)
{
    if (!m_hTreeView) {
        return;
    }
    const auto &it = m_Connectors.find(pLogin->m_pConnector);
    assert(it != m_Connectors.end());
    assert(m_Logins.find(pLogin) == m_Logins.end());
    HTREEITEM hTreeItem = AddItem(it->second, GetLoginItemText(pLogin), pLogin->m_pConnector->m_Logins.empty());
    if (hTreeItem) {
        m_Logins[pLogin] = hTreeItem;
    }
}

void SDKBrowserWin::OnLoginUpdated(SDKLogin *pLogin)
{
    if (!m_hTreeView) {
        return;
    }
    const auto &it = m_Logins.find(pLogin);
    assert(it != m_Logins.end());
    SetItemText(it->second, GetLoginItemText(pLogin));
}

void SDKBrowserWin::OnLoginDestroyed(SDKLogin *pLogin)
{
    if (!m_hTreeView) {
        return;
    }
    const auto &it = m_Logins.find(pLogin);
    assert(it != m_Logins.end());
    SendMessage(m_hTreeView, TVM_DELETEITEM, 0, (LPARAM)it->second);
    m_Logins.erase(it);
}

string SDKBrowserWin::GetSessionGroupItemText(SDKSessionGroup *pSessionGroup)
{
    return string_format("SessionGroup: %s", pSessionGroup->m_sessionGroupHandle.c_str());
}

void SDKBrowserWin::OnSessionGroupCreated(SDKSessionGroup *pSessionGroup)
{
    if (!m_hTreeView) {
        return;
    }
    const auto &it = m_Logins.find(pSessionGroup->m_pLogin);
    assert(it != m_Logins.end());
    assert(m_SessionGroups.find(pSessionGroup) == m_SessionGroups.end());
    HTREEITEM hTreeItem =
        AddItem(it->second, GetSessionGroupItemText(pSessionGroup), pSessionGroup->m_pLogin->m_SessionGroups.empty());
    if (hTreeItem) {
        m_SessionGroups[pSessionGroup] = hTreeItem;
    }
}

void SDKBrowserWin::OnSessionGroupDestroyed(SDKSessionGroup *pSessionGroup)
{
    if (!m_hTreeView) {
        return;
    }
    const auto &it = m_SessionGroups.find(pSessionGroup);
    assert(it != m_SessionGroups.end());
    SendMessage(m_hTreeView, TVM_DELETEITEM, 0, (LPARAM)it->second);
    m_SessionGroups.erase(it);
}

string SDKBrowserWin::GetSessionItemText(SDKSession *pSession)
{
    return string_format(
            "Session: %s, audio=%s, text=%s, handle=%s",
            pSession->m_uri.c_str(),
            vx_get_session_media_state_string(pSession->m_mediaState),
            vx_get_session_text_state_string(pSession->m_textState),
            pSession->m_sessionHandle.c_str()
            );
}

void SDKBrowserWin::OnSessionCreated(SDKSession *pSession)
{
    if (!m_hTreeView) {
        return;
    }
    const auto &it = m_SessionGroups.find(pSession->m_pSessionGroup);
    assert(it != m_SessionGroups.end());
    assert(m_Sessions.find(pSession) == m_Sessions.end());
    HTREEITEM hTreeItem = AddItem(it->second, GetSessionItemText(pSession), pSession->m_pSessionGroup->m_Sessions.empty());
    if (hTreeItem) {
        m_Sessions[pSession] = hTreeItem;
    }
}

void SDKBrowserWin::OnSessionUpdated(SDKSession *pSession)
{
    if (!m_hTreeView) {
        return;
    }
    const auto &it = m_Sessions.find(pSession);
    assert(it != m_Sessions.end());
    SetItemText(it->second, GetSessionItemText(pSession));
}

void SDKBrowserWin::OnSessionDestroyed(SDKSession *pSession)
{
    if (!m_hTreeView) {
        return;
    }
    const auto &it = m_Sessions.find(pSession);
    assert(it != m_Sessions.end());
    SendMessage(m_hTreeView, TVM_DELETEITEM, 0, (LPARAM)it->second);
    m_Sessions.erase(it);
}

string SDKBrowserWin::GetParticipantItemText(SDKParticipant *pParticipant)
{
    char *media = "";
    char *mod_muted = "";
    char *muted_for_me = "";
    if (pParticipant->m_activeMedia & (VX_MEDIA_FLAGS_AUDIO | VX_MEDIA_FLAGS_TEXT)) {
        media = "audio and text";
    } else if (pParticipant->m_activeMedia & VX_MEDIA_FLAGS_AUDIO) {
        media = "audio-only";
    } else if (pParticipant->m_activeMedia & VX_MEDIA_FLAGS_TEXT) {
        media = "text-only";
    } else {
        media = "no audio or text";
    }
    mod_muted =
        (pParticipant->m_isModeratorMutedText && pParticipant->m_isModeratorMutedAudio) ?
        " [muted by moderator]" :
        (pParticipant->m_isModeratorMutedText ?
         " [muted by moderator (text)]" :
         (pParticipant->m_isModeratorMutedAudio ?
          " [muted by moderator (audio)]" :
          ""
         )
        );
    muted_for_me =
        (pParticipant->m_isMutedForMeText && pParticipant->m_isMutedForMeAudio) ?
        " [muted for me]" :
        (pParticipant->m_isMutedForMeText ?
         " [muted for me (text)]" :
         (pParticipant->m_isMutedForMeAudio ?
          " [muted for me (audio)]" :
          ""
         )
        );
    return string_format(
            "%s%s%s%s (%s)%s%s%s%s, energy=%g, volume=%d, encoded_uri_with_tag=%s",
            pParticipant->m_isSpeaking ? "[S] " : "[_] ",
            pParticipant->m_isCurrentUser ? "[You] " : "",
            format_optional_display_name(pParticipant->m_displayName.c_str()).c_str(),
            pParticipant->m_uri.c_str(),
            media,
            mod_muted,
            muted_for_me,
            pParticipant->m_isSpeakingWhileMicMuted ? " [!SPK_MUTED!]" : "",
            pParticipant->m_isSpeakingWhileMicVolumeZero ? " [!SPK_MIC_VOL_0!]" : "",
            pParticipant->m_energy,
            pParticipant->m_volume,
            pParticipant->m_encoded_uri_with_tag.c_str()
            );
}

void SDKBrowserWin::OnParticipantCreated(SDKParticipant *pParticipant)
{
    if (!m_hTreeView) {
        return;
    }
    const auto &it = m_Sessions.find(pParticipant->m_pSession);
    assert(it != m_Sessions.end());
    assert(m_Participants.find(pParticipant) == m_Participants.end());
    HTREEITEM hTreeItem = AddItem(it->second, GetParticipantItemText(pParticipant), pParticipant->m_pSession->m_Participants.empty());
    if (hTreeItem) {
        m_Participants[pParticipant] = hTreeItem;
    }
}

void SDKBrowserWin::OnParticipantUpdated(SDKParticipant *pParticipant)
{
    if (!m_hTreeView) {
        return;
    }
    const auto &it = m_Participants.find(pParticipant);
    assert(it != m_Participants.end());
    SetItemText(it->second, GetParticipantItemText(pParticipant));
}

void SDKBrowserWin::OnParticipantDestroyed(SDKParticipant *pParticipant)
{
    if (!m_hTreeView) {
        return;
    }
    const auto &it = m_Participants.find(pParticipant);
    assert(it != m_Participants.end());
    SendMessage(m_hTreeView, TVM_DELETEITEM, 0, (LPARAM)it->second);
    m_Participants.erase(it);
}
