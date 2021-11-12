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

#include <assert.h>
#include "VxcEvents.h"
#include "VxcRequests.h"
#include "VxcResponses.h"

SDKBrowser::SDKBrowser()
{
}

SDKBrowser::~SDKBrowser()
{
    Close();
}

void SDKBrowser::Close()
{
    for (const auto &it : m_Connectors) {
        delete it.second;
    }
    m_Connectors.clear();
    m_Logins.clear();
    m_SessionGroups.clear();
    m_Sessions.clear();
}

SDKBrowser::SDKSessionGroup *SDKBrowser::InternalAddSessionGroup(SDKLogin *pLogin, VX_HANDLE sessionGroupHandle)
{
    assert(pLogin->m_SessionGroups.find(sessionGroupHandle) == pLogin->m_SessionGroups.end());
    SDKSessionGroup *pSessionGroup = new SDKSessionGroup(pLogin, sessionGroupHandle);
    pLogin->m_SessionGroups[sessionGroupHandle] = pSessionGroup;
    assert(m_SessionGroups.find(sessionGroupHandle) == m_SessionGroups.end());
    m_SessionGroups[sessionGroupHandle] = pSessionGroup;
    return pSessionGroup;
}

void SDKBrowser::OnVivoxSDKMessage(vx_message_base_t *msg)
{
    if (msg->type == msg_response) {
        vx_resp_base_t *base_resp = reinterpret_cast<vx_resp_base_t *>(msg);
        if (base_resp->return_code != 0) {
            return;
        }
        switch (base_resp->type) {
            case resp_connector_create:
            {
                vx_resp_connector_create_t *resp = (vx_resp_connector_create_t *)base_resp;
                assert(m_Connectors.find(resp->connector_handle) == m_Connectors.end());
                m_Connectors[resp->connector_handle] = new SDKConnector(this, resp->connector_handle);
                break;
            }
            case resp_account_anonymous_login:
            {
                vx_resp_account_anonymous_login_t *resp = (vx_resp_account_anonymous_login_t *)base_resp;
                vx_req_account_anonymous_login_t *req = (vx_req_account_anonymous_login_t *)(base_resp->request);
                const auto &it = m_Connectors.find(req->connector_handle);
                assert(it != m_Connectors.end());
                SDKConnector *pConnector = it->second;
                assert(pConnector->m_Logins.find(req->account_handle) == pConnector->m_Logins.end());
                SDKLogin *pLogin = new SDKLogin(pConnector, req->account_handle, req->displayname, resp->uri, resp->encoded_uri_with_tag);
                pConnector->m_Logins[req->account_handle] = pLogin;
                assert(m_Logins.find(req->account_handle) == m_Logins.end());
                m_Logins[req->account_handle] = pLogin;
                break;
            }
            case resp_account_logout:
            {
                // HINT: Not handling it here. The account will either fail to login (resp_account_anonymous_login will report failure),
                // or the account will be logged out by the server (handled in evt_account_login_state_change(login_state_logged_out) )
                break;
            }
            case resp_connector_initiate_shutdown:
            {
                vx_req_connector_initiate_shutdown_t *req = (vx_req_connector_initiate_shutdown_t *)(base_resp->request);
                const auto &it = m_Connectors.find(req->connector_handle);
                assert(it != m_Connectors.end());
                SDKConnector *pConnector = it->second;
                m_Connectors.erase(it);
                delete pConnector;
                break;
            }
            case resp_sessiongroup_add_session:
            {
                vx_req_sessiongroup_add_session_t *req = (vx_req_sessiongroup_add_session_t *)(base_resp->request);
                const auto &itLogin = m_Logins.find(req->account_handle);
                assert(itLogin != m_Logins.end());
                SDKLogin *pLogin = itLogin->second;
                const auto &itSessionGroup = m_SessionGroups.find(req->sessiongroup_handle);
                SDKSessionGroup *pSessionGroup = NULL;
                // HINT: evt_sessiongroup_added is received only after resp_sessiongroup_addsession,
                // therefore the session group may be absent during resp_sessiongroup_addsession handling
                if (itSessionGroup == m_SessionGroups.end()) {
                    pSessionGroup = InternalAddSessionGroup(pLogin, req->sessiongroup_handle);
                } else {
                    pSessionGroup = itSessionGroup->second;
                    assert(pLogin->m_SessionGroups.find(req->sessiongroup_handle) != pLogin->m_SessionGroups.end());
                }
                assert(m_Sessions.find(req->session_handle) == m_Sessions.end());
                assert(pSessionGroup->m_Sessions.find(req->session_handle) == pSessionGroup->m_Sessions.end());
                SDKSession *pSession = new SDKSession(pSessionGroup, req->session_handle, req->uri);
                pSessionGroup->m_Sessions[req->session_handle] = pSession;
                m_Sessions[req->session_handle] = pSession;
                break;
            }
            default:
                break;
        }
    } else if (msg->type == msg_event) {
        vx_evt_base_t *base_evt = reinterpret_cast<vx_evt_base_t *>(msg);
        switch (base_evt->type) {
            case evt_account_login_state_change:
            {
                vx_evt_account_login_state_change_t *evt = (vx_evt_account_login_state_change_t *)base_evt;
                if (evt->state == login_state_logging_in) {
                    // HINT: Skipping: login_state_logging_in event arrives before resp_account_anonymous_login, no way to associate with connector without access to vx_req_account_anonymous_login_t
                } else {
                    const auto &it = m_Logins.find(evt->account_handle);
                    if (it == m_Logins.end() && evt->state == login_state_logged_out) {
                        // Never logged in (login failed?)
                    } else {
                        assert(it != m_Logins.end());
                        SDKLogin *pLogin = it->second;
                        if (evt->state == login_state_logged_out) {
                            SDKConnector *pConnector = pLogin->m_pConnector;
                            pConnector->m_Logins.erase(pLogin->m_accountHandle);
                            m_Logins.erase(it);
                            delete pLogin;
                        } else {
                            pLogin->m_state = evt->state;
                            OnLoginUpdated(pLogin);
                        }
                    }
                }
                break;
            }
            case evt_sessiongroup_added:
            {
                vx_evt_sessiongroup_added_t *evt = (vx_evt_sessiongroup_added_t *)base_evt;
                const auto &it = m_Logins.find(evt->account_handle);
                assert(it != m_Logins.end());
                SDKLogin *pLogin = it->second;
                // HINT: evt_sessiongroup_added is received only after resp_sessiongroup_addsession,
                // therefore the session group may be already added during resp_sessiongroup_addsession handling
                if (pLogin->m_SessionGroups.find(evt->sessiongroup_handle) == pLogin->m_SessionGroups.end()) {
                    SDKSessionGroup *pSessionGroup = InternalAddSessionGroup(pLogin, evt->sessiongroup_handle);
                    (void)pSessionGroup;
                }
                break;
            }
            case evt_sessiongroup_removed:
            {
                vx_evt_sessiongroup_removed_t *evt = (vx_evt_sessiongroup_removed_t *)base_evt;
                const auto &it = m_SessionGroups.find(evt->sessiongroup_handle);
                assert(it != m_SessionGroups.end());
                SDKSessionGroup *pSessionGroup = it->second;
                assert(pSessionGroup->m_pLogin->m_SessionGroups.find(evt->sessiongroup_handle) != pSessionGroup->m_pLogin->m_SessionGroups.end());
                pSessionGroup->m_pLogin->m_SessionGroups.erase(evt->sessiongroup_handle);
                m_SessionGroups.erase(it);
                delete pSessionGroup;
                break;
            }
            case evt_session_added:
            {
                // HINT: channel URI is available only in req_sessiongroup_addsession; not available in evt_session_added. Add there?
                vx_evt_session_added_t *evt = (vx_evt_session_added_t *)base_evt;
                const auto &it = m_SessionGroups.find(evt->sessiongroup_handle);
                assert(it != m_SessionGroups.end());
                SDKSessionGroup *pSessionGroup = it->second;
                assert(pSessionGroup->m_Sessions.find(evt->session_handle) != pSessionGroup->m_Sessions.end());
                assert(m_Sessions.find(evt->session_handle) != m_Sessions.end());
                break;
            }
            case evt_session_removed:
            {
                vx_evt_session_removed_t *evt = (vx_evt_session_removed_t *)base_evt;
                const auto &it = m_Sessions.find(evt->session_handle);
                assert(it != m_Sessions.end());
                SDKSession *pSession = it->second;
                assert(pSession->m_pSessionGroup->m_Sessions.find(evt->session_handle) != pSession->m_pSessionGroup->m_Sessions.end());
                pSession->m_pSessionGroup->m_Sessions.erase(evt->session_handle);
                m_Sessions.erase(it);
                delete pSession;
                break;
            }
            case evt_participant_added:
            {
                vx_evt_participant_added_t *evt = (vx_evt_participant_added_t *)base_evt;
                const auto &it = m_Sessions.find(evt->session_handle);
                assert(it != m_Sessions.end());
                SDKSession *pSession = it->second;
                assert(pSession->m_pSessionGroup->m_sessionGroupHandle == evt->sessiongroup_handle);
                assert(pSession->m_Participants.find(evt->encoded_uri_with_tag) == pSession->m_Participants.end());
                SDKParticipant *pParticipant = new SDKParticipant(pSession, evt->displayname, evt->participant_uri, evt->encoded_uri_with_tag, evt->is_current_user ? true : false);
                pSession->m_Participants[evt->encoded_uri_with_tag] = pParticipant;
                break;
            }
            case evt_participant_removed:
            {
                vx_evt_participant_removed_t *evt = (vx_evt_participant_removed_t *)base_evt;

                const auto &it = m_Sessions.find(evt->session_handle);
                assert(it != m_Sessions.end());
                SDKSession *pSession = it->second;
                assert(pSession->m_pSessionGroup->m_sessionGroupHandle == evt->sessiongroup_handle);
                const auto &it2 = pSession->m_Participants.find(evt->encoded_uri_with_tag);
                assert(it2 != pSession->m_Participants.end());
                SDKParticipant *pParticipant = it2->second;
                pSession->m_Participants.erase(evt->encoded_uri_with_tag);
                delete pParticipant;
                break;
            }
            case evt_participant_updated:
            {
                vx_evt_participant_updated_t *evt = (vx_evt_participant_updated_t *)base_evt;

                const auto &it = m_Sessions.find(evt->session_handle);
                assert(it != m_Sessions.end());
                SDKSession *pSession = it->second;
                assert(pSession->m_pSessionGroup->m_sessionGroupHandle == evt->sessiongroup_handle);
                const auto &it2 = pSession->m_Participants.find(evt->encoded_uri_with_tag);
                assert(it2 != pSession->m_Participants.end());
                SDKParticipant *pParticipant = it2->second;
                assert(pParticipant->m_isCurrentUser == (evt->is_current_user ? true : false));
                assert(pParticipant->m_pSession->m_sessionHandle == evt->session_handle);
                assert(pParticipant->m_pSession->m_pSessionGroup->m_sessionGroupHandle == evt->sessiongroup_handle);
                assert(pParticipant->m_pSession->m_Participants.find(evt->encoded_uri_with_tag) != pParticipant->m_pSession->m_Participants.end());
                pParticipant->m_isModeratorMutedAudio = evt->is_moderator_muted ? true : false;
                pParticipant->m_isModeratorMutedText = evt->is_moderator_text_muted ? true : false;
                pParticipant->m_isSpeaking = evt->is_speaking ? true : false;
                pParticipant->m_volume = evt->volume;
                pParticipant->m_energy = evt->energy;
                pParticipant->m_activeMedia = evt->active_media;
                pParticipant->m_isMutedForMeAudio = evt->is_muted_for_me ? true : false;
                pParticipant->m_isMutedForMeText = evt->is_text_muted_for_me ? true : false;
                pParticipant->m_hasUnavailableCaptureDevice = evt->has_unavailable_capture_device ? true : false;
                pParticipant->m_hasUnavailableCaptureDevice = evt->has_unavailable_render_device ? true : false;
                bool isSpeakingWhileMicMuted = false;
                bool isSpeakingWhileMicVolumeZero = false;
                for (int i = 0; i < evt->diagnostic_state_count; i++) {
                    switch (evt->diagnostic_states[i]) {
                        case participant_diagnostic_state_speaking_while_mic_muted:
                        {
                            isSpeakingWhileMicMuted = true;
                            break;
                        }
                        case participant_diagnostic_state_speaking_while_mic_volume_zero:
                        {
                            isSpeakingWhileMicVolumeZero = false;
                            break;
                        }
                        case participant_diagnostic_state_no_capture_device:
                        case participant_diagnostic_state_no_render_device:
                        case participant_diagnostic_state_capture_device_read_errors:
                        case participant_diagnostic_state_render_device_write_errors:
                            break;
                        default:
                        {
                            assert(false); // new enum vx_participant_diagnostic_state_t codepoint?
                            break;
                        }
                    }
                }
                pParticipant->m_isSpeakingWhileMicMuted = isSpeakingWhileMicMuted;
                pParticipant->m_isSpeakingWhileMicVolumeZero = isSpeakingWhileMicVolumeZero;
                OnParticipantUpdated(pParticipant);
                break;
            }
            case evt_media_stream_updated:
            {
                vx_evt_media_stream_updated_t *evt = (vx_evt_media_stream_updated_t *)base_evt;
                const auto &it = m_SessionGroups.find(evt->sessiongroup_handle);
                assert(it != m_SessionGroups.end());
                SDKSessionGroup *pSessionGroup = it->second;
                const auto &itSession = m_Sessions.find(evt->session_handle);
                assert(itSession != m_Sessions.end());
                assert(pSessionGroup->m_Sessions.find(evt->session_handle) != pSessionGroup->m_Sessions.end());
                SDKSession *pSession = itSession->second;
                pSession->m_mediaState = evt->state;
                OnSessionUpdated(pSession);
                break;
            }
            case evt_text_stream_updated:
            {
                vx_evt_text_stream_updated_t *evt = (vx_evt_text_stream_updated_t *)base_evt;
                const auto &it = m_SessionGroups.find(evt->sessiongroup_handle);
                assert(it != m_SessionGroups.end());
                SDKSessionGroup *pSessionGroup = it->second;
                const auto &itSession = m_Sessions.find(evt->session_handle);
                assert(itSession != m_Sessions.end());
                assert(pSessionGroup->m_Sessions.find(evt->session_handle) != pSessionGroup->m_Sessions.end());
                SDKSession *pSession = itSession->second;
                pSession->m_textState = evt->state;
                OnSessionUpdated(pSession);
                break;
            }
            default:
                break;
        }
    } else {
        assert(false);
    }
}

