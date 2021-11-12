#pragma once
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
#include "SDKBrowser.h"
#include "vxplatform/vxcplatform.h"
#include <windows.h>
#include <commctrl.h>

using namespace std;

class SDKBrowserWin :
    public SDKBrowser
{
public:
    SDKBrowserWin();
    virtual ~SDKBrowserWin();
    static SDKBrowserWin *GetInstance()
    {
        return s_pInstance;
    }

    virtual void OnConnectorCreated(SDKConnector *);
    virtual void OnConnectorDestroyed(SDKConnector *);

    virtual void OnLoginCreated(SDKLogin *);
    virtual void OnLoginUpdated(SDKLogin *);
    virtual void OnLoginDestroyed(SDKLogin *);

    virtual void OnSessionGroupCreated(SDKSessionGroup *);
    virtual void OnSessionGroupDestroyed(SDKSessionGroup *);

    virtual void OnSessionCreated(SDKSession *);
    virtual void OnSessionUpdated(SDKSession *);
    virtual void OnSessionDestroyed(SDKSession *);

    virtual void OnParticipantCreated(SDKParticipant *);
    virtual void OnParticipantUpdated(SDKParticipant *);
    virtual void OnParticipantDestroyed(SDKParticipant *);

protected:
    static SDKBrowserWin *s_pInstance;
    vxplatform::Lock *m_lock;
    HWND m_hWnd;
    HWND m_hTreeView;

    static vxplatform::os_error_t WindowThread(void *arg);
    void WindowThread();

    bool CreateBrowserWindow();
    static LRESULT CALLBACK SDKBrowserWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    // vxplatform::os_event_handle  m_windowThreadTerminatedEvent;
    vxplatform::os_thread_handle m_windowThread;

    map<SDKConnector *, HTREEITEM> m_Connectors;
    map<SDKLogin *, HTREEITEM> m_Logins;
    map<SDKSessionGroup *, HTREEITEM> m_SessionGroups;
    map<SDKSession *, HTREEITEM> m_Sessions;
    map<SDKParticipant *, HTREEITEM> m_Participants;

    HTREEITEM AddItem(HTREEITEM hParentItem, const string &str, bool bExpandParent);
    void SetItemText(HTREEITEM hItem, const string &str);
    string GetLoginItemText(SDKLogin *pLogin);
    string GetSessionGroupItemText(SDKSessionGroup *pSessionGroup);
    string GetSessionItemText(SDKSession *pSession);
    string GetParticipantItemText(SDKParticipant *pParticipant);
};
