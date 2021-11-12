#pragma once
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

#include "targetver.h"

// Windows Header Files:
#include <windows.h>
#include <windowsx.h>
#include <CommCtrl.h>

// C RunTime Header Files
#include <stdlib.h>
#include <malloc.h>
#include <memory.h>
#include <tchar.h>

#include "vivoxclientapi/windowsinvokeonuithread.h"
#include "vivoxclientapi/debugclientapieventhandler.h"
#include "vivoxclientapi/clientconnection.h"

using namespace VivoxClientApi;

class JoinChannelApp :
    public VivoxClientApi::WindowsInvokeOnUIThread<VivoxClientApi::DebugClientApiEventHandler>
{
    typedef VivoxClientApi::WindowsInvokeOnUIThread<VivoxClientApi::DebugClientApiEventHandler> base;

public:
    JoinChannelApp(HINSTANCE hInst, HWND hwndMainDialog, HWND hwndAudioSetup);
    ~JoinChannelApp();

    // Application Methods
    VCSStatus Initialize(IClientApiEventHandler::LogLevel logLevel) { return m_vivox.Initialize(this, logLevel, false, false); }
    void Uninitialize() { m_vivox.Uninitialize(); }
    VCSStatus Connect(const char *strBackend);
    VCSStatus Update();
    void MuteForMe(int controlId);
    void MuteForAll(int controlId);
    void SetPositionIn3dChannel();

    VivoxClientApi::ClientConnection &GetClientConnection() { return m_vivox; }

    // DebugClientApiEventHandler overrides
    virtual void WriteStatus(const char *msg) const;

    // IClientApiEventHandler overrides
    virtual void onLoginCompleted(const AccountName &accountName);
    virtual void onLogoutCompleted(const AccountName &accountName);
    virtual void onChannelJoined(const AccountName &accountName, const Uri &channelUri);
    virtual void onChannelExited(const AccountName &accountName, const Uri &channelUri, VCSStatus reasonCode);
    virtual void onParticipantAdded(const AccountName &accountName, const Uri &channelUri, const Uri &participantUri, bool isLoggedInUser);
    virtual void onParticipantLeft(const AccountName &accountName, const Uri &channelUri, const Uri &participantUri, bool isLoggedInUser, ParticipantLeftReason reason);
    virtual void onParticipantUpdated(const AccountName &accountName, const Uri &channelUri, const Uri &participantUri, bool isLoggedInUser, bool speaking, double vuMeterEnergy, bool isMutedForAll);

    virtual void onAvailableAudioDevicesChanged();

    virtual void onDefaultSystemAudioInputDeviceChanged(const AudioDeviceId &deviceId);
    virtual void onDefaultCommunicationAudioInputDeviceChanged(const AudioDeviceId &deviceId);
    virtual void onSetAudioInputDeviceCompleted(const AudioDeviceId &deviceId);
    virtual void onSetAudioInputDeviceFailed(const AudioDeviceId &deviceId, VCSStatus status);

    virtual void onDefaultSystemAudioOutputDeviceChanged(const AudioDeviceId &deviceId);
    virtual void onDefaultCommunicationAudioOutputDeviceChanged(const AudioDeviceId &deviceId);
    virtual void onSetAudioOutputDeviceCompleted(const AudioDeviceId &deviceId);
    virtual void onSetAudioOutputDeviceFailed(const AudioDeviceId &deviceId, VCSStatus status);
    virtual void onAudioInputDeviceTestPlaybackCompleted();

    INT_PTR CALLBACK AudioDialog(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
    INT_PTR CALLBACK MainDialog(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);

private:
    void UpdateAudioTestButtons();
    LRESULT InternalSendMessage(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) const;


    VivoxClientApi::ClientConnection m_vivox;
    HWND m_hwndMainDialog;
    HWND m_hwndAudioSetup;
    HWND m_hwndOsSystemInputRadio;
    HWND m_hwndOsCommInputRadio;
    HWND m_hwndAppInputRadio;
    HWND m_hwndOsSystemOutputRadio;
    HWND m_hwndOsCommOutputRadio;
    HWND m_hwndAppOutputRadio;
    HWND m_hwndInputDevices;
    HWND m_hwndOutputDevices;
    HWND m_hwndInputVolume;
    HWND m_hwndOutputVolume;
    HWND m_hwndTestInputRecord;
    HWND m_hwndTestInputPlayback;
    HWND m_hwndTestOutput;
    HWND m_hwndMuteInput;
    HWND m_hwndMuteOutput;
    mutable int m_logLines;
};
