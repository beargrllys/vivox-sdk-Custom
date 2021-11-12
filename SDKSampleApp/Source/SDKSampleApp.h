#pragma once
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

#include <string>
#include <ostream>
#include <vector>
#include <memory>
#include <map>
#include <list>
#include <mutex>
#include <condition_variable>
#include <set>
#include <functional>
#include <sstream>
#include <math.h>
#include "vxplatform/vxcplatform.h"
#include "Vxc.h"
#include "VxcEvents.h"
#include "VxcRequests.h"
#include "VxcResponses.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#include "SDKMessageObserver.h"

// End developers shouldn't set this value. This is only to be used by the SDKSampleApp.
// Please contact your Vivox representative for more information.
#define THIS_APP_UNIQUE_3_LETTERS_USER_AGENT_ID_STRING "VSA"

#include <winsock2.h>
#include <ws2tcpip.h>
#undef socket_t
#define socket_t SOCKET

#define socketclose close
#define GetHostName gethostname

#define TELNET_INFO_HEADER "**************************** Telnet UI Information *****************************\n";
#define TELNET_INFO_FOOTER "********************************************************************************\n";
#define TELNET_INFO "On Windows, we recommend PuTTY (https://www.chiark.greenend.org.uk/~sgtatham/putty/latest.html) to access the telnet interface. Required client settings for PuTTY:\n" \
                    "\tTerminal: Implicit CR in every LF\n"                                                                                                                                 \
                    "\tTerminal->Keyboard : The Backspace key - Control - H\n"                                                                                                              \
                    "\tConnection->Telnet : Telnet negotiation mode : Passive\n"                                                                                                            \
                    "On Linux and macOS, use Netcat (nc) utility or telnet client (if it available). On Linux and macOS, no additional settings are required.\n"

#if defined(__GNUC__) || defined(__clang__)
#define ATTRIBUTE_FORMAT(archetype, string_index, first_to_check) __attribute__((format(archetype, string_index, first_to_check)))
#else
#define ATTRIBUTE_FORMAT(archetype, string_index, first_to_check)
#endif

using namespace std;

class SDKSampleApp
{
public:
    typedef int(ATTRIBUTE_FORMAT(printf, 1, 0) * printf_wrapper_ptr)(const char *format, const va_list &vl);
    typedef void (*pf_exit_callback_t)();
    struct SampleAppPosition
    {
        double x, y, z;
    };
    struct SampleAppOrientation
    {
        double at_x, at_y, at_z, up_x, up_y, up_z;
    };

    // reference implementation methods
    SDKSampleApp(printf_wrapper_ptr printf_wrapper_proc, pf_exit_callback_t cbExit);
    ~SDKSampleApp();
    static SDKSampleApp *GetApp() { return s_pInstance; }
    static std::string getVersionAndCopyrightText();
    void Start();
    void Stop();
    void Shutdown();
    void Wakeup();
    void Lock();
    void Unlock();

    string GetServer()
    {
        return m_server;
    }
    void SetServer(const string &value)
    {
        m_server = value;
    }
    string GetRealm()
    {
        return m_realm;
    }
    void SetRealm(const string &value)
    {
        m_realm = value;
    }
    string GetIssuer()
    {
        return m_accessTokenIssuer;
    }
    void SetIssuer(const string &value)
    {
        m_accessTokenIssuer = value;
    }
    string GetKey()
    {
        return m_accessTokenKey;
    }
    void SetKey(const string &value)
    {
        m_accessTokenKey = value;
    }
    bool IsServerMultitenant() const
    {
        return m_isMultitenant;
    }
    void SetServerMultitenant(bool isMultitenant)
    {
        m_isMultitenant = isMultitenant;
    }
    int GetNeverRtpTimeout() const
    {
        return m_neverRtpTimeoutMS;
    }
    void SetNeverRtpTimeout(int never_rtp_timeout_ms)
    {
        m_neverRtpTimeoutMS = (never_rtp_timeout_ms <= 0) ? 0 : never_rtp_timeout_ms;
    }
    int GetLostRtpTimeout() const
    {
        return m_lostRtpTimeoutMS;
    }
    void SetLostRtpTimeout(int lost_rtp_timeout_ms)
    {
        m_lostRtpTimeoutMS = (lost_rtp_timeout_ms <= 0) ? 0 : lost_rtp_timeout_ms;
    }
    unsigned short GetTcpControlPort() const
    {
        return m_tcpControlPort;
    }
    void SetTcpControlPort(unsigned short tcp_control_port)
    {
        m_tcpControlPort = tcp_control_port;
    }
    long long GetProcessorAffinityMask() const
    {
        return m_processorAffinityMask;
    }
    void SetProcessorAffinityMask(long long processor_affinity_mask)
    {
        m_processorAffinityMask = processor_affinity_mask;
    }

