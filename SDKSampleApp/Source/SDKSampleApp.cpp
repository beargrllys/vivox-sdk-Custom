/* Copyright (c) 2013-2018 by Mercer Road Corp
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
#include "SDKSampleApp.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <vector>
#include <assert.h>
#include <algorithm>
#include <iostream>
#include <cstring>
#include "vxplatform/vxcplatform.h"

#include "VxcEvents.h"
#include "VxcErrors.h"
#include "VxcResponses.h"

#include <windows.h>

#define SDK_SAMPLE_APP_PI 3.1415926535897932384626433832795

using namespace std;
using namespace vxplatform;

template <typename T, size_t n>
size_t ARRAY_COUNT(T (&array)[n])
{
    (void)array;
    return n;
}

const char *safestr(const char *s)
{
    if (s)
    {
        return s;
    }
    return "";
}

static void safe_replace_string(char **ppString, const char *pszNewValue)
{
    assert(ppString);
    vx_free(*ppString);
    *ppString = vx_strdup(pszNewValue);
}

// CommandProcessor.cpp contains SDKSampleApp class definitions that are
// largely or exclusively relevant only to the framework of the application
// itself, not to a reference implementation of the Vivox SDK. This includes
// data and methods regarding in-app help text, commandline processing, and
// logging debug messages to the console.
//
// The function definitions in this file, conversely, offer a reference
// implementation of local state management, handling of Vivox SDK messages,
// and constructing requests using the Vivox Communications API.
#include "CommandProcessor.cpp"

SDKSampleApp *SDKSampleApp::s_pInstance;

std::string GetSocketErrorString()
{
    wchar_t wideBuffer[4096];
    char buffer[4096];
    int error = WSAGetLastError();
    if (FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, error, 0, wideBuffer, (DWORD)ARRAY_COUNT(wideBuffer), NULL) == 0 ||
        WideCharToMultiByte(CP_UTF8, 0, wideBuffer, -1, buffer, sizeof(buffer), NULL, NULL) == 0)
    {
        snprintf(buffer, sizeof(buffer), "error %d", error);
    }
    return std::string(buffer);
}

std::string SDKSampleApp::getVersionAndCopyrightText()
{
    return string_format(
        "Vivox SDK Sample App Version %s\n"
        "Copyright 2013-2019 (c) by Mercer Road Corp. All rights reserved.\n",
        vx_get_sdk_version_info_ex());
}

vxplatform::os_event_handle *SDKSampleApp::ListenerEventLink()
{
    return &listenerEvent;
}

SDKSampleApp::SDKSampleApp(printf_wrapper_ptr printf_wrapper_proc, pf_exit_callback_t cbExit)
{
    assert(NULL == s_pInstance);
    s_pInstance = this;
    m_pMessageObserver = NULL;
    _printf_wrapper = printf_wrapper_proc;
    m_cbExit = cbExit;
    m_started = false;
    m_listenerThreadTerminatedEvent = NULL;
    m_messageAvailableEvent = NULL;
    m_lock = new vxplatform::Lock();
    m_requestId = 1;
    m_isMultitenant = false;
    m_neverRtpTimeoutMS = -1; // unset
    m_lostRtpTimeoutMS = -1;  // unset
    m_tcpControlPort = 9876;
    m_processorAffinityMask = 0;
#ifndef SAMPLEAPP_DISABLE_TTS
    m_ttsManagerId = NULL;
#endif
    m_logRequests = true;
    m_logResponses = true;
    m_logEvents = false;
    m_logXml = false;
    m_logParticipantUpdate = false;

    // vad properties
    m_vadHangover = 2000;
    m_vadSensitivity = 43;
    m_vadNoiseFloor = 576;
    m_vadAuto = 0;

    DeclareCommands();

    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
    m_tcpConsoleClientSocket = INVALID_SOCKET;
    m_tcpConsoleServerSocket = INVALID_SOCKET;
    m_terminateConsoleThread = false;

    listenerEvent = NULL;
}

// console redirection
// static
SDKSampleApp::printf_wrapper_ptr SDKSampleApp::_printf_wrapper = NULL;

// static
int SDKSampleApp::con_print(const char *format, ...)
{
    va_list vl;

    va_start(vl, format);
    int size = vsnprintf(NULL, 0, format, vl) + 1;
    va_end(vl);

    va_start(vl, format);
    int retcode = 0;
    if (_printf_wrapper != NULL)
    {
        retcode = _printf_wrapper(format, vl);
    }
    else
    {
        std::vector<char> str(static_cast<size_t>(size));
        retcode = vsnprintf(str.data(), str.size(), format, vl);
        if (0 > retcode)
        {
            return retcode;
        }
        size = MultiByteToWideChar(CP_UTF8, 0, str.data(), -1, NULL, 0);

        std::vector<wchar_t> tmp(size);
        LPWSTR tmpBuf = (LPWSTR)tmp.data();
        size = MultiByteToWideChar(CP_UTF8, 0, str.data(), -1, tmpBuf, size);
        if (0 > size)
        {
            return -1;
        }
        tmp.resize(size);
        std::wcout << tmp.data() << std::flush;
    }
    va_end(vl);

    SDKSampleApp *pThis = SDKSampleApp::GetApp();
    if (pThis)
    {
        if (INVALID_SOCKET != pThis->m_tcpConsoleClientSocket)
        {
            std::string s;
            va_start(vl, format);
            if (size > 0)
            {
                s.resize(size);
                size = vsnprintf(&s[0], size + 1, format, vl);
                if (size < 0)
                {
                    s.clear();
                }
                else
                {
                    s[size] = '\0';
                }
            }
            va_end(vl);

            if (!s.empty())
            {
                send(pThis->m_tcpConsoleClientSocket, s.c_str(), static_cast<int>(s.length()), 0);
                // if ('\n' != s.back()) {
                //    send(pThis->m_tcpConsoleClientSocket, "\n", 1, 0);
                // }
            }
        }
    }

    return retcode;
}

void SDKSampleApp::Start()
{
    if (!m_started)
    {
        m_started = true;
        create_event(&m_listenerThreadTerminatedEvent);
        create_event(&m_messageAvailableEvent);
        create_thread(&SDKSampleApp::ListenerThread, this, &m_listenerThread, &m_listenerThreadId);
        create_thread(&SDKSampleApp::TcpConsoleThreadProcStatic, this, &m_tcpConsoleThread, &m_tcpConsoleThreadId);
    }
}

void SDKSampleApp::Stop()
{
    for (auto i = m_sessions.begin(); i != m_sessions.end(); ++i)
    {
        delete i->second;
    }
    m_sessions.clear();

    if (m_started)
    {
        m_started = false;
        set_event(m_messageAvailableEvent);
        if (wait_event(m_listenerThreadTerminatedEvent, 30000))
        {
            con_print("\r * Timed out waiting for listener thread");
        }
        delete_event(m_messageAvailableEvent);
        m_messageAvailableEvent = NULL;
        delete_event(m_listenerThreadTerminatedEvent);
        m_listenerThreadTerminatedEvent = NULL;
        TerminateListenerThread();
        TerminateTcpConsoleThread();
        WSACleanup();
    }
}

void SDKSampleApp::Shutdown()
{
    if (nullptr != m_cbExit)
    {
        m_cbExit();
    }
}

SDKSampleApp::~SDKSampleApp()
{
    Stop();
    delete m_lock;
    m_lock = nullptr;
    s_pInstance = nullptr;
    _printf_wrapper = nullptr;
}

void SDKSampleApp::Wakeup()
{
    set_event(m_messageAvailableEvent);
}

void SDKSampleApp::Lock()
{
    m_lock->Take();
}

void SDKSampleApp::Unlock()
{
    m_lock->Release();
}

os_error_t SDKSampleApp::ListenerThread(void *arg)
{
    SDKSampleApp *pThis = reinterpret_cast<SDKSampleApp *>(arg);
    pThis->ListenerThread();
    return 0;
}

void SDKSampleApp::HandleMessage(vx_message_base_t *msg)
{
    if (m_pMessageObserver)
    {
        m_pMessageObserver->OnVivoxSDKMessage(msg);
    }

    if (msg->type == msg_response)
    {
        vx_resp_base_t *resp = reinterpret_cast<vx_resp_base_t *>(msg);
        if (resp->return_code == 1)
        {
            if (m_logResponses)
            {
                con_print(
                    "\r * Response %s returned '%s'(%d)\n",
                    vx_get_response_type_string(resp->type),
                    vx_get_error_string(resp->status_code),
                    resp->status_code);
                if (m_logXml)
                {
                    con_print("\r * %s\n", Xml(resp).c_str());
                }
            }
            con_print("[SDKSampleApp]: ");
        }
        else
        {
            if (m_logResponses)
            {
                con_print("\r * Request %s with cookie=%s completed.\n", vx_get_request_type_string(resp->request->type), resp->request->cookie);
                if (m_logXml)
                {
                    con_print("\r * %s\n", Xml(resp).c_str());
                }
                con_print("\n%s\n", "Cummunication Termination!");
                vxplatform::set_event(listenerEvent);

            }
            switch (resp->type)
            {
            // needed to properly store the connector handle if autogenerated
            case resp_connector_create:
            {
                vx_resp_connector_create_t *tresp = (vx_resp_connector_create_t *)resp;
                m_connectorHandle = tresp->connector_handle;
                break;
            }
            case resp_connector_get_local_audio_info:
            {
                vx_resp_connector_get_local_audio_info *r = reinterpret_cast<vx_resp_connector_get_local_audio_info *>(resp);
                con_print("\r * is_mic_muted = %s\n", r->is_mic_muted ? "true" : "false");
                con_print("\r * is_speaker_muted = %s\n", r->is_speaker_muted ? "true" : "false");
                con_print("\r * mic_volume = %d\n", r->mic_volume);
                con_print("\r * speaker_volume = %d\n", r->speaker_volume);
                break;
            }
            // these responses help us keep track of active accounts, session groups, and sessions locally
            case resp_account_anonymous_login:
            {
                lock_guard<mutex> lock(m_accountHandleUserNameMutex);
                vx_resp_account_anonymous_login *tresp = reinterpret_cast<vx_resp_account_anonymous_login *>(resp);
                vx_req_account_anonymous_login *req = reinterpret_cast<vx_req_account_anonymous_login *>(resp->request);
                m_accountHandles.insert(tresp->account_handle);
                m_accountHandleUserNames[tresp->account_handle] = req->acct_name;
                break;
            }
            case resp_sessiongroup_add_session:
            {
                vx_resp_sessiongroup_add_session *tresp = reinterpret_cast<vx_resp_sessiongroup_add_session *>(resp);
                vx_req_sessiongroup_add_session *req = reinterpret_cast<vx_req_sessiongroup_add_session *>(resp->request);
                m_sessions.insert(make_pair(tresp->session_handle, new Session(req->uri, tresp->session_handle, req->sessiongroup_handle, req->account_handle)));
                break;
            }
            case resp_sessiongroup_remove_session:
                break;
            // these are all the responses that can return data that the user might be interested in seeing
            case resp_aux_get_render_devices:
            {
                vx_resp_aux_get_render_devices *tresp = reinterpret_cast<vx_resp_aux_get_render_devices *>(resp);
                vx_req_aux_get_render_devices_t *req = reinterpret_cast<vx_req_aux_get_render_devices_t *>(resp->request);
                string account_handle;
                if (req->account_handle != NULL)
                {
                    account_handle = req->account_handle;
                }

                // Guarding this because the command handler and response handler
                // run on separate threads and can contend for this map very easily,
                // especially in scenarios where there are multiple account_handles.
                lock_guard<mutex> lock(m_setRenderDeviceIndexMutex);
                auto renderDeviceIndexByAccountHandle = m_setRenderDeviceIndexByAccountHandle.find(account_handle);
                if (renderDeviceIndexByAccountHandle == m_setRenderDeviceIndexByAccountHandle.end())
                {
                    con_print("\r * Current Render Device: %s (%s)\n", safestr(tresp->current_render_device->display_name), safestr(tresp->current_render_device->device));
                    con_print("\r * Default Render Device: %s (%s)\n", safestr(tresp->default_render_device->display_name), safestr(tresp->default_render_device->device));
                    con_print("\r * Default Communication Render Device: %s (%s)\n", safestr(tresp->default_communication_render_device->display_name), safestr(tresp->default_communication_render_device->device));
                    con_print("\r * Effective Render Device: %s (%s)\n", safestr(tresp->effective_render_device->display_name), safestr(tresp->effective_render_device->device));
                    for (int i = 0; i < tresp->count; ++i)
                    {
                        con_print("\r * Available Render Device: [%d] %s (%s)\n", i + 1, safestr(tresp->render_devices[i]->display_name), safestr(tresp->render_devices[i]->device));
                    }
                }
                else
                {
                    // This branch is used as a consequence of "renderdevice -i <index>".
                    // It will set a key/value pair on this map and call aux_set_render_device
                    // to do the actual "renderdevice set" operation.
                    if (renderDeviceIndexByAccountHandle->second > tresp->count)
                    {
                        con_print("Error: unable to switch to render device with index %d - there are only %d render devices.\n", renderDeviceIndexByAccountHandle->second, tresp->count);
                    }
                    else
                    {
                        vx_device_t *device = tresp->render_devices[renderDeviceIndexByAccountHandle->second - 1];
                        con_print("Switching to [%d] %s (%s)\n", renderDeviceIndexByAccountHandle->second, safestr(device->display_name), safestr(device->device));
                        aux_set_render_device(renderDeviceIndexByAccountHandle->first, device->device);
                    }
                    m_setRenderDeviceIndexByAccountHandle.erase(renderDeviceIndexByAccountHandle);
                }
                break;
            }
            case resp_aux_get_capture_devices:
            {
                vx_resp_aux_get_capture_devices *tresp = reinterpret_cast<vx_resp_aux_get_capture_devices *>(resp);
                vx_req_aux_get_capture_devices_t *req = reinterpret_cast<vx_req_aux_get_capture_devices_t *>(resp->request);
                string account_handle;
                if (req->account_handle != NULL)
                {
                    account_handle = req->account_handle;
                }

                // Guarding this because the command handler and response handler
                // run on separate threads and can contend for this map very easily,
                // especially in scenarios where there are multiple account_handles.
                lock_guard<mutex> lock(m_setCaptureDeviceIndexMutex);
                auto captureDeviceIndexByAccountHandle = m_setCaptureDeviceIndexByAccountHandle.find(account_handle);
                if (captureDeviceIndexByAccountHandle == m_setCaptureDeviceIndexByAccountHandle.end())
                {
                    con_print("\r * Current Capture Device: %s (%s)\n", safestr(tresp->current_capture_device->display_name), safestr(tresp->current_capture_device->device));
                    con_print("\r * Default Capture Device: %s (%s)\n", safestr(tresp->default_capture_device->display_name), safestr(tresp->default_capture_device->device));
                    con_print("\r * Default Communication Capture Device: %s (%s)\n", safestr(tresp->default_communication_capture_device->display_name), safestr(tresp->default_communication_capture_device->device));
                    con_print("\r * Effective Capture Device: %s (%s)\n", safestr(tresp->effective_capture_device->display_name), safestr(tresp->effective_capture_device->device));
                    for (int i = 0; i < tresp->count; ++i)
                    {
                        con_print("\r * Available Capture Device: [%d] %s (%s)\n", i + 1, safestr(tresp->capture_devices[i]->display_name), safestr(tresp->capture_devices[i]->device));
                    }
                }
                else
                {
                    // This branch is used as a consequence of "capturedevice -i <index>".
                    // It will set a key/value pair on this map and call aux_set_capture_device
                    // to do the actual "capturedevice set" operation.
                    if (captureDeviceIndexByAccountHandle->second > tresp->count)
                    {
                        con_print("Error: unable to switch to capture device with index %d - there are only %d capture devices.\n", captureDeviceIndexByAccountHandle->second, tresp->count);
                    }
                    else
                    {
                        vx_device_t *device = tresp->capture_devices[captureDeviceIndexByAccountHandle->second - 1];
                        con_print("Switching to [%d] %s (%s)\n", captureDeviceIndexByAccountHandle->second, safestr(device->display_name), safestr(device->device));
                        aux_set_capture_device(captureDeviceIndexByAccountHandle->first, device->device);
                    }
                    m_setCaptureDeviceIndexByAccountHandle.erase(captureDeviceIndexByAccountHandle);
                }
                break;
            }
            case resp_aux_get_mic_level:
            {
                vx_resp_aux_get_mic_level *tresp = reinterpret_cast<vx_resp_aux_get_mic_level *>(resp);
                con_print("\r * Current Master Mic Volume: %d\n", tresp->level);
                break;
            }
            case resp_aux_get_speaker_level:
            {
                vx_resp_aux_get_speaker_level *tresp = reinterpret_cast<vx_resp_aux_get_speaker_level *>(resp);
                con_print("\r * Current Master Speaker Volume: %d\n", tresp->level);
                break;
            }
            case resp_aux_get_vad_properties:
            {
                vx_resp_aux_get_vad_properties *tresp = reinterpret_cast<vx_resp_aux_get_vad_properties *>(resp);
                con_print("\r * VAD Hangover: %d\n", tresp->vad_hangover);
                con_print("\r * VAD Sensitivity: %d\n", tresp->vad_sensitivity);
                con_print("\r * VAD Noise Floor: %d\n", tresp->vad_noise_floor);
                con_print("\r * VAD Automatic: %d\n", tresp->vad_auto);
                // Since we're tracking these locally, sync up
                m_vadHangover = tresp->vad_hangover;
                m_vadSensitivity = tresp->vad_sensitivity;
                m_vadNoiseFloor = tresp->vad_noise_floor;
                m_vadAuto = tresp->vad_auto;
                break;
            }
            case resp_aux_get_derumbler_properties:
            {
                vx_resp_aux_get_derumbler_properties *tresp = reinterpret_cast<vx_resp_aux_get_derumbler_properties *>(resp);
                con_print("\r * Derumbler Enabled: %d\n", tresp->enabled);
                con_print("\r * Derumbler Stopband Corner Frequency: %d\n", tresp->stopband_corner_frequency);
                break;
            }
            case resp_sessiongroup_get_stats:
            {
                vx_resp_sessiongroup_get_stats *tresp = reinterpret_cast<vx_resp_sessiongroup_get_stats *>(resp);
                if (tresp->base.return_code == 0)
                {
                    stringstream stats;
                    stats << "\r * incoming_received: " << tresp->incoming_received << "\n";
                    stats << "\r * incoming_expected: " << tresp->incoming_expected << "\n";
                    stats << "\r * incoming_packetloss: " << tresp->incoming_packetloss << "\n";
                    stats << "\r * incoming_out_of_time: " << tresp->incoming_out_of_time << "\n";
                    stats << "\r * incoming_discarded: " << tresp->incoming_discarded << "\n";
                    stats << "\r * outgoing_sent: " << tresp->outgoing_sent << "\n";
                    stats << "\r * render_device_underruns: " << tresp->render_device_underruns << "\n";
                    stats << "\r * render_device_overruns: " << tresp->render_device_overruns << "\n";
                    stats << "\r * render_device_errors: " << tresp->render_device_errors << "\n";
                    stats << "\r * call_id: " << safestr(tresp->call_id) << "\n";
                    stats << "\r * plc_on: " << tresp->plc_on << "\n";
                    stats << "\r * plc_synthetic_frames: " << tresp->plc_synthetic_frames << "\n";
                    stats << "\r * codec_name: " << safestr(tresp->codec_name) << "\n";
                    stats << "\r * min_latency: " << tresp->min_latency << "\n";
                    stats << "\r * max_latency: " << tresp->max_latency << "\n";
                    stats << "\r * latency_measurement_count: " << tresp->latency_measurement_count << "\n";
                    stats << "\r * latency_sum: " << tresp->latency_sum << "\n";
                    stats << "\r * last_latency_measured: " << tresp->last_latency_measured << "\n";
                    stats << "\r * latency_packets_lost: " << tresp->latency_packets_lost << "\n";
                    stats << "\r * r_factor: " << tresp->r_factor << "\n";
                    stats << "\r * latency_packets_sent: " << tresp->latency_packets_sent << "\n";
                    stats << "\r * latency_packets_dropped: " << tresp->latency_packets_dropped << "\n";
                    stats << "\r * latency_packets_malformed: " << tresp->latency_packets_malformed << "\n";
                    stats << "\r * latency_packets_negative_latency: " << tresp->latency_packets_negative_latency << "\n";
                    stats << "\r * sample_interval_begin: " << tresp->sample_interval_begin << "\n";
                    stats << "\r * sample_interval_end: " << tresp->sample_interval_end << "\n";
                    for (int i = 0; i < sizeof(tresp->capture_device_consecutively_read_count) / sizeof(tresp->capture_device_consecutively_read_count[0]); ++i)
                    {
                        stats << "\r * capture_device_consecutively_read_count: [" << i << "]=" << tresp->capture_device_consecutively_read_count[i] << "\n";
                    }
                    stats << "\r * signal_secure: " << tresp->signal_secure;
                    con_print("%s\n", stats.str().c_str());
                }
                break;
            }
            case resp_account_list_buddies_and_groups:
            {
                vx_resp_account_list_buddies_and_groups *tresp = reinterpret_cast<vx_resp_account_list_buddies_and_groups *>(resp);
                if (tresp->buddy_count == 0)
                {
                    con_print("\r * No buddies found for account %s\n", reinterpret_cast<vx_req_account_list_buddies_and_groups *>(tresp->base.request)->account_handle);
                }
                else
                {
                    for (int i = 0; i < tresp->buddy_count; ++i)
                    {
                        con_print("\r * Buddy URI: %s\n", safestr(tresp->buddies[i]->buddy_uri));
                    }
                }
                break;
            }
            case resp_account_list_block_rules:
            {
                vx_resp_account_list_block_rules *tresp = reinterpret_cast<vx_resp_account_list_block_rules *>(resp);
                if (tresp->rule_count == 0)
                {
                    con_print("\r * No block rules found for account %s\n", reinterpret_cast<vx_req_account_list_block_rules *>(tresp->base.request)->account_handle);
                }
                else
                {
                    for (int i = 0; i < tresp->rule_count; ++i)
                    {
                        con_print("\r * Block Rule: %s\n", safestr(tresp->block_rules[i]->block_mask));
                    }
                }
                break;
            }
            case resp_account_list_auto_accept_rules:
            {
                vx_resp_account_list_auto_accept_rules *tresp = reinterpret_cast<vx_resp_account_list_auto_accept_rules *>(resp);
                if (tresp->rule_count == 0)
                {
                    con_print("\r * No accept rules found for account %s\n", reinterpret_cast<vx_req_account_list_auto_accept_rules *>(tresp->base.request)->account_handle);
                }
                else
                {
                    for (int i = 0; i < tresp->rule_count; ++i)
                    {
                        con_print("\r * Accept Rule: %s\n", safestr(tresp->auto_accept_rules[i]->auto_accept_mask));
                    }
                }
                break;
            }
            case resp_account_send_message:
            {
                vx_resp_account_send_message *tresp = reinterpret_cast<vx_resp_account_send_message *>(resp);
                con_print("\r * Directed message send request id assigned: %s\n", tresp->request_id);
                break;
            }
            case resp_account_control_communications:
            {
                vx_req_account_control_communications *tresp = reinterpret_cast<vx_req_account_control_communications *>(resp);
                std::string operation = "";
                switch (tresp->operation)
                {
                case vx_control_communications_operation_block:
                    con_print("\r * Blocked URIs:\n");
                    operation = "blocked";
                    break;
                case vx_control_communications_operation_mute:
                    con_print("\r * Muted URIs:\n");
                    operation = "muted";
                    break;
                case vx_control_communications_operation_unblock:
                    con_print("\r * Unblocked URIs:\n");
                    operation = "unblocked";
                    break;
                case vx_control_communications_operation_unmute:
                    con_print("\r * Unmuted URIs:\n");
                    operation = "unmuted";
                    break;
                case vx_control_communications_operation_block_list:
                    con_print("\r * Currently Blocked URIs:\n");
                    operation = "blocked";
                    break;
                case vx_control_communications_operation_mute_list:
                    con_print("\r * Currently Muted URIs:\n");
                    operation = "muted";
                    break;
                case vx_control_communications_operation_clear_block_list:
                    con_print("\r * Block list is clear.\n");
                    break;
                case vx_control_communications_operation_clear_mute_list:
                    con_print("\r * Mute list is clear.\n");
                    break;
                default:
                    con_print("\r * Error: Unknown operation.\n");
                }
                if (!operation.empty())
                {
                    if (tresp->user_uris != NULL)
                    {
                        con_print("\r * %s\n", tresp->user_uris);
                    }
                }
                break;
            }

            // Not handled
            case resp_none:
            case resp_connector_initiate_shutdown:
            case resp_account_logout:
            case resp_account_set_login_properties:
            case resp_sessiongroup_set_focus:
            case resp_sessiongroup_unset_focus:
            case resp_sessiongroup_reset_focus:
            case resp_sessiongroup_set_tx_session:
            case resp_sessiongroup_set_tx_all_sessions:
            case resp_sessiongroup_set_tx_no_session:
            case resp_session_media_connect:
            case resp_session_media_disconnect:
            case resp_session_mute_local_speaker:
            case resp_session_set_local_speaker_volume:
            case resp_session_set_local_render_volume:
            case resp_session_set_participant_volume_for_me:
            case resp_session_set_participant_mute_for_me:
            case resp_session_set_3d_position:
            case resp_channel_mute_user:
            case resp_channel_kick_user:
            case resp_channel_mute_all_users:
            case resp_connector_mute_local_mic:
            case resp_connector_mute_local_speaker:
            case resp_account_buddy_set:
            case resp_account_buddy_delete:
            case resp_account_set_presence:
            case resp_account_send_subscription_reply:
            case resp_account_create_block_rule:
            case resp_account_delete_block_rule:
            case resp_account_create_auto_accept_rule:
            case resp_account_delete_auto_accept_rule:
            case resp_session_send_message:
            case resp_session_send_notification:
            case resp_aux_set_render_device:
            case resp_aux_set_capture_device:
            case resp_aux_set_mic_level:
            case resp_aux_set_speaker_level:
            case resp_aux_render_audio_start:
            case resp_aux_render_audio_stop:
            case resp_aux_capture_audio_start:
            case resp_aux_capture_audio_stop:
            case resp_sessiongroup_set_session_3d_position:
            case resp_aux_start_buffer_capture:
            case resp_aux_play_audio_buffer:
            case resp_session_text_connect:
            case resp_session_text_disconnect:
            case resp_aux_set_vad_properties:
            case resp_sessiongroup_control_audio_injection:
            case resp_aux_notify_application_state_change:
            case resp_session_archive_query:
            case resp_account_archive_query:
            case resp_session_transcription_control:
            case resp_aux_set_derumbler_properties:
            case resp_max:
            default:
                break;
            }
            con_print("[SDKSampleApp]: ");
        }
    }
    else if (msg->type == msg_event)
    {
        vx_evt_base_t *evt = reinterpret_cast<vx_evt_base_t *>(msg);
        if (m_logEvents)
        {
            con_print("\r * Event %s received\n", vx_get_event_type_string(evt->type));
            if (m_logXml)
            {
                con_print("\r * %s\n", Xml(evt).c_str());
            }
        }
        switch (evt->type)
        {
        case evt_tts_injection_started:
        {
            vx_evt_tts_injection_started *tevt = (vx_evt_tts_injection_started *)evt;
            con_print("\r * %s: Text-to-speech message was injected.\n\tID: %d\n\tDestination: %s\n\tDuration: %fs\n\tNumber of consumer(s): %d\n.", vx_get_event_type_string(evt->type), tevt->utterance_id, vx_get_tts_dest_string(tevt->tts_destination), tevt->utterance_duration, tevt->num_consumers);
            break;
        }
        case evt_tts_injection_ended:
        {
            vx_evt_tts_injection_ended *tevt = (vx_evt_tts_injection_ended *)evt;
            con_print("\r * %s: Injection of text-to-speech message ended.\n\tID: %d\n\tDestination: %s\n\tNumber of consumer(s): %d\n.", vx_get_event_type_string(evt->type), tevt->utterance_id, vx_get_tts_dest_string(tevt->tts_destination), tevt->num_consumers);
            break;
        }
        case evt_tts_injection_failed:
        {
            vx_evt_tts_injection_failed *tevt = (vx_evt_tts_injection_failed *)evt;
            con_print("\r * %s: Injection of text-to-speech message failed.\n\tID: %d\n\tDestination: %s\n\tTTS status code: %d\n\tTTS status string: %s\n", vx_get_event_type_string(evt->type), tevt->utterance_id, vx_get_tts_dest_string(tevt->tts_destination), tevt->status, vx_get_tts_status_string(tevt->status));
            break;
        }
        case evt_account_login_state_change:
        {
            vx_evt_account_login_state_change *tevt = (vx_evt_account_login_state_change *)evt;
            if (tevt->status_code)
            {
                con_print("\r * %s: %s %s %s\n", vx_get_event_type_string(evt->type), tevt->account_handle, vx_get_login_state_string(tevt->state), vx_get_error_string(tevt->status_code));
            }
            else
            {
                con_print("\r * %s: %s %s\n", vx_get_event_type_string(evt->type), tevt->account_handle, vx_get_login_state_string(tevt->state));
            }
            if (login_state_logged_out == tevt->state)
            {
                InternalLoggedOut(tevt->account_handle);
            }
            break;
        }
        case evt_buddy_presence:
        {
            vx_evt_buddy_presence *tevt = (vx_evt_buddy_presence *)evt;
            con_print("\r * %s: %s %s %s displayname=\"%s\" custom_message=\"%s\"\n", vx_get_event_type_string(evt->type), tevt->account_handle, tevt->buddy_uri, vx_get_presence_state_string(tevt->presence), tevt->displayname ? tevt->displayname : "", tevt->custom_message);
            break;
        }
        case evt_subscription:
        {
            vx_evt_subscription *tevt = (vx_evt_subscription *)evt;
            con_print("\r * %s: %s %s displayname=\"%s\"\n", vx_get_event_type_string(evt->type), tevt->account_handle, tevt->buddy_uri, tevt->displayname ? tevt->displayname : "");
            break;
        }
        case evt_session_notification:
        {
            vx_evt_session_notification *tevt = (vx_evt_session_notification *)evt;
            con_print("\r * %s: %s %s %s\n", vx_get_event_type_string(evt->type), tevt->session_handle, tevt->participant_uri, vx_get_notification_type_string(tevt->notification_type));
            break;
        }
        case evt_message:
        {
            vx_evt_message *tevt = (vx_evt_message *)evt;
            con_print("\r * %s: %s %s %s (is_current_user = %d) displayname=\"%s\" custom data[%s]: %s\n", vx_get_event_type_string(evt->type), tevt->session_handle, tevt->participant_uri, tevt->message_body, tevt->is_current_user, tevt->participant_displayname ? tevt->participant_displayname : "", tevt->application_stanza_namespace, tevt->application_stanza_body);

#ifndef SAMPLEAPP_DISABLE_TTS
            // Optional: Check if (!tevt->is_current_user) in order to speak only other users' messages!
            if (m_ttsManagerId != NULL)
            {
                string sessionHandle = string(tevt->session_handle);
                if (m_ttsSessionHandles.count(sessionHandle))
                {
                    string username;
                    if (tevt->participant_displayname != NULL)
                    {
                        username = string(tevt->participant_displayname);
                    }
                    else
                    {
                        username = string("Anonymous");
                    }
                    string text = username + " says " + string(tevt->message_body);
                    vx_tts_utterance_id utteranceID;
                    vx_tts_speak(*m_ttsManagerId, m_ttsVoiceID, text.c_str(), tts_dest_queued_local_playback, &utteranceID);
                }
            }
#endif
            break;
        }
        case evt_aux_audio_properties:
            // too frequent for a console application
            return;
        case evt_buddy_changed:
        {
            vx_evt_buddy_changed *tevt = (vx_evt_buddy_changed *)evt;
            con_print("\r * %s: %s %s %s\n", vx_get_event_type_string(evt->type), tevt->account_handle, tevt->buddy_uri, tevt->change_type == change_type_set ? "set" : "delete");
            break;
        }
        case evt_media_stream_updated:
        {
            vx_evt_media_stream_updated *tevt = (vx_evt_media_stream_updated *)evt;
            con_print("\r * %s: %s %s '%s' (%d)\n", vx_get_event_type_string(evt->type), tevt->session_handle, vx_get_session_media_state_string(tevt->state), vx_get_error_string(tevt->status_code), tevt->status_code);
            if (tevt->call_stats && tevt->call_stats->codec_name && tevt->call_stats->codec_name[0])
            {
                con_print("    Codec: %s\n", tevt->call_stats->codec_name);
            }
            break;
        }
        case evt_text_stream_updated:
        {
            vx_evt_text_stream_updated *tevt = (vx_evt_text_stream_updated *)evt;
            con_print("\r * %s: %s %s '%s' (%d)\n", vx_get_event_type_string(evt->type), tevt->session_handle, vx_get_session_text_state_string(tevt->state), vx_get_error_string(tevt->status_code), tevt->status_code);
            break;
        }
        case evt_sessiongroup_added:
        {
            vx_evt_sessiongroup_added *evt_sg_added = (vx_evt_sessiongroup_added *)evt;
            con_print("\r * %s: %s [%s]\n", vx_get_event_type_string(evt->type), evt_sg_added->sessiongroup_handle, evt_sg_added->account_handle);
            // DOOMan: don't know the session_handle here, therefore can't add anything to m_SSGHandles, will wait for evt_session_added
            break;
        }
        case evt_sessiongroup_removed:
        {
            vx_evt_sessiongroup_removed *evt_sg_removed = (vx_evt_sessiongroup_removed *)evt;
            con_print("\r * %s: %s\n", vx_get_event_type_string(evt->type), evt_sg_removed->sessiongroup_handle);
            // Nothing to do here
            break;
        }
        case evt_session_added:
        {
            vx_evt_session_added *evt_s_added = (vx_evt_session_added *)evt;
            con_print("\r * %s: %s [%s]\n", vx_get_event_type_string(evt->type), evt_s_added->session_handle, evt_s_added->sessiongroup_handle);
            m_SSGHandles.insert(m_SSGHandles.begin(), SSGPair(evt_s_added->sessiongroup_handle, evt_s_added->session_handle));
            // DOOMan: don't know the channel URI here, therefore can't add anything to m_sessions, will add in resp_sessiongroup_add_session handler
            break;
        }
        case evt_session_removed:
        {
            vx_evt_session_removed *evt_sr = (vx_evt_session_removed *)evt;
            con_print("\r * %s: %s [%s]\n", vx_get_event_type_string(evt->type), evt_sr->session_handle, evt_sr->sessiongroup_handle);
            auto itr = std::find_if(m_SSGHandles.begin(), m_SSGHandles.end(), [&](const SSGPair &element)
                                    { return element.second == evt_sr->session_handle; });
            if (itr != m_SSGHandles.end())
            {
                m_SSGHandles.erase(itr);
            }

            auto i = m_sessions.find(evt_sr->session_handle);
            if (i != m_sessions.end())
            {
                delete i->second;
                m_sessions.erase(i);
            }

            break;
        }
        case evt_participant_added:
        {
            vx_evt_participant_added *tevt = (vx_evt_participant_added *)evt;
            con_print("\r * %s: %s %s displayname=\"%s\"\n", vx_get_event_type_string(evt->type), tevt->session_handle, tevt->participant_uri, tevt->displayname ? tevt->displayname : "");
            auto itr = m_sessions.find(tevt->session_handle);
            if (itr != m_sessions.end())
            {
                itr->second->AddParticipant(tevt->participant_uri);
            }
            break;
        }
        case evt_participant_removed:
        {
            vx_evt_participant_removed *tevt = (vx_evt_participant_removed *)evt;
            con_print("\r * %s: %s %s\n", vx_get_event_type_string(evt->type), tevt->session_handle, tevt->participant_uri);
            auto itr = m_sessions.find(tevt->session_handle);
            if (itr != m_sessions.end())
            {
                itr->second->RemoveParticipant(tevt->participant_uri);
            }
            break;
        }
        case evt_participant_updated:
        {
            vx_evt_participant_updated *tevt = (vx_evt_participant_updated *)evt;
            if (m_logParticipantUpdate)
            {
                std::stringstream optionalPrintouts;
                if (tevt->has_unavailable_capture_device)
                {
                    optionalPrintouts << " has_unavailable_capture_device = true";
                }
                if (tevt->has_unavailable_render_device)
                {
                    optionalPrintouts << " has_unavailable_render_device = true";
                }
                if (tevt->diagnostic_state_count > 0)
                {
                    optionalPrintouts << " diagnostic_states = [ ";
                }
                for (int i = 0; i < tevt->diagnostic_state_count; ++i)
                {
                    vx_participant_diagnostic_state_t diagnosticState = (vx_participant_diagnostic_state_t)tevt->diagnostic_states[i];
                    switch (diagnosticState)
                    {
                    case participant_diagnostic_state_speaking_while_mic_muted:
                        optionalPrintouts << "speaking_while_mic_muted";
                        break;
                    case participant_diagnostic_state_speaking_while_mic_volume_zero:
                        optionalPrintouts << "speaking_while_mic_volume_zero";
                        break;
                    case participant_diagnostic_state_no_capture_device:
                        optionalPrintouts << "no_capture_device";
                        break;
                    case participant_diagnostic_state_no_render_device:
                        optionalPrintouts << "no_render_device";
                        break;
                    case participant_diagnostic_state_capture_device_read_errors:
                        optionalPrintouts << "capture_device_read_errors";
                        break;
                    case participant_diagnostic_state_render_device_write_errors:
                        optionalPrintouts << "render_device_write_errors";
                        break;
                    }
                    optionalPrintouts << " ";
                }
                if (tevt->diagnostic_state_count > 0)
                {
                    optionalPrintouts << "]";
                }
                con_print(
                    "\r * %s: %s %s [speaking = %d energy = %.2f is_moderator_muted = %s is_muted_for_me = %s%s]\n",
                    vx_get_event_type_string(evt->type),
                    tevt->session_handle,
                    tevt->participant_uri,
                    tevt->is_speaking,
                    tevt->energy,
                    tevt->is_moderator_muted ? "true" : "false",
                    tevt->is_muted_for_me ? "true" : "false",
                    optionalPrintouts.str().c_str());
            }
            auto itr = m_sessions.find(tevt->session_handle);
            if (itr != m_sessions.end())
            {
                itr->second->UpdateParticipant(tevt);
            }
        }
            return;
        case evt_media_completion:
        {
            vx_evt_media_completion *tevt = (vx_evt_media_completion *)evt;
            con_print("\r * %s: %s %s\n", vx_get_event_type_string(evt->type), tevt->sessiongroup_handle, vx_get_media_completion_type_string(tevt->completion_type));
            break;
        }
        case evt_audio_device_hot_swap:
        {
            vx_evt_audio_device_hot_swap *tevt = (vx_evt_audio_device_hot_swap *)evt;
            con_print("\r * %s: %s %s\n", vx_get_event_type_string(evt->type), vx_get_audio_device_hot_swap_type_string(tevt->event_type), tevt->relevant_device ? tevt->relevant_device->display_name : "");
            break;
        }
        case evt_user_to_user_message:
        {
            vx_evt_user_to_user_message *tevt = (vx_evt_user_to_user_message *)evt;
            con_print("\r * %s: %s %s - %s\n", vx_get_event_type_string(evt->type), tevt->account_handle, tevt->from_uri, tevt->message_body);
            if (tevt->from_displayname)
            {
                con_print(" *   from_displayname: %s\n", tevt->from_displayname);
            }
            if (tevt->application_stanza_namespace)
            {
                con_print(" *   application_stanza_namespace: %s\n", tevt->application_stanza_namespace);
            }
            if (tevt->application_stanza_body)
            {
                con_print(" *   application_stanza_body: %s\n", tevt->application_stanza_body);
            }
            break;
        }
        case evt_session_archive_message:
        {
            vx_evt_session_archive_message *tevt = (vx_evt_session_archive_message *)evt;
            con_print(
                "\r * %s: %s %s %s [%s : %s] %s\n",
                vx_get_event_type_string(evt->type),
                tevt->session_handle,
                (tevt->is_current_user == 1) ? "me" : "<-",
                tevt->participant_uri,
                tevt->time_stamp,
                tevt->message_id,
                tevt->message_body);
            break;
        }
        case evt_session_archive_query_end:
        {
            vx_evt_session_archive_query_end *tevt = (vx_evt_session_archive_query_end *)evt;
            if (tevt->return_code == 0)
            {
                con_print("\r * %s Query ends adnormally: '%s' (%d)\n", vx_get_event_type_string(evt->type), vx_get_error_string(tevt->status_code), tevt->status_code);
            }
            else
            {
                con_print("\r * %s: %s {%s (%d) - %s} %d\n", vx_get_event_type_string(evt->type), tevt->session_handle, tevt->first_id, tevt->first_index, tevt->last_id, tevt->count);
            }
            break;
        }
        case evt_account_archive_message:
        {
            vx_evt_account_archive_message *tevt = (vx_evt_account_archive_message *)evt;
            con_print(
                "\r * %s: %s %s %s [%s : %s] %s\n",
                vx_get_event_type_string(evt->type),
                tevt->account_handle,
                (tevt->is_inbound == 1) ? "<-" : "->",
                (tevt->participant_uri ? tevt->participant_uri : tevt->channel_uri),
                tevt->time_stamp,
                tevt->message_id,
                tevt->message_body);
            break;
        }
        case evt_account_archive_query_end:
        {
            vx_evt_account_archive_query_end *tevt = (vx_evt_account_archive_query_end *)evt;
            if (tevt->return_code == 0)
            {
                con_print("\r * %s Query ends adnormally: '%s' (%d)\n", vx_get_event_type_string(evt->type), vx_get_error_string(tevt->status_code), tevt->status_code);
            }
            else
            {
                con_print("\r * %s: %s {%s (%d) - %s} %d\n", vx_get_event_type_string(evt->type), tevt->account_handle, tevt->first_id, tevt->first_index, tevt->last_id, tevt->count);
            }
            break;
        }
        case evt_account_send_message_failed:
        {
            vx_evt_account_send_message_failed *tevt = (vx_evt_account_send_message_failed *)evt;
            con_print("\r * %s [%s] Directed message send failed for '%s': '%s' (%d)\n", vx_get_event_type_string(evt->type), tevt->account_handle, tevt->request_id, vx_get_error_string(tevt->status_code), tevt->status_code);
            break;
        }
        case evt_transcribed_message:
        {
            vx_evt_transcribed_message *tevt = (vx_evt_transcribed_message *)evt;
            con_print(
                "\r * %s: %s %s %s\n",
                vx_get_event_type_string(evt->type),
                tevt->session_handle,
                (tevt->is_current_user == 0) ? tevt->participant_uri : "Me",
                tevt->text);
            if (tevt->participant_displayname)
            {
                con_print(" *   participant_displayname: %s\n", tevt->participant_displayname);
            }
            break;
        }
        case evt_none:
        case evt_max:
        default:
            break;
        }
        con_print("[SDKSampleApp]: ");
    }
}

void SDKSampleApp::SetRenderDeviceIndexByAccountHandle(const std::string &accountHandle, int deviceIndex)
{
    lock_guard<mutex> lock(m_setRenderDeviceIndexMutex);
    m_setRenderDeviceIndexByAccountHandle[accountHandle] = deviceIndex;
}

void SDKSampleApp::SetCaptureDeviceIndexByAccountHandle(const std::string &accountHandle, int deviceIndex)
{
    lock_guard<mutex> lock(m_setCaptureDeviceIndexMutex);
    m_setCaptureDeviceIndexByAccountHandle[accountHandle] = deviceIndex;
}

static char escapeCharacter(char c)
{
    switch (c)
    {
    case 't':
        return '\t';
    case 'n':
        return '\n';
    case 'r':
        return '\r';
    default:
        return c;
    }
}

std::vector<std::string> SDKSampleApp::splitCmdLine(const std::string &s)
{
    std::vector<std::string> items;
    std::string currentString;
    bool quote = false;
    for (auto i = begin(s); i != end(s); ++i)
    {
        char c = *i;
        if (c == '\\')
        {
            ++i;
            if (i == end(s))
            {
                // escape character at the end of the string
                items.clear();
                break;
            }
            currentString.push_back(escapeCharacter(*i));
            continue;
        }
        if (c == '"')
        {
            quote = !quote;
            continue;
        }
        if (quote)
        {
            currentString.push_back(c);
            continue;
        }
        if (c == '\r')
        {
            continue;
        }
        if (c == '\n')
        {
            continue;
        }
        if (c == ' ')
        {
            if (currentString.empty())
            {
                continue;
            }
            items.push_back(currentString);
            currentString.clear();
        }
        else
        {
            currentString.push_back(c);
        }
    }
    if (!currentString.empty())
    {
        items.push_back(currentString);
    }
    return items;
}

void SDKSampleApp::TerminateListenerThread()
{
    if (!m_listenerThread)
    {
        return;
    }

    join_thread(m_listenerThread);
    m_listenerThread = NULL;
    m_listenerThreadId = static_cast<vxplatform::os_thread_id>(NULL);
}

void SDKSampleApp::TerminateTcpConsoleThread()
{
    if (!m_tcpConsoleThread)
    {
        return;
    }

    m_terminateConsoleThread = true;
    {
        vxplatform::Locker lock(&m_tcpConsoleThreadLock);
        if (INVALID_SOCKET != m_tcpConsoleServerSocket)
        {
            closesocket(m_tcpConsoleServerSocket);
            m_tcpConsoleServerSocket = INVALID_SOCKET;
        }
        if (INVALID_SOCKET != m_tcpConsoleClientSocket)
        {
            closesocket(m_tcpConsoleClientSocket);
            m_tcpConsoleClientSocket = INVALID_SOCKET;
        }
    }

    if (m_tcpConsoleThreadId != get_current_thread_id())
    {
        join_thread(m_tcpConsoleThread);
        m_tcpConsoleThread = NULL;
        m_tcpConsoleThreadId = static_cast<vxplatform::os_thread_id>(NULL);
    }
}

os_error_t SDKSampleApp::TcpConsoleThreadProcStatic(void *arg)
{
    SDKSampleApp *pThis = reinterpret_cast<SDKSampleApp *>(arg);
    return pThis->TcpConsoleThreadProc();
}

os_error_t SDKSampleApp::TcpConsoleThreadProc()
{
    int sock_result = 0;
    int sock_opt_value = 1;

    bool relisten;
    do
    {
        relisten = false;
        { // create socket
            vxplatform::Locker lock(&m_tcpConsoleThreadLock);
            if (m_terminateConsoleThread)
            {
                break;
            }

            m_tcpConsoleServerSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
            if (m_tcpConsoleServerSocket < 0)
            {
                con_print("Error: failed to create listening socket for TCP console: %s\n", GetSocketErrorString().c_str());
                m_tcpConsoleServerSocket = INVALID_SOCKET;
                break;
            }

            sock_result = setsockopt(m_tcpConsoleServerSocket, SOL_SOCKET, SO_REUSEADDR, (char *)&sock_opt_value, sizeof(sock_opt_value));
            if (sock_result != 0)
            {
                con_print("Error: setsockopt, can't enable SOL_SOCKET/SO_REUSEADDR: %s\n", GetSocketErrorString().c_str());
                // break; // will try to continue
            }

#if defined(SO_NOSIGPIPE)
            sock_opt_value = 1;
            sock_result = setsockopt(m_tcpConsoleServerSocket, SOL_SOCKET, SO_NOSIGPIPE, (void *)&sock_opt_value, sizeof(sock_opt_value));
            if (sock_result != 0)
            {
                con_print("Error: setsockopt, can't enable SOL_SOCKET/SO_NOSIGPIPE: %s\n", GetSocketErrorString().c_str());
                // break; // will try to continue
            }
#endif
        }

        sockaddr_in ip_addr = {0};
        ip_addr.sin_family = AF_INET;
        ip_addr.sin_port = htons(m_tcpControlPort);
        ip_addr.sin_addr.s_addr = htonl(INADDR_ANY);

        std::vector<std::string> ipAddresses;

        char buf[1024];
        if (GetHostName(buf, sizeof(buf)) != 0)
        {
            con_print("Error: failed to get host name: %s\n", GetSocketErrorString().c_str());
            break;
        }
        struct hostent *host = gethostbyname(buf);
        if (host == NULL)
        {
            con_print("Error: failed to get host by name: %s\n", GetSocketErrorString().c_str());
            break;
        }

        for (int i = 0; host->h_addr_list[i] != 0; ++i)
        {
            struct in_addr addr;
            memcpy(&addr, host->h_addr_list[i], sizeof(struct in_addr));
            std::string ip = inet_ntoa(addr);
            ipAddresses.push_back(ip);
        }

        { // lock for short operations
            vxplatform::Locker lock(&m_tcpConsoleThreadLock);
            if (m_terminateConsoleThread || m_tcpConsoleServerSocket == INVALID_SOCKET)
            {
                break;
            }

            sock_result = ::bind(m_tcpConsoleServerSocket, (sockaddr *)&ip_addr, sizeof(ip_addr));
            if (sock_result != 0)
            {
                con_print("Error: failed to bind the TCP console socket: %s\n", GetSocketErrorString().c_str());
                break;
            }

            sock_result = listen(m_tcpConsoleServerSocket, 1);
            if (sock_result != 0)
            {
                con_print("Error: failed to listen on the TCP console socket: %s\n", GetSocketErrorString().c_str());
                break;
            }
        }

        std::stringstream ss;
        ss << TELNET_INFO_HEADER;
        ss << "UI is available via telnet on ";
        if (ipAddresses.empty())
        {
            ss << "{console IP}:" << m_tcpControlPort;
        }
        else
        {
            bool first = true;
            for (const std::string &ipAddress : ipAddresses)
            {
                if (first)
                {
                    ss << ipAddress.c_str() << ":" << m_tcpControlPort;
                    first = false;
                }
                else
                {
                    ss << ", " << ipAddress.c_str() << ":" << m_tcpControlPort;
                }
            }
        }
        ss << std::endl;
        ss << TELNET_INFO;
        ss << TELNET_INFO_FOOTER;
        con_print("%s", ss.str().c_str());
        vxplatform::set_event(listenerEvent);

        // const char *prompt = config.use_access_tokens ? "[SDKSampleApp use_access_tokens]: " : "[SDKSampleApp]: ";
        const char prompt[] = "[SDKSampleApp]: ";
        /**/

        bool showInfo = true;
        while (true)
        {
            if (m_tcpConsoleClientSocket == INVALID_SOCKET)
            {
                showInfo = true;

                if (m_terminateConsoleThread)
                {
                    break;
                }

                socket_t client_socket = accept(m_tcpConsoleServerSocket, (sockaddr *)nullptr, (socklen_t *)nullptr);
                if (m_terminateConsoleThread)
                {
                    closesocket(client_socket);
                    client_socket = INVALID_SOCKET;
                    break;
                }

                if (client_socket < 0)
                {
                    con_print("Error: failed to accept on the TCP console socket: %s\n", GetSocketErrorString().c_str());
                    relisten = true;
                    break;
                }

                { // set client socket
                    vxplatform::Locker lock(&m_tcpConsoleThreadLock);
                    if (m_terminateConsoleThread)
                    {
                        break;
                    }
                    m_tcpConsoleClientSocket = client_socket;
#if SO_NOSIGPIPE
                    sock_opt_value = 1;
                    sock_result = setsockopt(m_tcpConsoleClientSocket, SOL_SOCKET, SO_NOSIGPIPE, (void *)&sock_opt_value, sizeof(sock_opt_value));
                    if (sock_result != 0)
                    {
                        con_print("Error: setsockopt, can't enable SOL_SOCKET/SO_NOSIGPIPE: %s\n", GetSocketErrorString().c_str());
                    }
#endif
                }
            }
            else
            {
                if (showInfo)
                {
                    showInfo = false;
                    std::stringstream ss2;
                    ss2 << getVersionAndCopyrightText();
                    ss2 << TELNET_INFO_HEADER;
                    ss2 << TELNET_INFO;
                    ss2 << TELNET_INFO_FOOTER;
                    send(m_tcpConsoleClientSocket, ss2.str().c_str(), static_cast<int>(ss2.str().size()), 0);
                    if (m_terminateConsoleThread)
                    {
                        break;
                    }
                }

                send(m_tcpConsoleClientSocket, prompt, sizeof(prompt), 0);
                if (m_terminateConsoleThread)
                {
                    break;
                }

                std::string tmpLine;
                tmpLine.resize(0x4000);
                char *p = &tmpLine[0];
                int nread = 0;
                int remaining = 0x4000;
                while (remaining > 0)
                {
                    int n = (int)recv(m_tcpConsoleClientSocket, p, remaining, 0);
                    if (n <= 0)
                    {
                        nread = n;
                        break;
                    }
                    nread += n;
                    p += n;
                    remaining -= n;
                    if (p[-1] == '\n' || p[-1] == '\x04' /*Ctrl+D*/)
                    {
                        break;
                    }
                }
                if (nread <= 0 || p[-1] == '\x04')
                {
                    vxplatform::Locker lock(&m_tcpConsoleThreadLock);
                    if (m_terminateConsoleThread)
                    {
                        break;
                    }
                    closesocket(m_tcpConsoleClientSocket);
                    m_tcpConsoleClientSocket = INVALID_SOCKET;
                    continue;
                }
                tmpLine.resize(nread);
                std::vector<std::string> cmd = splitCmdLine(tmpLine);
                if (cmd.empty())
                {
                    continue;
                }
                /*
                if (cmd.at(0) == "q") {
                // can't quit remotely
                pThis->writeCout("You can't quit remotely.\n");
                }
                else {
                */
                if (!ProcessCommand(cmd))
                {
                    vxplatform::Locker lock(&m_tcpConsoleThreadLock);
                    if (m_terminateConsoleThread)
                    {
                        break;
                    }
                    closesocket(m_tcpConsoleClientSocket);
                    m_tcpConsoleClientSocket = INVALID_SOCKET;
                    Shutdown();
                }
                /*
                }
                */
            }
        }
        ipAddresses.clear();
    } while (relisten);

    { // close sockets
        vxplatform::Locker lock(&m_tcpConsoleThreadLock);
        if (m_tcpConsoleClientSocket != INVALID_SOCKET)
        {
            closesocket(m_tcpConsoleClientSocket);
            m_tcpConsoleClientSocket = INVALID_SOCKET;
        }

        if (m_tcpConsoleServerSocket != INVALID_SOCKET)
        {
            closesocket(m_tcpConsoleServerSocket);
            m_tcpConsoleServerSocket = INVALID_SOCKET;
        }
    }

    return 0;
}