void SDKBrowser::OnConnectorCreated(SDKConnector *)
{
    wprintf(L"%hs\n", __FUNCTION__);
}

void SDKBrowser::OnConnectorDestroyed(SDKConnector *)
{
    wprintf(L"%hs\n", __FUNCTION__);
}

void SDKBrowser::OnLoginCreated(SDKLogin *)
{
    wprintf(L"%hs\n", __FUNCTION__);
}

void SDKBrowser::OnLoginUpdated(SDKLogin *)
{
    wprintf(L"%hs\n", __FUNCTION__);
}

void SDKBrowser::OnLoginDestroyed(SDKLogin *)
{
    wprintf(L"%hs\n", __FUNCTION__);
}

void SDKBrowser::OnSessionGroupCreated(SDKSessionGroup *)
{
    wprintf(L"%hs\n", __FUNCTION__);
}

void SDKBrowser::OnSessionGroupDestroyed(SDKSessionGroup *)
{
    wprintf(L"%hs\n", __FUNCTION__);
}

void SDKBrowser::OnSessionCreated(SDKSession *)
{
    wprintf(L"%hs\n", __FUNCTION__);
}

void SDKBrowser::OnSessionUpdated(SDKSession *)
{
    wprintf(L"%hs\n", __FUNCTION__);
}