    // sample application only methods for command processing
    void connect(const vector<string> &cmd);
    void participanteffect(const vector<string> &cmd);
    void capturedevice(const vector<string> &cmd);
    void crash(const vector<string> &cmd);
    void renderdevice(const vector<string> &cmd);
    void login(const vector<string> &cmd);
    void addsession(const vector<string> &cmd);
    void removesession(const vector<string> &cmd);
    void move(const vector<string> &cmd);
    void dance(const vector<string> &cmd);
    void focus(const vector<string> &cmd);
    void inject(const vector<string> &cmd);
    void transmit(const vector<string> &cmd);
    void message(const vector<string> &cmd);
    void media(const vector<string> &cmd);
    void text(const vector<string> &cmd);
    void mutelocal(const vector<string> &cmd);
    void muteforme(const vector<string> &cmd);
    void muteuser(const vector<string> &cmd);
    void muteall(const vector<string> &cmd);
    void blockusers(const vector<string> &cmd);
    void volumelocal(const vector<string> &cmd);
    void volumeforme(const vector<string> &cmd);
    void sessionvolumeforme(const vector<string> &cmd);
    void kick(const vector<string> &cmd);
    void getstats(const vector<string> &cmd);
    void blockrule(const vector<string> &cmd);
    void acceptrule(const vector<string> &cmd);
    void captureaudio(const vector<string> &cmd);
    void renderaudio(const vector<string> &cmd);
    void logout(const vector<string> &cmd);
    void loginprops(const vector<string> &cmd);
    void shutdown(const vector<string> &cmd);
    void log(const vector<string> &cmd);
    void state(const vector<string> &cmd);
    void sstats(const vector<string> &cmd);
    void buddy(const vector<string> &cmd);
    void presence(const vector<string> &cmd);
    void mutesession(const vector<string> &cmd);
    void vadproperties(const vector<string> &cmd);
    void derumblerproperties(const vector<string> &cmd);
    void help(const vector<string> &cmd);
    void archive(const vector<string> &cmd);
    void history(const vector<string> &cmd);
    void audioinfo(const vector<string> &cmd);
    void rtp(const vector<string> &cmd);
    // Sleep is done in different places on different platforms, so this is platform selective for now
    void sleep(const std::vector<std::string> &cmd);
    void transcription(const std::vector<std::string> &cmd);
    void backend(const vector<string> &cmd);
    bool ProcessCommand(const vector<string> &args);
    std::vector<std::string> GetCommandsMatch(const std::string &text);
    std::string GetCommandUsage(const std::string &command);

    bool CheckHasConnectorHandle();
    bool CheckDisconnected();
    std::string CheckAndGetDefaultAccountHandle();
    bool CheckHasSessionHandle();
    bool CheckHasSessionGroupHandle();
    bool CheckHasSessionOrSessionGroupHandle();

    static int con_print(const char *format, ...) ATTRIBUTE_FORMAT(printf, 1, 2);