//
// Pulls a message from the queue.  If no message found, waits until notified, then tries again.
// Type of the message, and then type of the Event or Response is discovered, and the
// appropriate call is made to process that message type.
//
void SDKSampleApp::ListenerThread()
{
    for (; m_started;)
    {
        wait_event(m_messageAvailableEvent, -1);
        for (;;)
        {
            vx_message_base_t *msg = NULL;
            vx_get_message(&msg);
            if (msg == NULL)
            {
                break;
            }
            Lock();
            HandleMessage(msg);
            Unlock();
            vx_destroy_message(msg);
        }
    }
    set_event(m_listenerThreadTerminatedEvent); //응답 수신 THREAD 종료
    //con_print("\n%s\n", "Cummunication Termination!");
    //vxplatform::set_event(listenerEvent);
}

string SDKSampleApp::GetNextRequestId()
{
    stringstream ss;
    ss << m_requestId++;
    return ss.str();
}

string SDKSampleApp::IssueRequest(vx_req_base_t *req, bool bSilent)
{
    string nextId = GetNextRequestId();
    safe_replace_string(&req->cookie, nextId.c_str());
    int request_count;
    if (!bSilent && m_logRequests)
    {
        con_print("\r * Issuing %s with cookie=%s\n", vx_get_request_type_string(req->type), nextId.c_str());
        if (m_logXml)
        {
            con_print("\r * %s\n", Xml(req).c_str());
        }
    }
    int error = vx_issue_request3(req, &request_count);
    if (error)
    {
        con_print("\r * Error: vx_issue_request3() returned error %s(%d) for request %s\n", vx_get_error_string(error), error, vx_get_request_type_string(req->type));
        return string();
    }
    return nextId;
}

