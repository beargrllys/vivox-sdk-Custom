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
#include "SDKMessageObserver.h"
#include <string>
#include <map>

using namespace std;

class SDKBrowser :
    public ISDKMessageObserver
{
public:
    SDKBrowser();
    virtual ~SDKBrowser();

    // ISDKMessageObserver interface implementation

public:
    virtual void OnVivoxSDKMessage(vx_message_base_t *msg);

    // Overrides
    class SDKConnector;
    class SDKLogin;
    class SDKSessionGroup;
    class SDKSession;
    class SDKParticipant;

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

    // Implementation

public:
    class SDKConnector
    {
    public:
        SDKConnector(SDKBrowser *pBrowser, VX_HANDLE connectorHandle) :
            m_pBrowser(pBrowser),
            m_connectorHandle(connectorHandle)
        {
            m_pBrowser->OnConnectorCreated(this);
        }
        ~SDKConnector();

        SDKBrowser *m_pBrowser;
        string m_connectorHandle;
        map<string, SDKLogin *> m_Logins;
    };

    class SDKLogin
    {
    public:
        SDKLogin(SDKConnector *pConnector, VX_HANDLE accountHandle, const char *displayname, const char *uri, const char *encoded_uri_with_tag) :
            m_pBrowser(pConnector->m_pBrowser),
            m_pConnector(pConnector),
            m_accountHandle(accountHandle),
            m_uri(uri),
            m_encoded_uri_with_tag(encoded_uri_with_tag),
            m_state(login_state_logged_out)
        {
            if (displayname) {
                m_displayName = displayname;
            }
            m_pBrowser->OnLoginCreated(this);
        }
        ~SDKLogin();

        SDKBrowser *m_pBrowser;
        SDKConnector *m_pConnector;
        string m_accountHandle;
        string m_displayName;
        string m_uri;
        string m_encoded_uri_with_tag;
        vx_login_state_change_state m_state;
        map<string, SDKSessionGroup *> m_SessionGroups;
    };

    class SDKSessionGroup
    {
    public:
        SDKSessionGroup(SDKLogin *pLogin, VX_HANDLE sessionGroupHandle) :
            m_pBrowser(pLogin->m_pBrowser),
            m_pLogin(pLogin),
            m_sessionGroupHandle(sessionGroupHandle)
        {
            m_pBrowser->OnSessionGroupCreated(this);
        }
        ~SDKSessionGroup();

        SDKBrowser *m_pBrowser;
        SDKLogin *m_pLogin;
        string m_sessionGroupHandle;
        map<string, SDKSession *> m_Sessions;
    };

    class SDKSession
    {
    public:
        SDKSession(SDKSessionGroup *pSessionGroup, VX_HANDLE sessionHandle, const char *uri) :
            m_pBrowser(pSessionGroup->m_pBrowser),
            m_pSessionGroup(pSessionGroup),
            m_sessionHandle(sessionHandle),
            m_uri(uri),
            m_mediaState(session_media_disconnected),
            m_textState(session_text_disconnected)
        {
            m_pBrowser->OnSessionCreated(this);
        }
        ~SDKSession();

        SDKBrowser *m_pBrowser;
        SDKSessionGroup *m_pSessionGroup;
        string m_sessionHandle;
        string m_uri;
        vx_session_media_state m_mediaState;
        vx_session_text_state m_textState;
        map<string, SDKParticipant *> m_Participants;
    };

    class SDKParticipant
    {
    public:
        SDKParticipant(SDKSession *pSession, const char *displayname, const char *uri, const char *encoded_uri_with_tag, bool isCurrentUser) :
            m_pBrowser(pSession->m_pBrowser),
            m_pSession(pSession),
            m_uri(uri),
            m_encoded_uri_with_tag(encoded_uri_with_tag),
            m_isCurrentUser(isCurrentUser),
            m_isModeratorMutedText(false),
            m_isModeratorMutedAudio(false),
            m_isSpeaking(false),
            m_volume(0),
            m_energy(0),
            m_activeMedia(0),
            m_isMutedForMeAudio(false),
            m_isMutedForMeText(false),
            m_isSpeakingWhileMicMuted(false),
            m_isSpeakingWhileMicVolumeZero(false)
        {
            if (displayname) {
                m_displayName = displayname;
            }
            m_pBrowser->OnParticipantCreated(this);
        }
        ~SDKParticipant();

        SDKBrowser *m_pBrowser;
        SDKSession *m_pSession;
        string m_displayName;
        string m_uri;
        string m_encoded_uri_with_tag;
        bool m_isCurrentUser;
        bool m_isModeratorMutedText; // is_moderator_muted_text
        bool m_isModeratorMutedAudio; // is_moderator_muted_audio
        bool m_isSpeaking; // is_speaking
        int m_volume; // volume
        double m_energy; // energy
        int m_activeMedia; // active_media
        bool m_isMutedForMeAudio; // is_muted_for_me
        bool m_isMutedForMeText; // is_muted_for_me
        bool m_isSpeakingWhileMicMuted; // diagnostic_states
        bool m_isSpeakingWhileMicVolumeZero; // diagnostic_states
        bool m_hasUnavailableCaptureDevice; // has_unavailable_capture_device
        bool m_hasUnavailableRenderDevice; // has_unavailable_render_device
    };

protected:
    map<string, SDKConnector *> m_Connectors;
    map<string, SDKLogin *> m_Logins;
    map<string, SDKSessionGroup *> m_SessionGroups;
    map<string, SDKSession *> m_Sessions;

    virtual void Close();
    SDKSessionGroup *InternalAddSessionGroup(SDKLogin *pLogin, VX_HANDLE sessionGroupHandle);
};