    static std::vector<std::string> splitCmdLine(const std::string &s);

    ISDKMessageObserver *SetMessageObserver(ISDKMessageObserver *pObserver);

    map<string, int> m_participantsEffectTimes;
    static void sProcessTremoloEffect(short *pcm_frames, int pcm_frame_count, int audio_frame_rate, int channels_per_frame, int *time);

    // callbacks
    void OnBeforeReceivedAudioMixed(const char *session_group_handle, const char *initial_target_uri, vx_before_recv_audio_mixed_participant_data_t *participants_data, size_t num_participants);

    vxplatform::os_event_handle *SDKSampleApp::ListenerEventLink();

private:
    static SDKSampleApp *s_pInstance;
    // reference implementation methods
    string DebugGetAccessToken(const string &cmd, const string &subject, const string &from, const string &to);
    string IssueRequest(vx_req_base_t *req, bool bSilent = false);
    string GetNextRequestId();
    void connector_create(const string &server, const string &connectorHandle, unsigned int configured_codecs);
    void account_anonymous_login(const string &user, const string &connectorHandle, const string &accountHandle, const string &displayName, int frequency = -1, const string &langs = string(), bool autoAcceptBuddies = true);
    void connector_initiate_shutdown(const string &connectorHandle);
    void connector_get_local_audio_info(const string &accountHandle, const string &connectorHandle);
    void account_logout(const string &accountHandle);
    void account_set_login_properties(const string &accountHandle, int frequency);
    void sessiongroup_add_session(const string &accountHandle, const string &sessionGroupHandle, const string &sessionHandle, const string &channel, bool joinMuted, bool connectAudio, bool connectText);
    void sessiongroup_remove_session(const string &sessionGroupHandle, const string &sessionHandle);
    void sessiongroup_set_focus(const string &sessionHandle);
    void sessiongroup_reset_focus(const string &sessionGroupHandle);
    void sessiongroup_set_tx_session(const string &sessionHandle);
    void sessiongroup_set_tx_all_sessions(const string &sessionGroupHandle);
    void sessiongroup_set_tx_no_session(const string &sessionGroupHandle);
    void session_media_connect(const string &sessionHandle);
    void session_media_disconnect(const string &sessionHandle);
    void session_mute_local_speaker(const string &sessionHandle, bool muted, vx_mute_scope scope);
    void session_set_local_speaker_volume(const string &sessionHandle, int level);
    void session_set_local_render_volume(const string &sessionHandle, int level);
    void session_set_participant_volume_for_me(const string &sessionHandle, const string &participant, int level);
    void session_set_participant_mute_for_me(const string &sessionHandle, const string &participant, bool muted, vx_mute_scope scope = mute_scope_all);
    void session_set_3d_position(const string &sessionHandle, int x, int y, double radians);
    void channel_mute_user(const string &accountHandle, const string &channel, const string &participant, bool muted, vx_mute_scope scope);
    void channel_kick_user(const string &accountHandle, const string &channel, const string &participant);
    void channel_mute_all_users(const string &accountHandle, const string &channel, bool muted, vx_mute_scope scope);
    void connector_mute_local_mic(const string &accountHandle, bool muted);
    void connector_mute_local_speaker(const string &accountHandle, bool muted);
    void account_buddy_set(const string &accountHandle, const string &buddy);
    void account_buddy_delete(const string &accountHandle, const string &buddy);
    void account_list_buddies_and_groups(const string &accounthandle);
    void session_send_message(const string &sessionHandle, const string &message, const string &customMetadataNS, const string &customMetadata);
    void account_send_message(const string &accountHandle, const string &message, const string &user, const string &customMetadataNS, const string &customMetadata);
    void session_archive_messages(
        const string &sessionHandle,
        unsigned int max,
        const string &time_start,
        const string &time_end,
        const string &text,
        const string &before,
        const string &after,
        int index,
        const string &user);
    void account_archive_query(
        const string &accountHandle,
        unsigned int max,
        const string &time_start,
        const string &time_end,
        const string &text,
        const string &before,
        const string &after,
        int index,
        const string &channel,
        const string &user);
    void account_set_presence(const string &accountHandle, vx_buddy_presence_state presence, const string &message);
    void account_send_subscription_reply(const string &accountHandle, const string &buddy, bool allow);
    void session_send_notification(const string &sessionHandle, bool typing);
    void account_create_block_rule(const string &accountHandle, const string &otherUser);
    void account_delete_block_rule(const string &accountHandle, const string &otherUser);
    void account_list_block_rules(const string &accountHandle);
    void account_create_auto_accept_rule(const string &accountHandle, const string &otherUser);
    void account_delete_auto_accept_rule(const string &accountHandle, const string &otherUser);
    void account_list_auto_accept_rules(const string &accountHandle);
    void aux_get_render_devices(const string &accountHandle);
    void aux_get_capture_devices(const string &accountHandle);
    void aux_set_render_device(const string &accountHandle, const string &deviceId);
    void aux_set_capture_device(const string &accountHandle, const string &deviceId);
    void aux_get_mic_level(const string &accountHandle);
    void aux_get_speaker_level(const string &accountHandle);
    void aux_set_mic_level(const string &accountHandle, int level);
    void aux_set_speaker_level(const string &accountHandle, int level);
    void aux_render_audio_start(const string &accountHandle, const string &filename, bool loop);
    void aux_render_audio_stop(const string &accountHandle);
    void aux_capture_audio_start(const string &accountHandle);
    void aux_capture_audio_stop(const string &accountHandle);
    void sessiongroup_set_session_3d_position(const string &sessionHandle, int x, int y);
    void aux_start_buffer_capture(const string &accountHandle);
    void aux_play_audio_buffer(const string &accountHandle);
    void session_text_connect(const string &sessionHandle);
    void session_text_disconnect(const string &sessionHandle);
    void aux_set_vad_properties(const string &accountHandle, int hangover, int sensitivity, int noiseFloor, int isAuto);
    void aux_get_vad_properties(const string &accountHandle);
    void aux_get_derumbler_properties(const string &accountHandle);
    void aux_set_derumbler_properties(const string &accountHandle, int enabled, int stopband_corner_frequency);
    void sessiongroup_inject_audio_start(const string &sessionGroupHandle, const string &file);
    void sessiongroup_inject_audio_stop(const string &sessionGroupHandle);
    void sessiongroup_get_stats(const string &sessionGroupHandle);
    void account_blockusers(const string &accountHandle, const list<string> &otherUsers, vx_control_communications_operation operation);
    void session_transcription_control(const string &sessionHandle, const string &channel, bool enable);
    void aec(const std::vector<std::string> &cmd);
    void agc(const std::vector<std::string> &cmd);
#ifndef SAMPLEAPP_DISABLE_TTS
    vx_tts_status InitializeTTSManagerIfNeeded();
    void ttsspeak(const std::vector<std::string> &cmd);
    void ttscancel(const std::vector<std::string> &cmd);
    void ttsshutdown(const std::vector<std::string> &cmd);
    void ttsvoice(const std::vector<std::string> &cmd);
    void ttsspeaktextchannel(const std::vector<std::string> &cmd);
#endif