unsigned long long SDKSampleApp::GetNextSerialNumber()
{
    static unsigned long long serial = 1;
    return serial++;
}

string SDKSampleApp::DebugGetAccessToken(const string &cmd, const string &subject, const string &from, const string &to)
{
    char *tmp = vx_debug_generate_token(
        m_accessTokenIssuer.c_str(),
        vx_time_t(-1),
        cmd.c_str(),
        GetNextSerialNumber(),
        subject.empty() ? NULL : GetUserUri(subject).c_str(),
        from.empty() ? NULL : GetUserUri(from).c_str(),
        to.empty() ? NULL : GetUserUri(to).c_str(),
        (const unsigned char *)m_accessTokenKey.c_str(),
        m_accessTokenKey.length());
    string token = tmp;
    vx_free(tmp);
    return token;
}

string SDKSampleApp::GetUserUriForAccountHandle(const string &accountHandle) const
{
    lock_guard<mutex> lock(m_accountHandleUserNameMutex);
    auto userName = m_accountHandleUserNames.find(accountHandle);
    if (userName == m_accountHandleUserNames.end() || userName->second.empty())
    {
        return string();
    }
    return string("sip:") + userName->second + "@" + m_realm;
}

string SDKSampleApp::GetUserUri(const string &nameOrUri)
{
    if (nameOrUri.find("sip:") == 0)
    {
        return nameOrUri;
    }
    string tmp = "sip:";
    tmp += nameOrUri;
    tmp += "@";
    tmp += m_realm;
    return tmp;
}