void SDKBrowser::OnSessionDestroyed(SDKSession *)
{
    wprintf(L"%hs\n", __FUNCTION__);
}

void SDKBrowser::OnParticipantCreated(SDKParticipant *)
{
    wprintf(L"%hs\n", __FUNCTION__);
}

void SDKBrowser::OnParticipantUpdated(SDKParticipant *)
{
    wprintf(L"%hs\n", __FUNCTION__);
}

void SDKBrowser::OnParticipantDestroyed(SDKParticipant *)
{
    wprintf(L"%hs\n", __FUNCTION__);
}

SDKBrowser::SDKConnector::~SDKConnector()
{
    for (const auto &it : m_Logins) {
        delete it.second;
    }
    m_Logins.clear();
    m_pBrowser->OnConnectorDestroyed(this);
}

SDKBrowser::SDKLogin::~SDKLogin()
{
    for (const auto &it : m_SessionGroups) {
        delete it.second;
    }
    m_SessionGroups.clear();
    m_pBrowser->OnLoginDestroyed(this);
}

SDKBrowser::SDKSessionGroup::~SDKSessionGroup()
{
    for (const auto &it : m_Sessions) {
        delete it.second;
    }
    m_Sessions.clear();
    m_pBrowser->OnSessionGroupDestroyed(this);
}

SDKBrowser::SDKSession::~SDKSession()
{
    for (const auto &it : m_Participants) {
        delete it.second;
    }
    m_Participants.clear();
    m_pBrowser->OnSessionDestroyed(this);
}

SDKBrowser::SDKParticipant::~SDKParticipant()
{
    m_pBrowser->OnParticipantDestroyed(this);
}