    // positional methods - all implment the same request and a typical implementation would need only one method
    void req_move_origin(const string &sessionHandle);
    void req_move_forward(const string &sessionHandle, double delta);
    void req_move_back(const string &sessionHandle, double delta);
    void req_move_left(const string &sessionHandle, double delta);
    void req_move_right(const string &sessionHandle, double delta);
    void req_move_up(const string &sessionHandle, double delta);
    void req_move_down(const string &sessionHandle, double delta);
    void req_turn_left(const string &sessionHandle, double delta);
    void req_turn_right(const string &sessionHandle, double delta);

    // local state management
    string GetUserUriForAccountHandle(const string &accountHandle) const;
    string GetUserUri(const string &nameOrUri);
    string DefaultSessionGroupHandle();
    string DefaultSessionHandle();
    bool CheckExistsSessionGroupHandle(string sessionGroupHandle);
    string FindSessionGroupHandleBySessionHandle(string sessionHandle);

    // sample application only methods
    string Xml(vx_req_base_t *req);
    string Xml(vx_resp_base_t *resp);
    string Xml(vx_evt_base_t *evt);
    void DeclareCommands();
    void GetListenerPosition(const std::string session_handle, SampleAppPosition &position);
    void GetListenerOrientation(const std::string session_handle, SampleAppOrientation &orientation);
    void SetListenerPosition(const std::string session_handle, SampleAppPosition &position);
    void SetListenerOrientation(const std::string session_handle, SampleAppOrientation &orientation);
    void GetListenerHeadingDegrees(const std::string session_handle, double &heading);
    void SetListenerHeadingDegrees(const std::string session_handle, double heading);
    void Set3DPositionRequestFields(vx_req_session_set_3d_position *req, SampleAppPosition &position, SampleAppOrientation &orientation);

private:
    vxplatform::Lock *m_lock;
    static vxplatform::os_error_t ListenerThread(void *arg); // For catching Reponses and Events
    void ListenerThread();
    void HandleMessage(vx_message_base_t *msg);
    unsigned long long GetNextSerialNumber();
    void SetRenderDeviceIndexByAccountHandle(const std::string &accountHandle, int deviceIndex);
    void SetCaptureDeviceIndexByAccountHandle(const std::string &accountHandle, int deviceIndex);