string SDKSampleApp::DefaultSessionGroupHandle()
{
    return m_SSGHandles.empty() ? string() : m_SSGHandles.front().first;
}

string SDKSampleApp::DefaultSessionHandle()
{
    return m_SSGHandles.empty() ? string() : m_SSGHandles.front().second;
}

bool SDKSampleApp::CheckExistsSessionGroupHandle(string sessionGroupHandle)
{
    vector<SSGPair>::const_iterator itr = std::find_if(m_SSGHandles.begin(), m_SSGHandles.end(), [&](const SSGPair &element)
                                                       { return element.first == sessionGroupHandle; });
    return itr != m_SSGHandles.end();
}

string SDKSampleApp::FindSessionGroupHandleBySessionHandle(string sessionHandle)
{
    vector<SSGPair>::const_iterator itr = std::find_if(m_SSGHandles.begin(), m_SSGHandles.end(), [&](const SSGPair &element)
                                                       { return element.second == sessionHandle; });
    if (itr == m_SSGHandles.end())
    {
        return string();
    }
    else
    {
        return itr->first;
    }
}

void SDKSampleApp::connector_create(const string &server, const string &handle, unsigned int configured_codecs)
{
    vx_req_connector_create_t *req;
    vx_req_connector_create_create(&req);
    safe_replace_string(&req->acct_mgmt_server, server.c_str());
    safe_replace_string(&req->connector_handle, handle.c_str());
#ifdef THIS_APP_UNIQUE_3_LETTERS_USER_AGENT_ID_STRING
    safe_replace_string(&req->user_agent_id, THIS_APP_UNIQUE_3_LETTERS_USER_AGENT_ID_STRING);
#endif
    if (configured_codecs)
    {
        req->configured_codecs = configured_codecs;
    }
    IssueRequest(&req->base);
}

void SDKSampleApp::account_anonymous_login(const string &user, const string &connectorHandle, const string &accountHandle, const string &displayName, int frequency, const string &langs, bool autoAcceptBuddies)
{
    vx_req_account_anonymous_login_t *req;
    vx_req_account_anonymous_login_create(&req);
    safe_replace_string(&req->access_token, DebugGetAccessToken("login", string(), user, string()).c_str());
    safe_replace_string(&req->account_handle, accountHandle.c_str());
    safe_replace_string(&req->connector_handle, connectorHandle.c_str());
    safe_replace_string(&req->acct_name, user.c_str());
    safe_replace_string(&req->displayname, displayName.c_str());
    req->enable_buddies_and_presence = 1;
    if (!autoAcceptBuddies)
    {
        // Cause evt_subscription events to occur when buddies are added
        req->buddy_management_mode = mode_application;
    }
    if (frequency != -1)
    {
        req->participant_property_frequency = frequency;
        m_logParticipantUpdate = true;
    }
    if (langs.empty())
    {
        req->languages = NULL;
    }
    else
    {
        safe_replace_string(&req->languages, langs.c_str());
    }
    IssueRequest(&req->base);
}

void SDKSampleApp::aux_get_capture_devices(const string &accountHandle)
{
    vx_req_aux_get_capture_devices_t *req;
    vx_req_aux_get_capture_devices_create(&req);
    if (!accountHandle.empty())
    {
        safe_replace_string(&req->account_handle, accountHandle.c_str());
    }
    IssueRequest(&req->base);
}

void SDKSampleApp::aux_set_capture_device(const std::string &accountHandle, const string &deviceId)
{
    vx_req_aux_set_capture_device_t *req;
    vx_req_aux_set_capture_device_create(&req);
    safe_replace_string(&req->capture_device_specifier, deviceId.c_str());
    if (!accountHandle.empty())
    {
        safe_replace_string(&req->account_handle, accountHandle.c_str());
    }
    IssueRequest(&req->base);
}

void SDKSampleApp::aux_get_render_devices(const string &accountHandle)
{
    vx_req_aux_get_render_devices_t *req;
    vx_req_aux_get_render_devices_create(&req);
    if (!accountHandle.empty())
    {
        safe_replace_string(&req->account_handle, accountHandle.c_str());
    }
    IssueRequest(&req->base);
}

void SDKSampleApp::aux_set_render_device(const std::string &accountHandle, const string &deviceId)
{
    vx_req_aux_set_render_device_t *req;
    vx_req_aux_set_render_device_create(&req);
    safe_replace_string(&req->render_device_specifier, deviceId.c_str());
    if (!accountHandle.empty())
    {
        safe_replace_string(&req->account_handle, accountHandle.c_str());
    }
    IssueRequest(&req->base);
}

void SDKSampleApp::connector_initiate_shutdown(const string &connectorHandle)
{
    vx_req_connector_initiate_shutdown_t *req;
    vx_req_connector_initiate_shutdown_create(&req);
    safe_replace_string(&req->connector_handle, connectorHandle.c_str());
    IssueRequest(&req->base);
}

void SDKSampleApp::connector_get_local_audio_info(const std::string &accountHandle, const string &connectorHandle)
{
    vx_req_connector_get_local_audio_info_t *req;
    vx_req_connector_get_local_audio_info_create(&req);
    safe_replace_string(&req->connector_handle, connectorHandle.c_str());
    if (!accountHandle.empty())
    {
        safe_replace_string(&req->account_handle, accountHandle.c_str());
    }
    IssueRequest(&req->base);
}

void SDKSampleApp::account_logout(const string &accountHandle)
{
    vx_req_account_logout_t *req;
    vx_req_account_logout_create(&req);
    safe_replace_string(&req->account_handle, accountHandle.c_str());
    IssueRequest(&req->base);
}

void SDKSampleApp::account_set_login_properties(const string &accountHandle, int frequency)
{
    vx_req_account_set_login_properties_t *req;
    vx_req_account_set_login_properties_create(&req);
    safe_replace_string(&req->account_handle, accountHandle.c_str());
    if (frequency != -1)
    {
        req->participant_property_frequency = frequency;
        m_logParticipantUpdate = true;
    }
    IssueRequest(&req->base);
}

void SDKSampleApp::sessiongroup_add_session(
    const string &accountHandle,
    const string &sessionGroupHandle,
    const string &sessionHandle,
    const string &channel,
    bool joinMuted,
    bool connectAudio,
    bool connectText)
{
    vx_req_sessiongroup_add_session_t *req;
    vx_req_sessiongroup_add_session_create(&req);
    safe_replace_string(&req->access_token, DebugGetAccessToken(joinMuted ? "join_muted" : "join", string(), GetUserUriForAccountHandle(accountHandle), channel).c_str());
    safe_replace_string(&req->account_handle, accountHandle.c_str());
    req->connect_audio = connectAudio ? 1 : 0;
    req->connect_text = connectText ? 1 : 0;
    safe_replace_string(&req->sessiongroup_handle, sessionGroupHandle.c_str());
    safe_replace_string(&req->session_handle, sessionHandle.c_str());
    safe_replace_string(&req->uri, channel.c_str());
    IssueRequest(&req->base);
}

void SDKSampleApp::sessiongroup_remove_session(const string &sessionGroupHandle, const string &sessionHandle)
{
    vx_req_sessiongroup_remove_session_t *req;
    vx_req_sessiongroup_remove_session_create(&req);
    safe_replace_string(&req->session_handle, sessionHandle.c_str());
    safe_replace_string(&req->sessiongroup_handle, sessionGroupHandle.c_str());
    IssueRequest(&req->base);
}

void SDKSampleApp::sessiongroup_set_focus(const string &sessionHandle)
{
    vx_req_sessiongroup_set_focus_t *req;
    vx_req_sessiongroup_set_focus_create(&req);
    safe_replace_string(&req->session_handle, sessionHandle.c_str());
    IssueRequest(&req->base);
}