    vxplatform::os_event_handle m_messageAvailableEvent;
    vxplatform::os_event_handle m_listenerThreadTerminatedEvent;
    vxplatform::os_event_handle listenerEvent;
    vxplatform::os_thread_handle m_listenerThread;
    vxplatform::os_thread_id m_listenerThreadId;
    bool m_started;
    int m_requestId;

    // local state management
    string m_connectorHandle;
    string m_realm;
    string m_server;
    mutable mutex m_accountHandleUserNameMutex;
    set<string> m_accountHandles;
    map<string, string> m_accountHandleUserNames;
    typedef pair<string, string> SSGPair;
    vector<SSGPair> m_SSGHandles;
    string m_accessTokenIssuer;
    string m_accessTokenKey;
    bool m_isMultitenant;
    int m_neverRtpTimeoutMS; // (< 0) => unset, 0 - disabled, (>0) - milliseconds
    int m_lostRtpTimeoutMS;  // (< 0) => unset, 0 - disabled, (>0) - milliseconds
    unsigned short m_tcpControlPort;
    long long m_processorAffinityMask;
#ifndef SAMPLEAPP_DISABLE_TTS
    vx_tts_manager_id *m_ttsManagerId;
    vx_tts_voice_id m_ttsVoiceID = 1;
    set<string> m_ttsSessionHandles;
#endif

    void InternalLoggedOut(string accountHandle);

    // controls sample application log messages
    bool m_logRequests;
    bool m_logResponses;
    bool m_logEvents;
    bool m_logXml;
    bool m_logParticipantUpdate;

    // vad properties
    int m_vadHangover;
    int m_vadSensitivity;
    int m_vadNoiseFloor;
    int m_vadAuto;

    // positional info
    std::map<std::string, double> m_listenerHeadingDegrees; // Listener's heading in degrees (North (Negative Z axis) is 0 deg, East (Positive X axis) is +90 deg etc)
    std::map<std::string, SampleAppPosition> m_listenerPositions;
    std::map<std::string, SampleAppOrientation> m_listenerOrientations;