void SDKSampleApp::sessiongroup_reset_focus(const string &sessionGroupHandle)
{
    vx_req_sessiongroup_reset_focus_t *req;
    vx_req_sessiongroup_reset_focus_create(&req);
    safe_replace_string(&req->sessiongroup_handle, sessionGroupHandle.c_str());
    IssueRequest(&req->base);
}

void SDKSampleApp::sessiongroup_set_tx_session(const string &sessionHandle)
{
    vx_req_sessiongroup_set_tx_session_t *req;
    vx_req_sessiongroup_set_tx_session_create(&req);
    safe_replace_string(&req->session_handle, sessionHandle.c_str());
    IssueRequest(&req->base);
}

void SDKSampleApp::sessiongroup_set_tx_all_sessions(const string &sessionGroupHandle)
{
    vx_req_sessiongroup_set_tx_all_sessions_t *req;
    vx_req_sessiongroup_set_tx_all_sessions_create(&req);
    safe_replace_string(&req->sessiongroup_handle, sessionGroupHandle.c_str());
    IssueRequest(&req->base);
}

void SDKSampleApp::sessiongroup_set_tx_no_session(const string &sessionGroupHandle)
{
    vx_req_sessiongroup_set_tx_no_session_t *req;
    vx_req_sessiongroup_set_tx_no_session_create(&req);
    safe_replace_string(&req->sessiongroup_handle, sessionGroupHandle.c_str());
    IssueRequest(&req->base);
}

void SDKSampleApp::session_media_connect(const string &sessionHandle)
{
    vx_req_session_media_connect_t *req;
    vx_req_session_media_connect_create(&req);
    safe_replace_string(&req->session_handle, sessionHandle.c_str());
    IssueRequest(&req->base);
}

void SDKSampleApp::session_media_disconnect(const string &sessionHandle)
{
    vx_req_session_media_disconnect_t *req;
    vx_req_session_media_disconnect_create(&req);
    safe_replace_string(&req->session_handle, sessionHandle.c_str());
    IssueRequest(&req->base);
}

void SDKSampleApp::session_mute_local_speaker(const string &sessionHandle, bool muted, vx_mute_scope scope)
{
    vx_req_session_mute_local_speaker_t *req;
    vx_req_session_mute_local_speaker_create(&req);
    safe_replace_string(&req->session_handle, sessionHandle.c_str());
    req->mute_level = muted ? 1 : 0;
    req->scope = scope;
    IssueRequest(&req->base);
}

void SDKSampleApp::session_set_local_speaker_volume(const string &sessionHandle, int level)
{
    vx_req_session_set_local_speaker_volume_t *req;
    vx_req_session_set_local_speaker_volume_create(&req);
    safe_replace_string(&req->session_handle, sessionHandle.c_str());
    req->volume = level;
    IssueRequest(&req->base);
}

void SDKSampleApp::session_set_local_render_volume(const string &sessionHandle, int level)
{
    vx_req_session_set_local_render_volume_t *req;
    vx_req_session_set_local_render_volume_create(&req);
    safe_replace_string(&req->session_handle, sessionHandle.c_str());
    req->volume = level;
    IssueRequest(&req->base);
}

void SDKSampleApp::session_set_participant_volume_for_me(const string &sessionHandle, const string &participant, int level)
{
    vx_req_session_set_participant_volume_for_me_t *req;
    vx_req_session_set_participant_volume_for_me_create(&req);
    safe_replace_string(&req->session_handle, sessionHandle.c_str());
    safe_replace_string(&req->participant_uri, participant.c_str());
    req->volume = level;
    IssueRequest(&req->base);
}

void SDKSampleApp::session_set_participant_mute_for_me(const string &sessionHandle, const string &participant, bool muted, vx_mute_scope scope)
{
    vx_req_session_set_participant_mute_for_me_t *req;
    vx_req_session_set_participant_mute_for_me_create(&req);
    safe_replace_string(&req->session_handle, sessionHandle.c_str());
    safe_replace_string(&req->participant_uri, participant.c_str());
    req->mute = muted ? 1 : 0;
    req->scope = scope;
    IssueRequest(&req->base);
}

void SDKSampleApp::session_set_3d_position(const string &sessionHandle, int x, int y, double radians)
{
    (void)radians;
    vx_req_session_set_3d_position_t *req;
    vx_req_session_set_3d_position_create(&req);
    safe_replace_string(&req->session_handle, sessionHandle.c_str());
    req->listener_position[0] = x;
    req->listener_position[1] = y;
    req->speaker_position[0] = x;
    req->speaker_position[1] = y;
    IssueRequest(&req->base);
}

void SDKSampleApp::channel_mute_user(const string &accountHandle, const string &channel, const string &participant, bool muted, vx_mute_scope scope)
{
    vx_req_channel_mute_user_t *req;
    vx_req_channel_mute_user_create(&req);
    safe_replace_string(&req->access_token, DebugGetAccessToken("mute", participant, GetUserUriForAccountHandle(accountHandle), channel).c_str());
    safe_replace_string(&req->account_handle, accountHandle.c_str());
    safe_replace_string(&req->channel_uri, channel.c_str());
    safe_replace_string(&req->participant_uri, participant.c_str());
    req->set_muted = muted ? 1 : 0;
    req->scope = scope;
    IssueRequest(&req->base);
}

void SDKSampleApp::channel_kick_user(const string &accountHandle, const string &channel, const string &participant)
{
    vx_req_channel_kick_user_t *req;
    vx_req_channel_kick_user_create(&req);
    safe_replace_string(&req->access_token, DebugGetAccessToken("kick", participant, GetUserUriForAccountHandle(accountHandle), channel).c_str());
    safe_replace_string(&req->account_handle, accountHandle.c_str());
    safe_replace_string(&req->channel_uri, channel.c_str());
    safe_replace_string(&req->participant_uri, participant.c_str());
    IssueRequest(&req->base);
}

void SDKSampleApp::channel_mute_all_users(const string &accountHandle, const string &channel, bool muted, vx_mute_scope scope)
{
    vx_req_channel_mute_all_users_t *req;
    vx_req_channel_mute_all_users_create(&req);
    safe_replace_string(&req->access_token, DebugGetAccessToken("mute", channel, GetUserUriForAccountHandle(accountHandle), channel).c_str());

    safe_replace_string(&req->account_handle, accountHandle.c_str());
    safe_replace_string(&req->channel_uri, channel.c_str());
    req->set_muted = muted ? 1 : 0;
    req->scope = scope;
    IssueRequest(&req->base);
}

void SDKSampleApp::connector_mute_local_mic(const std::string &accountHandle, bool muted)
{
    vx_req_connector_mute_local_mic_t *req;
    vx_req_connector_mute_local_mic_create(&req);
    req->mute_level = muted ? 1 : 0;
    if (!accountHandle.empty())
    {
        safe_replace_string(&req->account_handle, accountHandle.c_str());
    }
    IssueRequest(&req->base);
}

void SDKSampleApp::connector_mute_local_speaker(const std::string &accountHandle, bool muted)
{
    vx_req_connector_mute_local_speaker_t *req;
    vx_req_connector_mute_local_speaker_create(&req);
    req->mute_level = muted ? 1 : 0;
    if (!accountHandle.empty())
    {
        safe_replace_string(&req->account_handle, accountHandle.c_str());
    }
    IssueRequest(&req->base);
}

void SDKSampleApp::account_buddy_set(const string &accountHandle, const string &buddy)
{
    vx_req_account_buddy_set_t *req;
    vx_req_account_buddy_set_create(&req);
    safe_replace_string(&req->account_handle, accountHandle.c_str());
    safe_replace_string(&req->buddy_uri, buddy.c_str());
    IssueRequest(&req->base);
}

void SDKSampleApp::account_buddy_delete(const string &accountHandle, const string &buddy)
{
    vx_req_account_buddy_delete_t *req;
    vx_req_account_buddy_delete_create(&req);
    safe_replace_string(&req->account_handle, accountHandle.c_str());
    safe_replace_string(&req->buddy_uri, buddy.c_str());
    IssueRequest(&req->base);
}

void SDKSampleApp::account_list_buddies_and_groups(const string &accountHandle)
{
    vx_req_account_list_buddies_and_groups_t *req;
    vx_req_account_list_buddies_and_groups_create(&req);
    safe_replace_string(&req->account_handle, accountHandle.c_str());
    IssueRequest(&req->base);
}

void SDKSampleApp::session_send_message(const string &sessionHandle, const string &message, const string &customMetadataNS, const string &customMetadata)
{
    vx_req_session_send_message_t *req;
    vx_req_session_send_message_create(&req);
    safe_replace_string(&req->session_handle, sessionHandle.c_str());
    safe_replace_string(&req->message_body, message.c_str());
    safe_replace_string(&req->application_stanza_namespace, customMetadataNS.c_str());
    safe_replace_string(&req->application_stanza_body, customMetadata.c_str());
    IssueRequest(&req->base);
}

void SDKSampleApp::account_send_message(const string &accountHandle, const string &message, const string &user, const string &customMetadataNS, const string &customMetadata)
{
    vx_req_account_send_message_t *req;
    vx_req_account_send_message_create(&req);
    safe_replace_string(&req->account_handle, accountHandle.c_str());
    safe_replace_string(&req->message_body, message.c_str());
    safe_replace_string(&req->user_uri, user.c_str());
    safe_replace_string(&req->application_stanza_namespace, customMetadataNS.c_str());
    safe_replace_string(&req->application_stanza_body, customMetadata.c_str());
    IssueRequest(&req->base);
}

void SDKSampleApp::session_archive_messages(
    const string &sessionHandle,
    unsigned int max,
    const string &time_start,
    const string &time_end,
    const string &text,
    const string &before,
    const string &after,
    int index,
    const string &user)
{
    vx_req_session_archive_query_t *req;
    vx_req_session_archive_query_create(&req);
    safe_replace_string(&req->session_handle, sessionHandle.c_str());
    req->max = max;
    safe_replace_string(&req->time_start, time_start.c_str());
    safe_replace_string(&req->time_end, time_end.c_str());
    safe_replace_string(&req->search_text, text.c_str());
    safe_replace_string(&req->after_id, after.c_str());
    safe_replace_string(&req->before_id, before.c_str());
    req->first_message_index = index;
    safe_replace_string(&req->participant_uri, user.c_str());
    IssueRequest(&req->base);
}

void SDKSampleApp::account_archive_query(
    const string &accountHandle,
    unsigned int max,
    const string &time_start,
    const string &time_end,
    const string &text,
    const string &before,
    const string &after,
    int index,
    const string &channel,
    const string &user)
{
    vx_req_account_archive_query_t *req;
    vx_req_account_archive_query_create(&req);
    safe_replace_string(&req->account_handle, accountHandle.c_str());
    req->max = max;
    safe_replace_string(&req->time_start, time_start.c_str());
    safe_replace_string(&req->time_end, time_end.c_str());
    safe_replace_string(&req->search_text, text.c_str());
    safe_replace_string(&req->after_id, after.c_str());
    safe_replace_string(&req->before_id, before.c_str());
    req->first_message_index = index;
    safe_replace_string(&req->channel_uri, channel.c_str());
    safe_replace_string(&req->participant_uri, user.c_str());
    IssueRequest(&req->base);
}

void SDKSampleApp::account_set_presence(const string &accountHandle, vx_buddy_presence_state presence, const string &message)
{
    vx_req_account_set_presence_t *req;
    vx_req_account_set_presence_create(&req);
    safe_replace_string(&req->account_handle, accountHandle.c_str());
    safe_replace_string(&req->custom_message, message.c_str());
    req->presence = presence;
    IssueRequest(&req->base);
}

void SDKSampleApp::account_send_subscription_reply(const string &accountHandle, const string &buddy, bool allow)
{
    vx_req_account_send_subscription_reply_t *req;
    vx_req_account_send_subscription_reply_create(&req);
    safe_replace_string(&req->account_handle, accountHandle.c_str());
    safe_replace_string(&req->buddy_uri, buddy.c_str());
    req->rule_type = allow ? vx_rule_type::rule_allow : vx_rule_type::rule_block;
    // TODO: Subscription Handle
    IssueRequest(&req->base);
}

void SDKSampleApp::session_send_notification(const string &sessionHandle, bool typing)
{
    vx_req_session_send_notification_t *req;
    vx_req_session_send_notification_create(&req);
    safe_replace_string(&req->session_handle, sessionHandle.c_str());
    req->notification_type = typing ? vx_notification_type::notification_typing : vx_notification_type::notification_not_typing;
    IssueRequest(&req->base);
}

void SDKSampleApp::account_create_block_rule(const string &accountHandle, const string &otherUser)
{
    vx_req_account_create_block_rule_t *req;
    vx_req_account_create_block_rule_create(&req);
    safe_replace_string(&req->account_handle, accountHandle.c_str());
    safe_replace_string(&req->block_mask, otherUser.c_str());
    IssueRequest(&req->base);
}

void SDKSampleApp::account_delete_block_rule(const string &accountHandle, const string &otherUser)
{
    vx_req_account_delete_block_rule_t *req;
    vx_req_account_delete_block_rule_create(&req);
    safe_replace_string(&req->account_handle, accountHandle.c_str());
    safe_replace_string(&req->block_mask, otherUser.c_str());
    IssueRequest(&req->base);
}