    // Selecting devices by 1-based indicies, mapped by account_handle
    std::mutex m_setRenderDeviceIndexMutex;
    std::map<std::string, int> m_setRenderDeviceIndexByAccountHandle;
    std::mutex m_setCaptureDeviceIndexMutex;
    std::map<std::string, int> m_setCaptureDeviceIndexByAccountHandle;

    // console wrapper
    static printf_wrapper_ptr _printf_wrapper;

    // Exit wrapper
    pf_exit_callback_t m_cbExit;

    // Message observer
    ISDKMessageObserver *m_pMessageObserver;

    // sample application only to manage commands

    class Command
    {
    public:
        typedef void (SDKSampleApp::*CommandFunction)(const vector<string> &);

    public:
        Command(const string &name, const CommandFunction &commandFunction, const string &usage, const string &description, const string &documentation = string()) : m_commandFunction(commandFunction)
        {
            m_name = name;
            m_usage = usage;
            m_description = description;
            m_documentation = documentation;
        }
        const string &GetName() const
        {
            return m_name;
        }
        const CommandFunction &GetCommandFunction() const
        {
            return m_commandFunction;
        }
        const string &GetUsage() const
        {
            return m_usage;
        }
        const string &GetDescription() const
        {
            return m_description;
        }
        const string &GetDocumentation() const
        {
            return m_documentation;
        }
        void SetDocumentation(const string &documentation)
        {
            m_documentation = documentation;
        }

    private:
        string m_name;
        CommandFunction m_commandFunction;
        string m_usage;
        string m_description;
        string m_documentation;
    };

    typedef map<string, Command> CommandMap;
    typedef CommandMap::const_iterator CommandMapConstItr;
    CommandMap m_commands;

    // participant class for local state management

    class Participant
    {
    public:
        Participant(const string &uri)
        {
            m_uri = uri;
            m_hasAudio = false;
            m_hasText = false;
            m_channelMuted = false;
            m_mutedForMe = false;
            m_volume = 0;
        }
        const string &GetUri() const
        {
            return m_uri;
        }
        const bool &GetHasAudio() const
        {
            return m_hasAudio;
        }
        const bool &GetHasText() const
        {
            return m_hasText;
        }
        const bool &GetChannelMuted() const
        {
            return m_channelMuted;
        }
        const bool &GetMutedForMe() const
        {
            return m_mutedForMe;
        }
        const int &GetVolume() const
        {
            return m_volume;
        }
        void UpdateParticipant(vx_evt_participant_updated *evt)
        {
            m_hasAudio = (evt->active_media & VX_MEDIA_FLAGS_AUDIO) ? true : false;
            m_hasText = (evt->active_media & VX_MEDIA_FLAGS_TEXT) ? true : false;
            m_channelMuted = evt->is_moderator_muted ? true : false;
            m_mutedForMe = evt->is_muted_for_me ? true : false;
            m_volume = evt->volume;
        }
        void PrintParticipant(const string &format) const
        {
            stringstream ss;
            ss << "Participant: '" << m_uri << "' Audio(" << m_hasAudio << ") Text(" << m_hasText << ")";
            con_print(format.c_str(), ss.str().c_str());
            ss.str(string());
            ss << "              MutedInChannel(" << m_channelMuted << ") MutedForMe(" << m_mutedForMe << ") Volume(" << m_volume << ")";
            con_print(format.c_str(), ss.str().c_str());
        }

    private:
        string m_uri;
        bool m_hasAudio;
        bool m_hasText;
        bool m_channelMuted;
        bool m_mutedForMe;
        int m_volume;
    };

    typedef map<string, Participant> ParticipantMap;
    typedef ParticipantMap::iterator ParticipantMapItr;
    typedef ParticipantMap::const_iterator ParticipantMapConstItr;