void SDKSampleApp::account_list_block_rules(const string &accountHandle)
{
    vx_req_account_list_block_rules_t *req;
    vx_req_account_list_block_rules_create(&req);
    safe_replace_string(&req->account_handle, accountHandle.c_str());
    IssueRequest(&req->base);
}

void SDKSampleApp::account_create_auto_accept_rule(const string &accountHandle, const string &otherUser)
{
    vx_req_account_create_auto_accept_rule_t *req;
    vx_req_account_create_auto_accept_rule_create(&req);
    safe_replace_string(&req->account_handle, accountHandle.c_str());
    safe_replace_string(&req->auto_accept_mask, otherUser.c_str());
    IssueRequest(&req->base);
}

void SDKSampleApp::account_delete_auto_accept_rule(const string &accountHandle, const string &otherUser)
{
    vx_req_account_delete_auto_accept_rule_t *req;
    vx_req_account_delete_auto_accept_rule_create(&req);
    safe_replace_string(&req->account_handle, accountHandle.c_str());
    safe_replace_string(&req->auto_accept_mask, otherUser.c_str());
    IssueRequest(&req->base);
}

void SDKSampleApp::account_list_auto_accept_rules(const string &accountHandle)
{
    vx_req_account_list_auto_accept_rules_t *req;
    vx_req_account_list_auto_accept_rules_create(&req);
    safe_replace_string(&req->account_handle, accountHandle.c_str());
    IssueRequest(&req->base);
}

void SDKSampleApp::aux_get_mic_level(const std::string &accountHandle)
{
    vx_req_aux_get_mic_level_t *req;
    vx_req_aux_get_mic_level_create(&req);
    if (!accountHandle.empty())
    {
        safe_replace_string(&req->account_handle, accountHandle.c_str());
    }
    IssueRequest(&req->base);
}

void SDKSampleApp::aux_get_speaker_level(const std::string &accountHandle)
{
    vx_req_aux_get_speaker_level_t *req;
    vx_req_aux_get_speaker_level_create(&req);
    if (!accountHandle.empty())
    {
        safe_replace_string(&req->account_handle, accountHandle.c_str());
    }
    IssueRequest(&req->base);
}

void SDKSampleApp::aux_set_mic_level(const std::string &accountHandle, int level)
{
    vx_req_aux_set_mic_level_t *req;
    vx_req_aux_set_mic_level_create(&req);
    req->level = level;
    if (!accountHandle.empty())
    {
        safe_replace_string(&req->account_handle, accountHandle.c_str());
    }
    IssueRequest(&req->base);
}

void SDKSampleApp::aux_set_speaker_level(const std::string &accountHandle, int level)
{
    vx_req_aux_set_speaker_level_t *req;
    vx_req_aux_set_speaker_level_create(&req);
    req->level = level;
    if (!accountHandle.empty())
    {
        safe_replace_string(&req->account_handle, accountHandle.c_str());
    }
    IssueRequest(&req->base);
}

void SDKSampleApp::aux_render_audio_start(const std::string &accountHandle, const string &filename, bool loop)
{
    vx_req_aux_render_audio_start_t *req;
    vx_req_aux_render_audio_start_create(&req);
    safe_replace_string(&req->sound_file_path, filename.c_str());
    req->loop = loop ? 1 : 0;
    if (!accountHandle.empty())
    {
        safe_replace_string(&req->account_handle, accountHandle.c_str());
    }
    IssueRequest(&req->base);
}

void SDKSampleApp::aux_render_audio_stop(const std::string &accountHandle)
{
    vx_req_aux_render_audio_stop_t *req;
    vx_req_aux_render_audio_stop_create(&req);
    if (!accountHandle.empty())
    {
        safe_replace_string(&req->account_handle, accountHandle.c_str());
    }
    IssueRequest(&req->base);
}

void SDKSampleApp::aux_capture_audio_start(const std::string &accountHandle)
{
    vx_req_aux_capture_audio_start_t *req;
    vx_req_aux_capture_audio_start_create(&req);
    req->loop_to_render_device = 1;
    if (!accountHandle.empty())
    {
        safe_replace_string(&req->account_handle, accountHandle.c_str());
    }
    IssueRequest(&req->base);
}

void SDKSampleApp::aux_capture_audio_stop(const std::string &accountHandle)
{
    vx_req_aux_capture_audio_stop_t *req;
    vx_req_aux_capture_audio_stop_create(&req);
    if (!accountHandle.empty())
    {
        safe_replace_string(&req->account_handle, accountHandle.c_str());
    }
    IssueRequest(&req->base);
}

void SDKSampleApp::sessiongroup_set_session_3d_position(const string &sessionHandle, int x, int y)
{
    vx_req_sessiongroup_set_session_3d_position_t *req;
    vx_req_sessiongroup_set_session_3d_position_create(&req);
    safe_replace_string(&req->session_handle, sessionHandle.c_str());
    req->speaker_position[0] = x;
    req->speaker_position[1] = y;
    IssueRequest(&req->base);
}

void SDKSampleApp::aux_start_buffer_capture(const std::string &accountHandle)
{
    vx_req_aux_start_buffer_capture_t *req;
    vx_req_aux_start_buffer_capture_create(&req);
    if (!accountHandle.empty())
    {
        safe_replace_string(&req->account_handle, accountHandle.c_str());
    }
    IssueRequest(&req->base);
}

void SDKSampleApp::aux_play_audio_buffer(const std::string &accountHandle)
{
    vx_req_aux_play_audio_buffer_t *req;
    vx_req_aux_play_audio_buffer_create(&req);
    if (!accountHandle.empty())
    {
        safe_replace_string(&req->account_handle, accountHandle.c_str());
    }
    IssueRequest(&req->base);
}

void SDKSampleApp::session_text_connect(const string &sessionHandle)
{
    vx_req_session_text_connect_t *req;
    vx_req_session_text_connect_create(&req);
    safe_replace_string(&req->session_handle, sessionHandle.c_str());
    IssueRequest(&req->base);
}

void SDKSampleApp::session_text_disconnect(const string &sessionHandle)
{
    vx_req_session_text_disconnect_t *req;
    vx_req_session_text_disconnect_create(&req);
    safe_replace_string(&req->session_handle, sessionHandle.c_str());
    IssueRequest(&req->base);
}

void SDKSampleApp::aux_set_vad_properties(const std::string &accountHandle, int hangover, int sensitivity, int noiseFloor, int isAuto)
{
    vx_req_aux_set_vad_properties_t *req;
    vx_req_aux_set_vad_properties_create(&req);
    req->vad_hangover = hangover;
    req->vad_sensitivity = sensitivity;
    req->vad_noise_floor = noiseFloor;
    req->vad_auto = isAuto;
    if (!accountHandle.empty())
    {
        safe_replace_string(&req->account_handle, accountHandle.c_str());
    }
    IssueRequest(&req->base);
}

void SDKSampleApp::aux_get_vad_properties(const std::string &accountHandle)
{
    vx_req_aux_get_vad_properties_t *req;
    vx_req_aux_get_vad_properties_create(&req);
    if (!accountHandle.empty())
    {
        safe_replace_string(&req->account_handle, accountHandle.c_str());
    }
    IssueRequest(&req->base);
}

void SDKSampleApp::aux_get_derumbler_properties(const std::string &accountHandle)
{
    vx_req_aux_get_derumbler_properties_t *req;
    vx_req_aux_get_derumbler_properties_create(&req);
    if (!accountHandle.empty())
    {
        safe_replace_string(&req->account_handle, accountHandle.c_str());
    }
    IssueRequest(&req->base);
}

void SDKSampleApp::aux_set_derumbler_properties(const std::string &accountHandle, int enabled, int stopband_corner_frequency)
{
    vx_req_aux_set_derumbler_properties_t *req;
    vx_req_aux_set_derumbler_properties_create(&req);
    req->enabled = enabled;
    req->stopband_corner_frequency = stopband_corner_frequency;
    if (!accountHandle.empty())
    {
        safe_replace_string(&req->account_handle, accountHandle.c_str());
    }
    IssueRequest(&req->base);
}

void SDKSampleApp::sessiongroup_inject_audio_start(const string &sessionGroupHandle, const string &file)
{
    vx_req_sessiongroup_control_audio_injection_t *req;
    vx_req_sessiongroup_control_audio_injection_create(&req);
    safe_replace_string(&req->sessiongroup_handle, sessionGroupHandle.c_str());
    safe_replace_string(&req->filename, file.c_str());
    req->audio_injection_control_type = VX_SESSIONGROUP_AUDIO_INJECTION_CONTROL_START;
    IssueRequest(&req->base);
}

void SDKSampleApp::sessiongroup_inject_audio_stop(const string &sessionGroupHandle)
{
    vx_req_sessiongroup_control_audio_injection_t *req;
    vx_req_sessiongroup_control_audio_injection_create(&req);
    safe_replace_string(&req->sessiongroup_handle, sessionGroupHandle.c_str());
    req->audio_injection_control_type = VX_SESSIONGROUP_AUDIO_INJECTION_CONTROL_STOP;
    IssueRequest(&req->base);
}

void SDKSampleApp::sessiongroup_get_stats(const string &sessionGroupHandle)
{
    vx_req_sessiongroup_get_stats_t *req;
    vx_req_sessiongroup_get_stats_create(&req);
    safe_replace_string(&req->sessiongroup_handle, sessionGroupHandle.c_str());
    IssueRequest(&req->base);
}

void SDKSampleApp::account_blockusers(const string &accountHandle, const list<string> &otherUsers, vx_control_communications_operation operation)
{
    vx_req_account_control_communications_t *req;
    vx_req_account_control_communications_create(&req);
    safe_replace_string(&req->account_handle, accountHandle.c_str());
    req->operation = operation;
    stringstream ss;
    for (list<std::string>::const_iterator i = otherUsers.begin(); i != otherUsers.end(); ++i)
    {
        ss << (*i) << "\n";
    }
    safe_replace_string(&req->user_uris, ss.str().c_str());
    IssueRequest(&req->base);
}

void SDKSampleApp::session_transcription_control(const string &sessionHandle, const string &channel, bool enable)
{
    vx_req_session_transcription_control_t *req;
    vx_req_session_transcription_control_create(&req);
    safe_replace_string(&req->session_handle, sessionHandle.c_str());
    req->enable = enable ? 1 : 0;

    string accountHandle;
    auto it = m_sessions.find(sessionHandle);
    if (it != m_sessions.end())
    {
        accountHandle = it->second->GetAccountHandle();
    }
    safe_replace_string(&req->access_token, DebugGetAccessToken("trxn", string(), GetUserUriForAccountHandle(accountHandle), channel).c_str());
    IssueRequest(&req->base);
}

void SDKSampleApp::req_move_origin(const string &sessionHandle)
{
    SampleAppOrientation listenerOrientation;
    SampleAppPosition listenerPosition;

    GetListenerOrientation(sessionHandle, listenerOrientation);

    listenerPosition = {0.0, 0.0, 0.0};

    vx_req_session_set_3d_position *req;
    vx_req_session_set_3d_position_create(&req);
    safe_replace_string(&req->session_handle, sessionHandle.c_str());
    Set3DPositionRequestFields(req, listenerPosition, listenerOrientation);

    SetListenerPosition(sessionHandle, listenerPosition);

    IssueRequest(&req->base);
}

void SDKSampleApp::req_move_forward(const string &sessionHandle, double delta)
{
    SampleAppPosition listenerPosition;
    SampleAppOrientation listenerOrientation;
    double delta_x = 0.0;
    double delta_z = 0.0;

    GetListenerPosition(sessionHandle, listenerPosition);
    GetListenerOrientation(sessionHandle, listenerOrientation);

    delta_x = delta * listenerOrientation.at_x;
    delta_z = delta * listenerOrientation.at_z;
    listenerPosition.x += delta_x;
    listenerPosition.z += delta_z;

    vx_req_session_set_3d_position *req;
    vx_req_session_set_3d_position_create(&req);
    safe_replace_string(&req->session_handle, sessionHandle.c_str());
    Set3DPositionRequestFields(req, listenerPosition, listenerOrientation);

    SetListenerPosition(sessionHandle, listenerPosition);

    IssueRequest(&req->base);
}

void SDKSampleApp::req_move_back(const string &sessionHandle, double delta)
{
    SampleAppPosition listenerPosition;
    SampleAppOrientation listenerOrientation;
    double delta_x = 0.0;
    double delta_z = 0.0;

    GetListenerPosition(sessionHandle, listenerPosition);
    GetListenerOrientation(sessionHandle, listenerOrientation);

    delta_x = -1.0 * delta * listenerOrientation.at_x;
    delta_z = -1.0 * delta * listenerOrientation.at_z;
    listenerPosition.x += delta_x;
    listenerPosition.z += delta_z;

    vx_req_session_set_3d_position *req;
    vx_req_session_set_3d_position_create(&req);
    safe_replace_string(&req->session_handle, sessionHandle.c_str());
    Set3DPositionRequestFields(req, listenerPosition, listenerOrientation);

    SetListenerPosition(sessionHandle, listenerPosition);

    IssueRequest(&req->base);
}

void SDKSampleApp::req_move_left(const string &sessionHandle, double delta)
{
    SampleAppPosition listenerPosition;
    SampleAppOrientation listenerOrientation;
    double delta_x = 0.0;
    double delta_z = 0.0;

    GetListenerPosition(sessionHandle, listenerPosition);
    GetListenerOrientation(sessionHandle, listenerOrientation);

    delta_x = delta * listenerOrientation.at_z;
    delta_z = -1.0 * delta * listenerOrientation.at_x;
    listenerPosition.x += delta_x;
    listenerPosition.z += delta_z;

    vx_req_session_set_3d_position *req;
    vx_req_session_set_3d_position_create(&req);
    safe_replace_string(&req->session_handle, sessionHandle.c_str());
    Set3DPositionRequestFields(req, listenerPosition, listenerOrientation);

    SetListenerPosition(sessionHandle, listenerPosition);

    IssueRequest(&req->base);
}

void SDKSampleApp::req_move_right(const string &sessionHandle, double delta)
{
    SampleAppPosition listenerPosition;
    SampleAppOrientation listenerOrientation;
    double delta_x = 0.0;
    double delta_z = 0.0;

    GetListenerPosition(sessionHandle, listenerPosition);
    GetListenerOrientation(sessionHandle, listenerOrientation);

    delta_x = -1.0 * delta * listenerOrientation.at_z;
    delta_z = delta * listenerOrientation.at_x;
    listenerPosition.x += delta_x;
    listenerPosition.z += delta_z;

    vx_req_session_set_3d_position *req;
    vx_req_session_set_3d_position_create(&req);
    safe_replace_string(&req->session_handle, sessionHandle.c_str());
    Set3DPositionRequestFields(req, listenerPosition, listenerOrientation);

    SetListenerPosition(sessionHandle, listenerPosition);

    IssueRequest(&req->base);
}

void SDKSampleApp::req_move_up(const string &sessionHandle, double delta)
{
    SampleAppPosition listenerPosition;
    SampleAppOrientation listenerOrientation;

    GetListenerPosition(sessionHandle, listenerPosition);
    GetListenerOrientation(sessionHandle, listenerOrientation);

    listenerPosition.y += delta;

    vx_req_session_set_3d_position *req;
    vx_req_session_set_3d_position_create(&req);
    safe_replace_string(&req->session_handle, sessionHandle.c_str());
    Set3DPositionRequestFields(req, listenerPosition, listenerOrientation);

    SetListenerPosition(sessionHandle, listenerPosition);

    IssueRequest(&req->base);
}

void SDKSampleApp::req_move_down(const string &sessionHandle, double delta)
{
    SampleAppPosition listenerPosition;
    SampleAppOrientation listenerOrientation;

    GetListenerPosition(sessionHandle, listenerPosition);
    GetListenerOrientation(sessionHandle, listenerOrientation);

    listenerPosition.y -= delta;

    vx_req_session_set_3d_position *req;
    vx_req_session_set_3d_position_create(&req);
    safe_replace_string(&req->session_handle, sessionHandle.c_str());
    Set3DPositionRequestFields(req, listenerPosition, listenerOrientation);

    SetListenerPosition(sessionHandle, listenerPosition);

    IssueRequest(&req->base);
}

void SDKSampleApp::req_turn_left(const string &sessionHandle, double delta)
{
    double listenerHeadingDegrees;
    SampleAppPosition listenerPosition;
    SampleAppOrientation listenerOrientation;

    GetListenerHeadingDegrees(sessionHandle, listenerHeadingDegrees);
    GetListenerPosition(sessionHandle, listenerPosition);
    GetListenerOrientation(sessionHandle, listenerOrientation);

    listenerHeadingDegrees -= delta;
    if (listenerHeadingDegrees < 0.0)
    {
        listenerHeadingDegrees += 360.0;
    }
    listenerOrientation.at_x = 1.0 * sin(2 * SDK_SAMPLE_APP_PI * (listenerHeadingDegrees / 360.0));
    listenerOrientation.at_z = -1.0 * cos(2 * SDK_SAMPLE_APP_PI * (listenerHeadingDegrees / 360.0));

    vx_req_session_set_3d_position_t *req;
    vx_req_session_set_3d_position_create(&req);
    safe_replace_string(&req->session_handle, sessionHandle.c_str());
    Set3DPositionRequestFields(req, listenerPosition, listenerOrientation);

    SetListenerHeadingDegrees(sessionHandle, listenerHeadingDegrees);
    SetListenerOrientation(sessionHandle, listenerOrientation);

    IssueRequest(&req->base);
}

void SDKSampleApp::req_turn_right(const string &sessionHandle, double delta)
{
    double listenerHeadingDegrees;
    SampleAppPosition listenerPosition;
    SampleAppOrientation listenerOrientation;

    GetListenerHeadingDegrees(sessionHandle, listenerHeadingDegrees);
    GetListenerPosition(sessionHandle, listenerPosition);
    GetListenerOrientation(sessionHandle, listenerOrientation);

    listenerHeadingDegrees += delta;
    if (listenerHeadingDegrees >= 360.0)
    {
        listenerHeadingDegrees -= 360.0;
    }
    listenerOrientation.at_x = 1.0 * sin(2 * SDK_SAMPLE_APP_PI * (listenerHeadingDegrees / 360.0));
    listenerOrientation.at_z = -1.0 * cos(2 * SDK_SAMPLE_APP_PI * (listenerHeadingDegrees / 360.0));

    vx_req_session_set_3d_position_t *req;
    vx_req_session_set_3d_position_create(&req);
    safe_replace_string(&req->session_handle, sessionHandle.c_str());
    Set3DPositionRequestFields(req, listenerPosition, listenerOrientation);

    SetListenerHeadingDegrees(sessionHandle, listenerHeadingDegrees);
    SetListenerOrientation(sessionHandle, listenerOrientation);

    IssueRequest(&req->base);
}

SDKSampleApp::Session::Dancer::Dancer()
{
    m_x0 = 0;
    m_y0 = 0;
    m_z0 = 0;
    m_rmin = 0;
    m_rmax = 0;
    m_angularVelocityDegPerSec = 0;
    m_oscillationPeriodSeconds = 0;
    m_updateMilliseconds = 0;
    m_dancerThread = NULL;
    m_dancerThreadTerminatedEvent = NULL;
    m_lock = new vxplatform::Lock();
    m_threadRunning = false;
    m_dancerThreadStartedEvent = NULL;
    m_dancerThreadTerminatedEvent = NULL;
    m_dancerThreadStopEvent = NULL;
}

SDKSampleApp::Session::Dancer::~Dancer()
{
    Stop();
    delete m_lock;
    m_lock = NULL;
}

void SDKSampleApp::Session::Dancer::Start(
    const string &sessionHandle,
    double x0,
    double y0,
    double z0,
    double rmin,
    double rmax,
    double angularVelocityDegPerSec,
    double oscillationPeriodSeconds,
    int updateMilliseconds)
{
    Stop();
    m_sessionHandle = sessionHandle;
    m_x0 = x0;
    m_y0 = y0;
    m_z0 = z0;
    m_rmin = rmin;
    m_rmax = rmax;
    m_angularVelocityDegPerSec = angularVelocityDegPerSec;
    m_oscillationPeriodSeconds = oscillationPeriodSeconds;
    m_updateMilliseconds = updateMilliseconds;

    // FIXME: add error handling
    // ...
    assert(NULL == m_dancerThreadStartedEvent);
    assert(NULL == m_dancerThreadTerminatedEvent);
    assert(NULL == m_dancerThreadStopEvent);

    m_threadRunning = true;
    create_event(&m_dancerThreadStartedEvent);
    create_event(&m_dancerThreadTerminatedEvent);
    create_event(&m_dancerThreadStopEvent);
    create_thread(&SDKSampleApp::Session::Dancer::DancerThread, this, &m_dancerThread);
    wait_event(m_dancerThreadStartedEvent);
}

void SDKSampleApp::Session::Dancer::Stop()
{
    if (m_threadRunning)
    {
        set_event(m_dancerThreadStopEvent);
        wait_event(m_dancerThreadTerminatedEvent);
        assert(!m_threadRunning);
    }
    if (m_dancerThread)
    {
        delete_thread(m_dancerThread);
        m_dancerThread = NULL;
    }
    if (m_dancerThreadStartedEvent)
    {
        delete_event(m_dancerThreadStartedEvent);
        m_dancerThreadStartedEvent = NULL;
    }
    if (m_dancerThreadTerminatedEvent)
    {
        delete_event(m_dancerThreadTerminatedEvent);
        m_dancerThreadTerminatedEvent = NULL;
    }
    if (m_dancerThreadStopEvent)
    {
        delete_event(m_dancerThreadStopEvent);
        m_dancerThreadStopEvent = NULL;
    }
}

// static
double SDKSampleApp::Session::Dancer::GetMilliseconds()
{
    return get_millisecond_tick_counter();
}

// static
vxplatform::os_error_t SDKSampleApp::Session::Dancer::DancerThread(void *arg)
{
    Dancer *pThis = reinterpret_cast<SDKSampleApp::Session::Dancer *>(arg);
    pThis->DancerThread();
    return 0;
}

void SDKSampleApp::Session::Dancer::DancerThread()
{
    set_event(m_dancerThreadStartedEvent);

    double amp = (m_rmax - m_rmin) / 2;
    double r0 = (m_rmax + m_rmin) / 2;
    double t0 = GetMilliseconds();
    double t = 0;

    // SDKSampleApp::GetApp()->req_move_origin(m_sessionHandle);

    do
    {
        t = GetMilliseconds() - t0;
        // Set 3D position
        SampleAppPosition listenerPosition;
        SampleAppOrientation listenerOrientation;

        SDKSampleApp::GetApp()->GetListenerOrientation(m_sessionHandle, listenerOrientation);

        double anglePhi = 2 * SDK_SAMPLE_APP_PI * m_angularVelocityDegPerSec / 360 * t / 1000;
        double cosOmega = cos(2 * SDK_SAMPLE_APP_PI * t / 1000 / m_oscillationPeriodSeconds);

        listenerPosition.x = m_x0 + (r0 - amp * cosOmega) * sin(anglePhi);
        listenerPosition.y = m_y0;
        listenerPosition.z = m_z0 - (r0 - amp * cosOmega) * cos(anglePhi);

        vx_req_session_set_3d_position *req;
        vx_req_session_set_3d_position_create(&req);
        safe_replace_string(&req->session_handle, m_sessionHandle.c_str());
        req->req_disposition_type = req_disposition_no_reply_required;
        SDKSampleApp::GetApp()->Set3DPositionRequestFields(req, listenerPosition, listenerOrientation);
        SDKSampleApp::GetApp()->SetListenerPosition(m_sessionHandle, listenerPosition);
        SDKSampleApp::GetApp()->IssueRequest(&req->base, true);

        t = GetMilliseconds() - t0;
        double tEnd = (floor(t / m_updateMilliseconds) + 1) * m_updateMilliseconds;
        int delay = (int)(t - tEnd);
        if (delay <= 0)
        {
            delay = m_updateMilliseconds;
        }
        if (0 == wait_event(m_dancerThreadStopEvent, delay))
        {
            // Need to stop dancing
            break;
        }
    } while (true);

    m_threadRunning = false;
    set_event(m_dancerThreadTerminatedEvent);
}

ISDKMessageObserver *SDKSampleApp::SetMessageObserver(ISDKMessageObserver *pObserver)
{
    Lock();
    ISDKMessageObserver *pOldObserver = m_pMessageObserver;
    m_pMessageObserver = pObserver;
    Unlock();
    return pOldObserver;
}

void SDKSampleApp::sProcessTremoloEffect(short *pcm_frames, int pcm_frame_count, int audio_frame_rate, int channels_per_frame, int *time)
{
    if (pcm_frames == nullptr || time == nullptr)
    {
        return;
    }

    // 2 * pi * freq / T;
    double omega = 2.0 * M_PI * 10.0 / audio_frame_rate;
    auto numberOfSamples = (unsigned int)(pcm_frame_count * channels_per_frame);
    for (unsigned int j = 0; j < numberOfSamples; j++)
    {
        // Change range from (-1)-1 to 0-1.
        double modulationValue = (1.0 - sin(omega * (*time))) / 2.0;
        pcm_frames[j] = short(pcm_frames[j] * modulationValue);
        if (j % channels_per_frame == 0)
        {
            // increment on new frame
            (*time)++;
        }
    }
}

// MARK: Audio callbacks
void SDKSampleApp::OnBeforeReceivedAudioMixed(const char *session_group_handle, const char *initial_target_uri, vx_before_recv_audio_mixed_participant_data_t *participants_data, size_t num_participants)
{
    (void)session_group_handle;
    (void)initial_target_uri;

    // Iterate over the multiple audio streams
    for (unsigned int i = 0; i < num_participants; i++)
    {
        auto &data = participants_data[i];
        string participant_uri = data.participant_uri;
        auto iter = m_participantsEffectTimes.find(participant_uri);
        if (iter == m_participantsEffectTimes.end())
        {
            continue;
        }

        sProcessTremoloEffect(data.pcm_frames, data.pcm_frame_count, data.audio_frame_rate, data.channels_per_frame, &(iter->second));
    }
}