    // session class for local state management
    class Session
    {
    public:
        Session(const string &uri, const string &sessionHandle, const string &sessionGroupHandle, const string &accountHandle)
        {
            m_uri = uri;
            m_sessionHandle = sessionHandle;
            m_sessionGroupHandle = sessionGroupHandle;
            m_accountHandle = accountHandle;
        }
        const string &GetUri() const
        {
            return m_uri;
        }
        const string &GetSessionHandle() const
        {
            return m_sessionHandle;
        }
        const string &GetSessionGroupHandle() const
        {
            return m_sessionGroupHandle;
        }
        const string &GetAccountHandle() const
        {
            return m_accountHandle;
        }
        const ParticipantMap &GetParticipants() const
        {
            return m_participants;
        }
        pair<ParticipantMapConstItr, bool> AddParticipant(const string &uri)
        {
            return m_participants.insert(make_pair(uri, Participant(uri)));
        }
        size_t RemoveParticipant(const string &uri)
        {
            return m_participants.erase(uri);
        }
        const ParticipantMapConstItr UpdateParticipant(vx_evt_participant_updated *evt)
        {
            ParticipantMapItr itr = m_participants.find(evt->participant_uri);
            if (itr != m_participants.end())
            {
                itr->second.UpdateParticipant(evt);
            }
            return itr;
        }
        void PrintParticipants(const string &format) const
        {
            if (m_participants.empty())
            {
                return;
            }
            for (ParticipantMapConstItr itr = m_participants.begin(); itr != m_participants.end(); ++itr)
            {
                itr->second.PrintParticipant(format);
            }
        }
        bool IsDancing() const { return m_dancer.IsStarted(); }
        void DanceStart(
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
            m_dancer.Start(sessionHandle, x0, y0, z0, rmin, rmax, angularVelocityDegPerSec, oscillationPeriodSeconds, updateMilliseconds);
        }
        void DanceStop() { m_dancer.Stop(); }

    private:
        string m_uri;
        string m_sessionHandle;
        string m_sessionGroupHandle;
        string m_accountHandle;
        ParticipantMap m_participants;

        class Dancer
        {
        public:
            Dancer();
            virtual ~Dancer();

        private:
            Dancer(const Dancer &) {} // disabled

        public:
            void Start(
                const string &sessionHandle,
                double x0,
                double y0,
                double z0,
                double rmin,
                double rmax,
                double angularVelocityDegPerSec,
                double oscillationPeriodSeconds,
                int updateMilliseconds);
            void Stop();
            bool IsStarted() const
            {
                return m_threadRunning;
            }

        private:
            string m_sessionHandle;
            double m_x0;
            double m_y0;
            double m_z0;
            double m_rmin;
            double m_rmax;
            double m_angularVelocityDegPerSec;
            double m_oscillationPeriodSeconds;
            int m_updateMilliseconds;

            static double GetMilliseconds();

            vxplatform::Lock *m_lock;
            volatile bool m_threadRunning;
            vxplatform::os_thread_handle m_dancerThread;
            static vxplatform::os_error_t DancerThread(void *arg); // For updating user's position
            void DancerThread();
            vxplatform::os_event_handle m_dancerThreadStartedEvent;
            vxplatform::os_event_handle m_dancerThreadTerminatedEvent;
            vxplatform::os_event_handle m_dancerThreadStopEvent;
        };

        Dancer m_dancer;
    };

    map<string, Session *> m_sessions;

    void TerminateListenerThread();

public:
    bool isTcpConsoleConnected() const { return INVALID_SOCKET != m_tcpConsoleClientSocket; }

private:
    void TerminateTcpConsoleThread();

    volatile bool m_terminateConsoleThread;

    socket_t m_tcpConsoleClientSocket;
    socket_t m_tcpConsoleServerSocket;

    vxplatform::os_thread_handle m_tcpConsoleThread;
    vxplatform::os_thread_id m_tcpConsoleThreadId;
    vxplatform::Lock m_tcpConsoleThreadLock;

    static vxplatform::os_error_t TcpConsoleThreadProcStatic(void *arg);
    vxplatform::os_error_t TcpConsoleThreadProc();
};
