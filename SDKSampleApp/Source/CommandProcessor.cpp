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

#define __PRINT_SSG_HANDLE_ERROR(type) SDKSampleApp::con_print("error: You must have a session%s handle to perform this action.\n", 0 == type ? "" : 1 == type ? "group" : " or sessiongroup")
#define PRINT_SESSION_HANDLE_ERROR() __PRINT_SSG_HANDLE_ERROR(0)
#define PRINT_GROUP_HANDLE_ERROR() __PRINT_SSG_HANDLE_ERROR(1)
#define PRINT_SSG_HANDLE_ERROR() __PRINT_SSG_HANDLE_ERROR(2)


#define snprintf _snprintf
#define sscanf sscanf_s

#include <thread>
#include <cctype>

bool SDKSampleApp::CheckHasConnectorHandle()
{
    if (m_connectorHandle.empty()) {
        SDKSampleApp::con_print("error: You must have a connector handle to perform this action.\n");
        return false;
    }
    return true;
}

bool SDKSampleApp::CheckDisconnected()
{
    if (!m_connectorHandle.empty()) {
        SDKSampleApp::con_print("error: You must be disconnected to perform this action.\n");
        return false;
    }
    return true;
}

std::string SDKSampleApp::CheckAndGetDefaultAccountHandle()
{
    lock_guard<mutex> lock(m_accountHandleUserNameMutex);
    if (m_accountHandles.empty()) {
        SDKSampleApp::con_print("error: You must have an account handle to perform this action.\n");
        return "";
    }
    return *m_accountHandles.begin();
}

bool SDKSampleApp::CheckHasSessionHandle()
{
    if (m_SSGHandles.empty()) {
        PRINT_SESSION_HANDLE_ERROR();
        return false;
    }
    return true;
}

bool SDKSampleApp::CheckHasSessionGroupHandle()
{
    if (m_SSGHandles.empty()) {
        PRINT_GROUP_HANDLE_ERROR();
        return false;
    }
    return true;
}

bool SDKSampleApp::CheckHasSessionOrSessionGroupHandle()
{
    if (m_SSGHandles.empty()) {
        PRINT_SSG_HANDLE_ERROR();
        return false;
    }
    return true;
}

string SDKSampleApp::Xml(vx_req_base_t *req)
{
    char *xml = NULL;
    vx_request_to_xml(req, &xml);
    if (xml == NULL) {
        con_print("\r * vx_request_to_xml() failed to convert %s to xml\n", vx_get_request_type_string(req->type));
        return string();
    } else {
        string strXml = xml;
        vx_free(xml);
        return strXml;
    }
}

string SDKSampleApp::Xml(vx_resp_base_t *resp)
{
    char *xml = NULL;
    vx_response_to_xml(resp, &xml);
    if (xml == NULL) {
        con_print("\r * vx_response_to_xml() failed to convert %s to xml\n", vx_get_response_type_string(resp->type));
        return string();
    } else {
        string strXml = xml;
        vx_free(xml);
        return strXml;
    }
}

string SDKSampleApp::Xml(vx_evt_base_t *evt)
{
    char *xml = NULL;
    vx_event_to_xml(evt, &xml);
    if (xml == NULL) {
        con_print("\r * vx_event_to_xml() failed to convert %s to xml\n", vx_get_event_type_string(evt->type));
        return string();
    } else {
        string strXml = xml;
        vx_free(xml);
        return strXml;
    }
}

static bool checkScopeArg(string &value, vx_mute_scope *scope)
{
    if (!value.compare("text")) {
        *scope = mute_scope_text;
        return true;
    }
    if (!value.compare("audio")) {
        *scope = mute_scope_audio;
        return true;
    }
    if (!value.compare("all")) {
        *scope = mute_scope_all;
        return true;
    }
    return false;
}

static bool nextArg(string &value, const vector<string> &cmd, vector<string>::const_iterator &i, bool &error)
{
    ++i;
    if (i == cmd.end()) {
        error = true;
        return false;
    }
    value = *i;
    return true;
}

static bool nextArg(bool &value, const vector<string> &cmd, vector<string>::const_iterator &i, bool &error)
{
    ++i;
    if (i == cmd.end()) {
        error = true;
        return false;
    }
    if (*i == "yes") {
        value = true;
    } else if (*i == "no") {
        value = false;
    } else {
        error = true;
        return false;
    }
    return true;
}

static bool nextArg(int &value, const vector<string> &cmd, vector<string>::const_iterator &i, bool &error)
{
    ++i;
    if (i == cmd.end()) {
        error = true;
        return false;
    }
    value = atoi(i->c_str());
    return true;
}

static bool nextArg(unsigned int &value, const vector<string> &cmd, vector<string>::const_iterator &i, bool &error)
{
    ++i;
    if (i == cmd.end()) {
        error = true;
        return false;
    }
    if (1 != sscanf(i->c_str(), "%u", &value)) {
        error = true;
        return false;
    }
    return true;
}

static bool nextArg(double &value, const vector<string> &cmd, vector<string>::const_iterator &i, bool &error)
{
    ++i;
    if (i == cmd.end()) {
        error = true;
        return false;
    }
    value = atof(i->c_str());
    return true;
}

#if defined(_MSC_VER) && (_MSC_VER < 1900)
#define MEMFUN1 std::mem_fun1
#else
#define MEMFUN1 std::mem_fun
#endif

#define DECLARE_COMMAND(x, y, z) m_commands.insert(make_pair(#x, Command(#x, &SDKSampleApp::x, y, z, ss.str()))); ss.str(string())
#define D(text) ss << text << "\n"
void SDKSampleApp::DeclareCommands()
{
    stringstream ss;
    // acceptrule
    D("Default Behavior: Prints user's accept rule list.");
    D("State: Requires an account handle (via 'login' command).");
    D("");
    D("Arguments:");
    D("    -create uri        Implements vx_req_account_create_auto_accept_rule_t. Accept rule is created for URI.");
    D("    -delete uri        Implements vx_req_account_delete_auto_accept_rule_t. Accept rule for URI is deleted.");
    D("    -list              Implements vx_req_account_list_auto_accept_rules_t. Prints user's accept rule list.");
    D("    -ah account_handle The account to operate on. Defaults to the oldest active account.");
    DECLARE_COMMAND(acceptrule, "[-create uri|-delete uri|-list] [-ah account_handle]", "List or define rules to allow individuals to see your presence.");
    // addsession
    D("Default Behavior: Creates and joins a new random echo channel unmuted, with audio and text enabled.");
    D("State: Requires an account handle (via 'login' command).");
    D("");
    D("Arguments:");
    D("    -c channel              URI of the channel to create or join in this session. Refer to \"Vivox");
    D("                            SDK Sample Application.pdf\" for information on how the naming choice of");
    D("                            ephemeral channel URIs controls special channel properties.");
    D("    -joinmuted              Start the session in the muted state.");
    D("    -audio                  Supply 'yes' to start the session with audio capability or 'no' to start");
    D("                            without it. Defaults to 'yes'.");
    D("    -text                   Supply 'yes' to start the session with text capability or 'no' to start");
    D("                            without it. Defaults to 'yes'.");
    D("    -sh session_handle      A string used as the handle to this session. Defaults to the channel URI");
    D("                            specified by '-c' argument.");
    D("    -gh sessiongroup_handle The sessiongroup to add this session to. If the supplied argument");
    D("                            matches an existing sessiongroup_handle, the new session will be added");
    D("                            to that sessiongroup. If the argument does not match, a new sessiongroup");
    D("                            will be created with the specified handle. If this argument is omitted,");
    D("                            the session will be added to the most recent sessiongroup created. If no");
    D("                            sessiongroup exists, a new sessiongroup will be created with a handle");
    D("                            constructed by prepending \"sg_\" to the session_handle specified by");
    D("                            '-sh' argument.");
    D("    -ah account_handle      The account to use with the session. Defaults to the oldest active account.");
    D("");
    D("Additional Notes:");
    D("    Implements vx_req_sessiongroup_add_session_t. The success of this request guarantees a new ");
    D("    session handle and the presence of a sessiongroup handle. Commands that require one or both of ");
    D("    these may be used until these handles are invalidated. While the handles are required for such ");
    D("    commands, session handle and sessiongroup handles arguments are always optional, defaulting to ");
    D("    the most recent added. You can check the default handles as well as a nested list of all active");
    D("    sessiongroups, sessions, and participant rosters in those sessions, with the 'state' command.");
    DECLARE_COMMAND(addsession, "[-c channel] [-joinmuted] [-audio yes|no] [-text yes|no] [-sh session_handle] [-gh sessiongroup_handle] [-ah account_handle]", "Connect to a new group text and/or audio conference.");
    // blockrule
    D("Default Behavior: Prints user's block rule list.");
    D("State: Requires an account handle (via 'login' command).");
    D("");
    D("Arguments:");
    D("    -create uri        Implements vx_req_account_create_block_rule_t. Block rule is created for URI.");
    D("    -delete uri        Implements vx_req_account_delete_block_rule_t. Block rule for URI is deleted.");
    D("    -list              Implements vx_req_account_list_block_rules_t. Prints user's block rule list.");
    D("    -ah account_handle The account to operate on. Defaults to the oldest active account.");
    DECLARE_COMMAND(blockrule, "[-create uri|-delete uri|-list] [-ah account_handle]", "List or define rules to block individuals or groups of individuals.");
    // buddy
    D("Default Behavior: Prints user's buddy list.");
    D("State: Requires an account handle (via 'login' command).");
    D("");
    D("Arguments:");
    D("    -set uri           Implements vx_req_account_buddy_set_t. URI is set as user's buddy.");
    D("    -delete uri        Implements vx_req_account_buddy_delete_t. URI is deleted from user's");
    D("                       buddy list.");
    D("    -list              Implements vx_req_account_list_buddies_and_groups_t. Prints user's buddy list.");
    D("    -ah account_handle The account to operate on. Defaults to the oldest active account.");
    DECLARE_COMMAND(buddy, "[-set uri|-delete uri|-list] [-ah account_handle]", "List user's buddies or add or remove a buddy from the user's buddy list.");
    // captureaudio
    D("Arguments:");
    D("    -start             Implements vx_req_aux_capture_audio_start_t. Starts audio capture (see");
    D("                       Additional Notes) and begins looping captured audio to the user's current");
    D("                       selected render device.");
    D("    -stop              Implements vx_req_aux_capture_audio_stop_t. Stops audio capture immediately.");
    D("    -ah account_handle The account to operate on. Defaults to the oldest active account.");
    D("");
    D("Additional Notes:");
    D("    This command can be used to ensure that the user's current selected capture device is");
    D("    functioning properly. It can only be invoked successfully if the capture device is not currently");
    D("    in use (in a voice session, for example). As soon as the capture audio start method completes");
    D("    successfully, the Vivox SDK sound system starts sending events of type");
    D("    'vx_evt_aux_audio_properties' at one half the capture frame rate until capture is stopped. Be");
    D("    aware the audio properties events, which show changes in mic activity, volume, energy, and more,");
    D("    occur too frequently to print in a console application, and so result in no messages within the");
    D("    Sample Application.");
    DECLARE_COMMAND(captureaudio, "-start|-stop [-ah account_handle]", "Perform a 'mic check' on a capture device that's not currently in use.");
    // capturedevice
    D("Default Behavior: Prints the device ID of the current, default, and effective capture device, and");
    D("                  device IDs of all available capture devices.");
    D("");
    D("Arguments:");
    D("    -get               Implements vx_req_aux_get_capture_devices_t. Prints device IDs for current,");
    D("                       default, effective, and all available capture devices");
    D("                       in the form: [<index>] <display_name> (<device_id>).");
    D("    -set device_id     Implements vx_req_aux_set_capture_device_t. Sets new capture device to");
    D("                       'device_id', a string matching a device ID returned by 'capturedevice -get'.");
    D("    -i index           Same as '-set', but refers to the device by its index rather than by its device id.");
    D("                       Use '-get' to list all available capture devices and their indicies.");
    D("    -ah account_handle The account to operate on. Defaults to the oldest active account.");
    D("");
    D("Additional Notes:");
    D("    Refer to \"Vivox SDK Sample Application.pdf\" for information on different types of audio devices.");
    DECLARE_COMMAND(capturedevice, "[-get] [-set device_id] [-i index] [-ah account_handle]", "Get current and available capture devices or set the capture device globally.");
    // connect
    D("Arguments:");
    D("    -ch connector_handle    A string used as the handle to this connection. Defaults to the fully");
    D("                            qualified hostname of the server to connect to.");
    D("    -c  codecs_mask         Specify codecs mask to use (sets vx_req_connector_create::configured_codecs)");
    D("                            1 - PCMU, 2 - Siren7, 4 - Siren14, 16 - Opus8, 32 - Opus40, 64 - Opus57, 128 - Opus72");
    D("                            0 (or missing '-c' option) will use the default codecs set for the platform");
    D("");
    D("Additional Notes:");
    D("    Implements vx_req_connector_create_t. Connects to the Vivox server specified by the '--server'");
    D("    argument to the application at startup. In order to use Vivox communication services, the");
    D("    application must first create and initialize a connector object with this command. Only");
    D("    auxiliary and application-only commands may be performed until a connector object is created.");
    D("    The Vivox SDK does not support multiple simultaneous Vivox service connections.");
    DECLARE_COMMAND(connect, "[-ch connector_handle] [-c codecs_mask]", "Connect the application to the Vivox service.");
    // crash
    D("Default Behavior: Crash the program on access to the zero pointer (-zp).");
    D("");
    D("Arguments:");
    D("    -zp Crash the program on access to to the zero pointer.");
    D("    -av Crash the program on access to random restricted page (access violation).");
    D("    -so Crash the program by overflowing the program stack.");
    D("    -hc Crash the program by heap corruption.");
#ifdef __clang__
    D("    -bt Crash the program by builtin_trap().");
#endif
    D("    Default is -zp.");
#ifdef __clang__
    DECLARE_COMMAND(crash, "[-zp|-av|-so|-hc|-bt]", "Do the crash test.");
#else
    DECLARE_COMMAND(crash, "[-zp|-av|-so|-hc]", "Do the crash test.");
#endif
    // participanteffect
    D("State: Requires a session handle (via 'addsession' command).");
    D("");
    D("Arguments:");
    D("    -u user                URI of the participant.");
    D("    -on|-off               Turn on and off audio effect.");
    DECLARE_COMMAND(participanteffect, "-u user [-on|-off]", "Add an audio effect to a participant, on the render side.");
    // focus
    D("State: Requires session handle for '-set' and sessiongroup handle for '-reset'.");
    D("");
    D("Arguments:");
    D("    -set                    Implements vx_req_sessiongroup_set_focus_t. The audible volume of the");
    D("                            session specified by '-sh' argument will be relatively raised with");
    D("                            respect to the channels that are not in 'focus'.");
    D("    -reset                  Implements vx_req_sessiongroup_reset_focus_t. Reset all sessions in a");
    D("                            sessiongroup specified by '-gh' argument so they have equal focus.");
    D("    -sh session_handle      Used with '-set'. The session to emphasize audio in. Defaults to most");
    D("                            recent added.");
    D("    -gh sessiongroup_handle Used with '-reset'. Indicates sessiongroup to equalize audio in.");
    D("                            Defaults to most recent added.");
    D("");
    D("Additional Notes:");
    D("    Only one session within each sessiongroup may have audio focus set at a time. Setting focus on a");
    D("    session only brings its audio to the foreground compared to other sessions within the same");
    D("    sessiongroup; it has no impact on the relative volume of sessions in other sessiongroups.");
    DECLARE_COMMAND(focus, "-set|-reset [-sh session_handle] [-gh sessiongroup_handle]", "Set or reset the 'bringing to the foreground' of audio in a session.");
    // getstats
    D("State: Requires a sessiongroup handle.");
    D("");
    D("Arguments:");
    D("    -gh sessiongroup_handle Indicates sessiongroup of the call to retrieve and print network");
    D("                            statistics for. Defaults to most recent added.");
    DECLARE_COMMAND(getstats, "[-gh sessiongroup_handle]", "Retrieve network statistics for the call associated with a sessiongroup.");
    // help
    D("Default Behavior: Prints Quickstart guide, full list of available commands, and general help.");
    D("");
    D("Arguments:");
    D("    search term     A string to search for. Lists the name and description of each command whose");
    D("                    name contains the search string.");
    D("");
    D("Additional Notes:");
    D("    For the usage and extended documentation of any command, instead type the name of the command");
    D("    followed by '-help' or '--help'.");
    DECLARE_COMMAND(help, "[search term]", "List name and description of all commands, or those that partially match search.");
    // inject
    D("State: Requires a sessiongroup handle.");
    D("");
    D("Arguments:");
    D("    -start filename         Starts playing filename into all sessions within sessiongroup");
    D("                            specified by '-gh' argument. See Additional Notes for restrictions on");
    D("                            filename.");
    D("    -stop                   Stops audio injection into into all sessions within sessiongroup");
    D("                            specified by '-gh' argument immediately.");
    D("    -gh sessiongroup_handle Indicates sessiongroup of the call to start or stop audio injection in.");
    D("                            Defaults to most recent added.");
    D("");
    D("Additional Notes:");
    D("    Both '-start' and '-stop' implement vx_req_sessiongroup_control_audio_injection_t. filename");
    D("    should be the full pathname for the WAV file to use for audio injection (MUST be single channel,");
    D("    16-bit PCM, with the same sample rate as the negotiated audio codec of the call in the chosen");
    D("    sessiongroup).");
    DECLARE_COMMAND(inject, "-start filename|-stop [-gh sessiongroup_handle]", "Start or stop audio injection into a sessiongroup.");
    // kick
    D("State: Requires an account handle (via 'login' command).");
    D("");
    D("Arguments:");
    D("    -u user            URI of the participant to be kicked.");
    D("    -c channel         URI of the channel the participant is to be kicked from.");
    D("    -ah account_handle The account to operate on. Defaults to the oldest active account.");
    D("");
    D("Additional Notes:");
    D("    Implements vx_req_channel_kick_user_t.");
    DECLARE_COMMAND(kick, "-u user -c channel [-ah account_handle]", "Kick a user out of a channel.");
    // log
    D("Default Behavior: Prints the enabled/disabled state of each log message type.");
    D("");
    D("Arguments:");
    D("    -requests   Supply 'yes' to enable logging of a message whenever an SDK request is issued. Will");
    D("                also print complete requests in XML format if '-xml' is enabled. Supply 'no' to");
    D("                disable these messages. Enabled by default.");
    D("    -responses  Supply 'yes' to enable logging of a message whenever an SDK response is received,");
    D("                including status code and message string in case of an error. Will also print");
    D("                complete responses in XML format if '-xml' is enabled. Supply 'no' to disable these");
    D("                messages.* Enabled by default.");
    D("    -events     Supply 'yes' to enable logging of a message whenever an SDK event occurs. Will also");
    D("                print complete events in XML format if '-xml' is enabled. Supply 'no' to disable");
    D("                these messages. Disabled by default.");
    D("    -xml        Supply 'yes' to enable XML logging. All SDK requests, responses, and events will be");
    D("                converted into XML format and printed in their entirety if the requisite message");
    D("                type has its logging enabled (see above). There is a 1-1 mapping between the names");
    D("                and fields of the structs and how their XML counterparts look. Disabled by default.");
    D("    -pu         Supply 'yes' to enable detailed logging for the participant updated event.");
    D("                Supply 'no' to disable these messages. Disabled by default.");
    D("");
    D("Additional Notes:");
    D("    The application will still generate some of its own log statements even when each type of Vivox");
    D("    SDK logging is disabled. Generally, these occur to relay state changes or the creation and");
    D("    destruction of handles. Additionally, many requests involve getting or listing info such as call");
    D("    statistics or audio devices. The requested information will still print even if other response");
    D("    and event messages are disabled.");
    D("");
    D("    *NB: Disabling response logging may silence error messages and hinder issue diagnosis.");
    DECLARE_COMMAND(log, "[-requests yes|no] [-responses yes|no] [-events yes|no] [-xml yes|no]", "Adjust the logging of different types of SDK messages to the console.");
    // login
    D("State: Requires a connector handle (via 'connect' command). This command will fail if an account");
    D("       handle is already active (See Additional Notes).");
    D("");
    D("Arguments:");
    D("    -u username         Username of the user to log in as. Defaults to a random user ID composed of");
    D("                        the prefix \"sa_\" followed by 8 cryptographically random alphanumeric");
    D("                        characters. This string is enclosed by a pair of '.'s. An example default");
    D("                        username would be \".sa_41fa680d.\". When choosing a username, please refer to");
    D("                        \"Vivox SDK Sample Application.pdf\" for restrictions.");
    D("    -ah account_handle  A string used as the handle for this account. Defaults to the username");
    D("                        string specified by '-u' argument.");
    D("    -d Display_Name     User's display name, this will be used as the display name that will be seen by others.");
    D("    -freq <frequency>   This is an integer that specifies how often the SDK will send participant property events while in a channel.");
    D("                        If this is not set the default will be \"on state change\", which means that the events will be sent when");
    D("                        the participant starts talking, stops talking, is muted, is unmuted.");
    D("                        The valid values are:");
    D("                         -   0 - Never");
    D("                         -   5 - once in  5*20 ms (10 times per second)");
    D("                         -  10 - once in 10*20 ms ( 5 times per second)");
    D("                         -  50 - once in 50*20 ms ( 1 time  per second)");
    D("                         - 100 - on participant state change (this is the default)");
    D("                        If this parameter is setted up, participant property events starts to logging to console.");
    D("    -langs pref_langs   Comma separated list of preferred languages (3 max). Example: en,ru,ch");
    D("    -noautoaccept       Do not auto-accept buddy requests (causes evt_subscription events to be generated)");
    D("");
    D("Additional Notes:");
    D("    Implements vx_req_account_anonymous_login_t. The success of this request guarantees a new account");
    D("    handle that may be used to perform account level commands until the handle is invalidated. Only");
    D("    connector, auxiliary and application-only commands may be performed until a user is signed in.");
    D("    This application does not support multiple signed in users; this is not a restriction of the");
    D("    Vivox SDK.");
    DECLARE_COMMAND(login, "[-u username] [-ah account_handle] [-d Display_Name] [-freq (0 | 5 | 10 | 50 | 100)] [-langs pref_langs]", "Used to login to the Vivox network.");
    // logout
    D("State: Requires an account handle (via 'login' command).");
    D("");
    D("Arguments:");
    D("    -ah account_handle The account to log out. Defaults to the oldest active account");
    D("                       which is still logged in.");
    D("");
    D("Additional Notes:");
    D("    Implements vx_req_account_logout_t. Logging out will causing the SDK to unwind any open sessions");
    D("    and close them, and logout the signed in user. No further actions other than connector,");
    D("    auxiliary, and application-only commands can be taken until the user logs in again.");
    DECLARE_COMMAND(logout, "[-ah account_handle]", "Used to logout of the Vivox network.");
    // loginprops
    D("State: Requires an account handle (via 'login' command).");
    D("");
    D("Arguments:");
    D("    -freq <frequency>   This is an integer that specifies how often the SDK will send participant property events while in a channel.");
    D("                        If this is not set the default will be \"on state change\", which means that the events will be sent when");
    D("                        the participant starts talking, stops talking, is muted, is unmuted.");
    D("                        The valid values are:");
    D("                         -   0 - Never");
    D("                         -   5 - once in  5*20 ms (10 times per second)");
    D("                         -  10 - once in 10*20 ms ( 5 times per second)");
    D("                         -  50 - once in 50*20 ms ( 1 time  per second)");
    D("                         - 100 - on participant state change (this is the default)");
    D("    -ah account_handle  The account to operate on. Defaults to the oldest active account.");
    D("");
    D("Additional Notes:");
    D("    Implements vx_req_account_set_login_properties_t.");
    DECLARE_COMMAND(loginprops, "[-freq (0 | 5 | 10 | 50 | 100)] [-ah account_handle]", "Used to set participant update frequency.");
    // media
    D("State: Requires a session handle (via 'addsession' command).");
    D("");
    D("Arguments:");
    D("    -connect            Implements vx_req_session_media_connect_t. Adds audio to the session.");
    D("    -disconnect         Implements vx_req_session_media_disconnect_t. Removes audio from the session.");
    D("    -sh session_handle  The session to add or remove audio in. Defaults to most recent added.");
    DECLARE_COMMAND(media, "-connect|-disconnect [-sh session_handle]", "Connect or disconnect the audio capability of a session.");
    // message
    D("State: Requires a session handle (via 'addsession' command).");
    D("");
    D("Arguments:");
    D("    -m message          Implements vx_req_session_send_message_t. Send a group chat message.");
    D("    -sh session_handle  The session to send the message to. Defaults to most recent added.");
    D("    -u user             URI of the participant to send the message to.");
    D("    -n number_of_msgs   Generate the specified number of messages. Default is 1. If number_of_messages > 1, then");
    D("                        message index starting with '1' will be added to the specified message text. Useful for");
    D("                        quickly generating a large amount of messages during experiments.");
    D("    -appns URN          Adds custom application namespace.  Example: urn:mytitle:myapp:ping");
    D("    -appdata XML        Adds custom application XML data.  Example: <t>1534730712</t>");
    D("    -ah account_handle  The account to operate on. Defaults to the oldest active account.");
    D("");
    D("Additional Notes:");
    D("    The underlying request will fail if your text capability is disabled in the session.");
    DECLARE_COMMAND(message, "-m message [-u user|[-sh session_handle]] [-n number_of_msgs] [-a custom application namespace URN] [-d custom application XML data] [-ah account_handle]", "Send a text message(s) to a specified account or session.");
    // move
    D("Default Behavior: Print the user's current position and heading.");
    D("State: Requires a session handle (via 'addsession' command). See Additional Notes.");
    D("");
    D("Arguments:");
    D("    -origin             Sets user position to the origin (0x,0y,0z), but does not affect heading.");
    D("    -forward d          Moves user 'd' units along the negative Z axis. 'd' is a non-zero float.");
    D("    -back d             Moves user 'd' units along the positive Z axis. 'd' is a non-zero float.");
    D("    -left d             Moves user 'd' units along the negative X axis. 'd' is a non-zero float.");
    D("    -right d            Moves user 'd' units along the positive X axis. 'd' is a non-zero float.");
    D("    -up d               Moves user 'd' units along the positive Y axis. 'd' is a non-zero float.");
    D("    -down d             Moves user 'd' units along the negative Y axis. 'd' is a non-zero float.");
    D("    -turnleft d         Rotates user heading 'd' degrees counter-clockwise. 'd' is a non-zero float.");
    D("    -turnright d        Rotates user heading 'd' degrees clockwise. 'd' is a non-zero float.");
    D("    -sh session_handle  The session to move or rotate in. Defaults to most recent added.");
    D("    -ah account_handle  The account to operate on. Defaults to the oldest active account.");
    D("");
    D("Additional Notes:");
    D("    Every usage of this command implements vx_req_session_set_3d_position_t. In a typical game");
    D("    integration using 3D positional audio, player position and orientation is calculated and updated");
    D("    several times a second and this request is called once each time to update the location of the");
    D("    virtual 'mouth' and 'ears'. Unlike most commands that result in SDK requests, arguments to this");
    D("    command other than session_handle do not relate to fields in the request struct, instead");
    D("    allowing the user to move and rotate around in the 3D channel. In order for the");
    D("    session_set_3d_position request to succeed, you must be in a 3D positional channel. To notice a");
    D("    change in your audio experience while moving within the sample application, you will need more");
    D("    than one player in the channel.");
    D("    Take a look at the 'dance' command for an example of more complex movement.");
    D("    IMPORTANT: neither you no other participants will be heard in 3D channel until setting 3D position first!");
    D("               Use e.g. 'move -origin' command after joining 3D channel to set your initial position.");
    DECLARE_COMMAND(move, "[-origin|-forward d|-back d|-left d|-right d|-up d|-down d|-turnleft d|-turnright d] [-sh session_handle]", "Print or change your position or heading in a 3D channel.");
    // dance
    D("Default Behavior: starts moving the user around the origin in the 3D channel. Allows all others to hear the user's voice in motion");
    D("State: Requires a session handle (via 'addsession' command). See Additional Notes.");
    D("");
    D("Arguments:");
    D("    -sh session_handle      The session to move or rotate in. Defaults to most recent added.");
    D("    -stop                   Stops the dance.");
    D("    -ms update_milliseconds Update 3D position each <update_milliseconds> interval.");
    D("                            Defaults to 100 ms (10 updates per second).");
    D("    -rmin min_distance      Sets the minimum distance. The distance will oscillate from minimum to");
    D("                            maximum with the period specified with -o parameter. Defaults to 1.");
    D("    -rmax max_distance      Sets the maximum distance. The distance will oscillate from minimum to");
    D("                            maximum with the period specified with -o parameter. Defaults to 10.");
    D("    -r distatance           Use the fixed radius for rotation. Effectively turns off distance oscillation.");
    D("                            Just a shorthand notation for '-rmin distance -rmax distance'.");
    D("    -a angualr_velocity_in_deg_per_sec Rotation speed in degrees per second. Use 0 to turn the rotation off.");
    D("                                       Defaults to 10 degrees per second (full circle in 36 seconds).");
    D("    -o oscillation_period_in_seconds   The distance oscillation period in seconds. Defaults to 9 seconds.");
    D("    -c x0 y0 z0             Rotate around the specified point. Defaults to origin: (0, 0, 0).");
    D("");
    D("Additional Notes:");
    D("    See 'move' command notes for details and requirements on moving in 3D channels.");
    D("    The 'dance' command starts a separate thread which updates the user's position in the 3D positional");
    D("    channel with vx_req_session_set_3d_position_t request. It moves the user in X-Z plane around the specified");
    D("    point (rotation center) with the specified constant angular velocity (possibly 0). The distance from the");
    D("    rotation center (rotation radius) oscillates between the specified minimum and maximum values with");
    D("    the specified period.");
    D("    Command examples:");
    D("    1. Start rotation with default parameters:");
    D("         dance");
    D("    2. Rotate around the origin with the fixed radius:");
    D("         dance -r 1");
    D("    3. Move forward and backward with 20 seconds period without rotation:");
    D("         dance -rmin 0 -rmax 10 -a 0 -o 20");
    D("    To notice a change in your audio experience while moving within the sample application, you will need more");
    D("    than one player in the channel.");
    DECLARE_COMMAND(dance, "[-sh session_handle] [-stop] [-ms update_milliseconds] [[-r distatance]|[-rmin min_distance -rmax max_distance]] [-a angualr_velocity_in_deg_per_sec] [-o oscillation_period_in_seconds] [-c x0 y0 z0]", "Starts/stops moving the user's position in 3D channel"); // "(3D channels only) Starts/stops rotating the user's position around the specified location with the specified angualr velocity and oscillation parameters"
    // muteall
    D("Default Behavior: Mute all current participants in a channel except yourself.");
    D("State: Requires an account handle (via 'login' command).");
    D("");
    D("Arguments:");
    D("    -c channel             URI of the channel whose participants you want muted or unmuted. Defaults to most recent added.");
    D("    -sh session_handle     The session to move or rotate in. Defaults to most recent added.");
    D("    -mute|-unmute          Implements vx_req_channel_mute_all_users_t. Flag indicates mute state.");
    D("                           Default is muted.");
    D("    -scope audio|text|all  What should be muted, only audio, only text messages or both; default: all.");
    D("    -ah account_handle     The account to operate on. Defaults to the oldest active account.");
    D("");
    D("Additional Notes:");
    D("    This command does not require you to be in the channel, but will not affect you if you are.");
    DECLARE_COMMAND(muteall, "[-c channel|-sh session_handle] [-mute|-unmute] [-ah account_handle]", "Mute or unmute all current participants in a channel except yourself.");
    // muteforme
    D("Default Behavior: Mute the specified channel participant just for you.");
    D("State: Requires a session handle (via 'addsession' command).");
    D("");
    D("Arguments:");
    D("    -u user                URI of the participant to be muted or unmuted.");
    D("    -mute|-unmute          Implements vx_req_session_set_participant_mute_for_me_t. Flag indicates mute");
    D("                           state. Default is muted.");
    D("    -scope audio|text|all  What should be muted, only audio, only text messages or both; default: all.");
    D("    -sh session_handle     The session to mute or unmute participant in. Defaults to most recent added.");
    DECLARE_COMMAND(muteforme, "-u user [-mute|-unmute] [-scope text|audio|all] [-sh session_handle]", "Mute or unmute a channel participant just for you in specified session only.");
    // mutelocal
    D("Default Behavior: Mute your own mic globally.");
    D("");
    D("Arguments:");
    D("    -mic               Implements vx_req_connector_mute_local_mic_t. Sets mic mute state globally.");
    D("    -speaker           Implements vx_req_connector_mute_local_speaker_t. Sets speaker mute state globally.");
    D("    -mute|-unmute      Flag indicates mute state. Default is muted.");
    D("    -ah account_handle The account to operate on. Defaults to the oldest active account.");
    DECLARE_COMMAND(mutelocal, "[-mic|-speaker] [-mute|-unmute] [-ah account_handle]", "Mute or unmute your own mic or speaker globally.");
    // muteuser
    D("Default Behavior: Mute the specified channel participant.");
    D("State: Requires an account handle (via 'login' command).");
    D("");
    D("Arguments:");
    D("    -u user                URI of the participant to be muted or unmuted.");
    D("    -c channel             URI of the channel wherein the participant is to be muted or unmuted.");
    D("    -mute|-unmute          Implements vx_req_channel_mute_user_t. Flag indicates mute state.");
    D("                           Default is muted.");
    D("    -scope audio|text|all  What should be muted, only audio, only text messages or both; default: all.");
    D("    -ah account_handle     The account to operate on. Defaults to the oldest active account.");
    DECLARE_COMMAND(muteuser, "-u user -c channel [-mute|-unmute] [-ah account_handle]", "Mute or unmute a channel participant for everyone.");
    // blockusers
    D("Default Behavior: Mute specified participants in all sessions.");
    D("State: Requires an account handle (via 'login' command).");
    D("");
    D("Arguments:");
    D("    -u user1           URI of the participant to be muted or unmuted.");
    D("    -u userN           You can specify several users.");
    D("    -mute|-unmute|-block|-unblock|-listblocked|-listmuted|-clearblocked|-clearmuted");
    D("                       Implements vx_req_account_control_communications. Flag indicates required operation.");
    D("                       Default is -block.");
    D("    -ah account_handle The account to operate on. Defaults to the oldest active account.");
    DECLARE_COMMAND(blockusers, "-ah account_handle [-u user1 -u userN] [-block|-unblock|-mute|-unmute|-listblocked|-listmuted|-clearblocked|-clearmuted]", "Mute, unmute or totally block (cross mute) a participant(s) just for you in all current sessions and in future sessions until unmuted.");
    // presence
    D("Default Behavior: Sets presence to 'buddy_presence_online' with no status message.");
    D("State: Requires an account handle (via 'login' command).");
    D("");
    D("Arguments:");
    D("    -available|-dnd|-chat|-away|-ea    Sets buddy presence to the specified state.");
    D("    -m message                                      Sets a custom status message.");
    D("");
    D("Additional Notes:");
    D("    Implements vx_req_account_set_presence_t. Refer to \"Vivox SDK Sample Application.pdf\" for");
    D("    information on buddy presence.");
    DECLARE_COMMAND(presence, "[-available|-dnd|-chat|-away|-ea] [-m message]", "Set the presence state and/or status message of your account.");
    // mutesession
    D("Default Behavior: Mute the specified session audio or/and text.");
    D("State: Requires a session handle (via 'addsession' command).");
    D("");
    D("Arguments:");
    D("    -sh session_handle     The session to mute or unmute text and/or speaker.");
    D("    -mute|-unmute          Implements vx_req_session_mute_local_speaker_t. Flag indicates mute state.");
    D("                           Default: mute.");
    D("    -scope audio|text|all  What should be muted, only audio, only text messages or both; default: all.");
    DECLARE_COMMAND(mutesession, "[-sh] [-mute|-unmute]", "Mute or unmute audio rendering for specified session.");
    // DECLARE_COMMAND(mutesession, "[-sh] [-text|-audio|-all] [-mute|-unmute]", "Mute or unmute your own mic or speaker globally.");
    // removesession
    D("State: Requires an account handle (via 'login' command).");
    D("");
    D("Arguments:");
    D("    -sh session_handle  The session to remove. Defaults to most recent added.");
    D("");
    D("Additional Notes:");
    D("    Implements vx_req_sessiongroup_remove_session_t. Terminating the session will destroy the");
    D("    session handle specified by '-sh' argument. If this leaves the session's associated sessiongroup");
    D("    with no active session, the sessiongroup will also be terminated. Commands requiring a session");
    D("    handle or sessiongroup handle may not be used once all sessions and sessiongroups have been");
    D("    removed, until one is added again with 'addsession'.");
    DECLARE_COMMAND(removesession, "[-sh session_handle]", "Disconnect from an established group text and/or audio conference.");
    // renderaudio
    D("Arguments:");
    D("    -start filename.wav Implements vx_req_aux_render_audio_start_t. Starts playing filename.wav on a render device.");
    D("    -loop               yes|no  loop mode: no (default) - play once, yes - loop infinitely.");
    D("    -stop               Implements vx_req_aux_capture_audio_stop_t. Stops audio render immediately.");
    D("    -ah account_handle  The account to operate on. Defaults to the oldest active account.");
    D("");
    DECLARE_COMMAND(renderaudio, "-start filename.wav [-loop yes|no] | -stop [-ah account_handle]", "Render an audio file on a render device.");
    // renderdevice
    D("Default Behavior: Prints the device ID of the current, default, and effective render device, and");
    D("                  device IDs of all available render devices.");
    D("");
    D("Arguments:");
    D("    -get               Implements vx_req_aux_get_render_devices_t. Prints device IDs for current,");
    D("                       default, effective, and all available render devices");
    D("                       in the form: [<index>] <display_name> (<device_id>).");
    D("    -set device_id     Implements vx_req_aux_set_render_device_t. Sets new render device to");
    D("                       'device_id', a string matching a device ID returned by 'renderdevice -get'.");
    D("    -i index           Same as '-set', but refers to the device by its index rather than by its device id.");
    D("                       Use '-get' to list all available render devices and their indicies.");
    D("    -ah account_handle The account to operate on. Defaults to the oldest active account.");
    D("");
    D("Additional Notes:");
    D("    Refer to \"Vivox SDK Sample Application.pdf\" for information on different types of audio devices.");
    DECLARE_COMMAND(renderdevice, "[-get] [-set device_id] [-i index] [-ah account_handle]", "Get current and available render devices or set the render device globally.");
    // shutdown
    D("State: Requires a connector handle (via 'connect' command).");
    D("");
    D("Additional Notes:");
    D("    Implements vx_req_connector_initiate_shutdown_t. Shutting down will causing the SDK to unwind");
    D("    any open sessions and close them, logout if the user is logged in, and remove the connector");
    D("    object. No further actions other than auxiliary or application-only commands can be taken until");
    D("    a new connector object is created.");
    DECLARE_COMMAND(shutdown, "", "Disconnect the application from the Vivox service.");
    // state
    D("Additional Notes:");
    D("    Much information about the SDK can be obtained with querying commands like 'buddy -list' or");
    D("    'capturedevice -get'. These commands send SDK requests to retrieve information, which is then");
    D("    printed by the sample application in the response handler. Some information about the SDK cannot");
    D("    be actively queried, and is only relayed to the application when a change occurs. For these");
    D("    cases it is up to the application to track that information, if access to it is later required.");
    D("    Generally, this includes handles to SDK objects, as well as information contained in SDK events,");
    D("    such as a participant joining a channel, or changes in media capability within a session.");
    D("");
    D("    Information stored by this sample application is updated whenever the Vivox SDK reports changes,");
    D("    but since this command is not actively querying the SDK, it may in rare cases be incorrect.");
    DECLARE_COMMAND(state, "", "Print local state tracked by the application, like handles and rosters.");
    // text
    D("State: Requires a session handle (via 'addsession' command).");
    D("");
    D("Arguments:");
    D("    -connect            Implements vx_req_session_text_connect_t. Adds text to the session.");
    D("    -disconnect         Implements vx_req_session_text_disconnect_t. Removes text from the session.");
    D("    -sh session_handle  The session to add or remove text in. Defaults to most recent added.");
    DECLARE_COMMAND(sstats, "", "Calls vx_get_system_stats");
    // sstats
    D("");
    D("");
    DECLARE_COMMAND(text, "-connect|-disconnect [-sh session_handle]", "Connect or disconnect the text capability of a session.");
    // transmit
    D("Default Behavior: Sets audio transmission to only* most recent channel added.");
    D("State: Requires sessiongroup handle for '-none' and '-all' flags. Requires session handle otherwise.");
    D("");
    D("Arguments:");
    D("    -none                   Implements vx_req_sessiongroup_set_tx_no_session_t. Transmit audio to no");
    D("                            sessions in sessiongroup specified by '-gh' argument.");
    D("    -all                    Implements vx_req_sessiongroup_set_tx_all_sessions_t. Transmit audio to");
    D("                            all sessions in sessiongroup specified by '-gh' argument.");
    D("    -sh session_handle      Implements vx_req_sessiongroup_set_tx_session_t. If '-none' and '-all'");
    D("                            are omitted, indicates session where you will exclusively* transmit");
    D("                            audio. Defaults to most recent added.");
    D("    -gh sessiongroup_handle Used with '-none' or '-all'. Indicates sessiongroup where you will set");
    D("                            the new transmission mode. Defaults to most recent added.");
    D("");
    D("Additional Notes:");
    D("    *This command affects audio transmission only within the scope of a single sessiongroup. Setting");
    D("    audio transmission to a single session will disable transmission to other sessions within the");
    D("    same sessiongroup only. Audio may still be transmitted to sessions in other sessiongroups.");
    DECLARE_COMMAND(transmit, "[-none|-all] [-sh session_handle] [-gh sessiongroup_handle]", "Set the session(s) within a sessiongroup your voice is transmitted to.");
    // vadproperties
    D("Default Behavior: Get the current VAD properties used by the Vivox System.");
    D("");
    D("Arguments:");
    D("    -get               Implements vx_req_aux_get_vad_properties_t. Gets and prints VAD properties.");
    D("    -set               Implements vx_req_aux_set_vad_properties_t. Sets one or more VAD properties");
    D("                       specified by '-h' '-s' and '-n' arguments. If '-set' is used with no specific");
    D("                       properties, all three will be set to their default values; otherwise only those");
    D("                       specified will be changed.");
    D("    -h hangover        Used with '-set'. hangover is an integer representing time in milliseconds.");
    D("    -s sensitivity     Used with '-set'. sensitivity is an integer between 0 and 100.");
    D("    -n noisefloor      Used with '-set'. noisefloor is an integer between 0 and 20000.");
    D("    -a auto            Used with '-set'. auto is either 0 or 1.");
    D("    -ah account_handle The account to operate on. Defaults to the oldest active account.");
    D("");
    D("Additional Notes:");
    D("    The 'VAD hangover time' is the time (in milliseconds) that it takes for the VAD to switch back to");
    D("    silence from speech mode after the last speech frame has been detected. Default is 2000.");
    D("");
    D("    The 'VAD sensitivity' is a dimensionless value between 0 and 100, indicating the sensitivity of");
    D("    the VAD. Increasing this value corresponds to decreasing the sensitivity of the VAD (i.e. '0'");
    D("    is most sensitive, while '100' is least sensitive). Default is 43.");
    D("");
    D("    The 'VAD noise floor' is a dimensionless value between 0 and 20000 that controls how the VAD");
    D("    separates speech from background noise. Default is 576.");
    DECLARE_COMMAND(vadproperties, "[-get|-set] [-h hangover] [-s sensitivity] [-n noisefloor] [-a auto] [-ah account_handle]", "Get or set the global VAD (Voice Activity Detector) properties.");
    // derumblerproperties
    D("Default Behavior: Get the current derumbler (high-pass filter) properties used by the Vivox System.");
    D("");
    D("Arguments:");
    D("    -get                                 Implements vx_req_aux_get_derumbler_properties_t. Gets and prints derumbler properties.");
    D("    -set                                 Implements vx_req_aux_set_derumbler_properties_t. Sets one or more derumbler properties");
    D("                                         specified by '-on'/'-off' and 'freq' arguments. If '-set' is used with no specific");
    D("                                         properties, all parameters will be set to their default values; otherwise only those");
    D("                                         specified will be set as desired, and those unspecified will be set to default.");
    D("    -on                                  Used with '-set'. -on enables the derumbler.");
    D("    -off                                 Used with '-set'. -off disables the derumbler.");
    D("    -freq stopband_corner_frequency      Used with '-set'. Freq in Hz where stopband is in full effect. Default is 60. 15 and 100 are also valid choices.");
    D("    -ah                                  account_handle The account to operate on. Defaults to the oldest active account.");
    D("");
    D("Additional Notes:");
    D("    The 'derumbler' heavily reduces rumble and other undesirable low frequency noise from");
    D("    being transmitted. It is enabled by default with a stopband corner frequency of 60 Hz.");
    DECLARE_COMMAND(derumblerproperties, "[-get|-set] [-on/-off] [-freq stopband_corner_frequency] [-ah account_handle]", " Get or set the derumbler (high-pass filter) properties.");
    // volumeforme
    D("Default Behavior: Set the volume of a channel participant to 50 just for you.");
    D("");
    D("Arguments:");
    D("    -u user             URI of the participant to set the volume of.");
    D("    -v volume           The new integer value for the level of the channel participant. Default is");
    D("                        50 (+0db, which represents 'normal' speaking volume).");
    D("    -sh session_handle  The session to set the volume of the participant in. Defaults to most recent");
    D("                        added.");
    D("");
    D("Additional Info:");
    D("    Implements vx_req_session_set_participant_volume_for_me_t. Volume settings for participants in");
    D("    channel have a settable range between 0-100 on a logarithmic scale with 0db setting being at");
    D("    50. Because it is a logarithmic scale, in general the usable range is between 40 and 75, with 75");
    D("    tending towards a blown out sound and possible pain on a headset.");
    DECLARE_COMMAND(volumeforme, "-u user [-v volume] [-sh session_handle]", "Set the volume of a channel participant just for you.");
    // sessionVolumeforme
    D("Default Behavior: Set the volume of all channel participants to 50 just for you.");
    D("");
    D("Arguments:");
    D("    -v volume           The new integer value for the level of the channel participant. Default is");
    D("                        50 (+0db, which represents 'normal' speaking volume).");
    D("    -sh session_handle  The session to set the volume of the participant in. Defaults to most recent");
    D("                        added.");
    D("");
    D("Additional Info:");
    D("    Implements vx_req_session_set_participant_volume_for_me_t. Volume settings for participants in");
    D("    channel have a settable range between 0-100 on a logarithmic scale with 0db setting being at");
    D("    50. Because it is a logarithmic scale, in general the usable range is between 40 and 75, with 75");
    D("    tending towards a blown out sound and possible pain on a headset.");
    DECLARE_COMMAND(sessionvolumeforme, "[-v volume] [-sh session_handle]", "Set the volume of all channel participants just for you.");
    // volumelocal
    D("Default Behavior: Get the volume of your own master speaker.");
    D("");
    D("Arguments:");
    D("    -get               Implements vx_req_aux_get_mic_level_t or vx_req_aux_get_speaker_level_t.");
    D("                       Gets master mic or speaker level.");
    D("    -set               Implements vx_req_aux_set_mic_level_t or vx_req_aux_set_speaker_level_t.");
    D("                       Sets master mic or speaker level to volume specified by '-v' argument.");
    D("    -mic|speaker       Flag indicates 'mic or 'speaker' version of get or set level requests.");
    D("    -v volume          Used with '-set'. volume is the new integer value for the level of the mic or");
    D("                       speaker. Default is 50 (+0db, which represents 'normal' speaking volume).");
    D("    -ah account_handle The account to operate on. Defaults to the oldest active account.");
    D("");
    D("Additional Info:");
    D("    Volume settings for both microphone and speakers have a settable range between 0-100 on a");
    D("    logarithmic scale with 0db setting being at 50. Because it is a logarithmic scale, in general");
    D("    the usable range is between 40 and 75, with 75 tending towards over modulation on a microphone");
    D("    and possible pain on a headset.");
    DECLARE_COMMAND(volumelocal, "[-get|-set] [-mic|-speaker] [-v volume] [-ah account_handle]", "Get or set the volume of your own mic or speaker globally.");
    // archive
    D("Default Behavior: Get messages from the currently logged in user's message archive (text messages history for the account).");
    D("State: Requires an account handle (via 'login' command).");
    D("");
    D("Arguments:");
    D("    -u user           Return only messages exchanged with the specified user. Mutually exclusive with -c");
    D("    -c channel_uri    Return only messages exchanged within the specified channel. Mutually exclusive with -u");
    D("    -max number       Maximum number of messages to return.");
    D("                      By default the result set is limited by 10 messages.");
    D("    -start time       Return messages exchanged no earlier than the time specified (in ISO 8601).");
    D("    -end time         Return messages exchanged before the time specified (in ISO 8601).");
    D("    -search text      Return only messages containing the requested text.");
    D("    -after id         Used for paging of the large message archives. Return messages after message with the given id.");
    D("    -before id        Used for paging of the large message archives. Return messages before message with the given id.");
    D("    -index number     Used for paging of the large message archives. Return messages after message with the given index.");
    D("");
    D("Additional Info:");
    D("    Implements vx_req_account_archive_query_t for the currently logged in user.");
    DECLARE_COMMAND(archive, "[-u user|-c channel_uri] [-max number] [-start time] [-end time] [-search text] [-after id|-before id] [-index number]", "Get account's message history.");
    // history
    D("Default Behavior: Get the channel message history.");
    D("State: Requires a session handle (via 'addsession' command).");
    D("");
    D("Arguments:");
    D("    -sh session_handle  The session to set the volume of the participant in. Defaults to most recent");
    D("                        added.");
    D("    -u user             Return only messages exchanged with the specified user.");
    D("    -max number         Maximum number of messages to return.");
    D("                        By default the result set is limited by 10 messages.");
    D("    -start time         Return messages exchanged no earlier than the time specified (in ISO 8601).");
    D("    -end time           Return messages exchanged before the time specified (in ISO 8601).");
    D("    -search text        Return only messages containing the requested text.");
    D("    -after id           Used for paging of the large message archives. Return messages after message with the given id.");
    D("    -before id          Used for paging of the large message archives. Return messages before message with the given id.");
    D("    -index number       Used for paging of the large message archives. Return messages after message with the given index.");
    D("");
    D("Additional Info:");
    D("    Implements vx_req_session_archive_query_t for the specified channel.");
    DECLARE_COMMAND(history, "[-sh session_handle] [-u user] [-max number] [-start time] [-end time] [-search text] [-after id|-before id] [-index number]", "Get channel's message history.");
    // rtp
    D("Default Behavior: Displays the current state of debug RTP restrictions.");
    D("State: none");
    D("");
    D("Arguments:");
    D("    -send <0|1>         Disables (0) or enables (1) sending of RTP traffic by the SDK.");
    D("    -recv <0|1>         Disables (0) or enables (1) receiving of RTP traffic by the SDK.");
    D("");
    D("Additional Info:");
    D("    Implements vx_req_session_archive_query_t for the specified channel.");
    DECLARE_COMMAND(rtp, "[-send <0|1>] [-recv <0|1>]", "Displays or sets the debug RTP restrictions. Used to artificially create 'no RTP' cases.");


// Looks like sleep is implemented out of band in main.cpp on other platforms, so selectively compile in here
    DECLARE_COMMAND(sleep, "", "seconds to sleep");

    // media
    D("State: Requires a session handle (via 'addsession' command).");
    D("");
    D("Arguments:");
    D("    -on                 Enable speech-to-text transcription for session.");
    D("    -off                Disable speech-to-text transcription for session.");
    D("    -sh session_handle  The session to add or remove audio in. Defaults to most recent added.");
    D("    -ah account_handle  The account to operate on. Defaults to the oldest active account.");
    DECLARE_COMMAND(transcription, "-on|off [-sh session_handle]", "Enable/disable speech-to-text transcription for session.");

    // audioinfo
    D("State: Requires a connector handle (via 'connect' command).");
    DECLARE_COMMAND(audioinfo, "[-ah account_handle]", "Shows current mic and speaker state (muted/no, volume).");

    // quit is a special case since it has no CommandFunction
    D("Additional Notes:");
    D("    This command may be issued at any time, causing the SDK to unwind any open sessions and close");
    D("    them, logout if the user is logged in, shutdown the connector object, and exit the application.");
    m_commands.insert(make_pair("quit", Command("quit", (Command::CommandFunction)NULL, "", "Uninitialize the Vivox SDK and exit the application.", ss.str())));
    ss.str(string());


    // aec
    D("Default Behavior: Show AEC state.");
    D("");
    D("Arguments:");
    D("    -on                Enable AEC.");
    D("    -off               Disable AEC.");
    D("");
    DECLARE_COMMAND(aec, "[on|off]", "Enable/disable AEC");

    // agc
    D("Default Behavior: Show AGC state.");
    D("");
    D("Arguments:");
    D("    -on                Enable AGC.");
    D("    -off               Disable AGC.");
    D("");
    DECLARE_COMMAND(agc, "[on|off]", "Enable/disable AGC");


#ifndef SAMPLEAPP_DISABLE_TTS
    // Text-to-speech
    D("Synthesizes and injects a text-to-speech message.");
    D("");
    D("Arguments:");
    D("  -dest destination  The destination of the message. Possible values are:");
    D("    0    tts_dest_remote_transmission");
    D("    1    tts_dest_local_playback");
    D("    2    tts_dest_remote_transmission_with_local_playback");
    D("    3    tts_dest_queued_remote_transmission");
    D("    4    tts_dest_queued_local_playback");
    D("    5    tts_dest_queued_remote_transmission_with_local_playback");
    D("    6    tts_dest_screen_reader");
    D("");
    D("  -text text     The text to speak as a synthesized audio message.");
    DECLARE_COMMAND(ttsspeak, "-dest destination [-text text]", "Converts the provided text parameter to speech and injects it into the specified destination.");

    D("Default Behavior: Cancels all text-to-speech messages in all destinations.");
    D("");
    D("Arguments:");
    D("  -dest destination  When specified, this cancels all utterances that were injected to the chosen destination. Possible values are:");
    D("    0    tts_dest_remote_transmission");
    D("    1    tts_dest_local_playback");
    D("    2    tts_dest_remote_transmission_with_local_playback");
    D("    3    tts_dest_queued_remote_transmission");
    D("    4    tts_dest_queued_local_playback");
    D("    5    tts_dest_queued_remote_transmission_with_local_playback");
    D("    6    tts_dest_screen_reader");
    D("  -id id     When specified, this cancels the TTS message with the matching ID. The utterance ID is obtained when calling ttsspeak.");
    DECLARE_COMMAND(ttscancel, "[-dest destination] | [-id id]", "Cancels a specific text-to-speech message, or many text-to-speech messages.");

    D("Default Behavior: Calls vx_tts_shutdown with the SDKSampleApp's TTS engine if it has been initialized.");
    DECLARE_COMMAND(ttsshutdown, "", "Shuts down the TTS engine if it has been initialized.");

    D("Default Behavior: Lists the available voices for TTS.");
    D("");
    D("Arguments:");
    D("  voice_id   The unique ID of the voice to use.");
    DECLARE_COMMAND(ttsvoice, "[voice_id]", "Manage voices for TTS.");

    D("State: Requires a session handle (via 'addsession' command).");
    D("");
    D("Arguments:");
    D("    -on                 Enable text-to-speech reading of messages for the session.");
    D("    -off                Disable text-to-speech reading of messages for the session.");
    D("    -sh session_handle  The session to add or remove text-to-speech message reading in. Without this, the command defaults to the most recently added session.");
    DECLARE_COMMAND(ttsspeaktextchannel, "-on|off [-sh session_handle]", " Enable/disable text-to-speech reading of messages for a session.");
#endif

    // set backend target values
    D("Default Behavior: Get the current values for server, realm, issuer, key, and multitenant.");
    D("State: Requires SDK to be disconnected to change values.");
    D("");
    D("Arguments:");
    D("    -s server          Vivox API URL (e.g. https://vdx5.www.vivox.com/api2.");
    D("    -r realm           Vivox domain (e.g. vdx5.vivox.com).");
    D("    -i issuer          Token signing issuer.");
    D("    -k key             Token signing key.");
    D("    -m multitenant     Use issuer prefix when generating random user and channel IDs.");
    D("");
    DECLARE_COMMAND(backend, "[-s server] [-r realm] [-i issuer] [-k key] [-m yes|no]", "Get the current values for server, realm, issuer, key, and multitenant.");
}

void PrintUsage(string cmd, string text)
{
    SDKSampleApp::con_print("Usage: %s %s\n", cmd.c_str(), text.c_str());
}

void SDKSampleApp::help(const vector<string> &cmd)
{
    if (cmd.size() > 1) {
        con_print("Commands matching '%s'\n", cmd.at(1).c_str());
        int found = 0;
        for (CommandMapConstItr itr = m_commands.begin(); itr != m_commands.end(); ++itr) {
            if (cmd.at(1).empty() || itr->first.find(cmd.at(1)) != string::npos) {
                con_print("\t%-15s%s\n", itr->second.GetName().c_str(), itr->second.GetDescription().c_str());
                ++found;
            }
        }
        if (!found) {
            con_print("\nType 'help' by itself to print all commands, and for general help.");
        }
        con_print("\nFor usage and extended documentation, type the name of a command followed by '-help'\n");
    } else {
        con_print("Quickstart - The following commands, with no arguments, will put you in an echo channel:\n");
        con_print("\tconnect\n");
        con_print("\tlogin\n");
        con_print("\taddsession\n");
        con_print("\n");
        con_print("The following commands are valid: \n");
        for (CommandMap::const_iterator itr = m_commands.begin(); itr != m_commands.end(); ++itr) {
            con_print("\t%-15s%s\n", itr->second.GetName().c_str(), itr->second.GetDescription().c_str());
        }
        con_print("\n");
        con_print("Use 'help STRING' to find all commands whose name contains STRING.\n");
        con_print("For usage and extended documentation, type the name of any command followed by '-help'\n");
        con_print("Commands may be performed by typing any unique prefix of the command's name.\n");
    }
}

bool SDKSampleApp::ProcessCommand(const std::vector<std::string> &args)
{
    if (args.empty()) {
        return true;
    }
    const std::string &cmd = args[0];
    // Try map::find first so complete commands that prefix others (e.g. 'log' prefixes 'login')
    // work correctly. This is also more efficient if commands tend to be typed fully anyways.
    CommandMapConstItr command = m_commands.find(cmd);
    if (m_commands.end() == command) {
        // When the whole command isn't found, check if it's a unique prefix.
        int count = 0;
        for (CommandMapConstItr itr = m_commands.begin(); itr != m_commands.end(); ++itr) {
            if (cmd.size() >= itr->first.size()) {
                continue;
            }
            auto result = mismatch(cmd.begin(), cmd.end(), itr->first.begin());
            if (result.first == cmd.end()) {
                ++count;
                command = itr;
            }
            if (count > 1) {
                break;
            }
        }
        if (count != 1) {
            con_print("'%s' is not a valid command. Type 'help' for a list of valid commands.\n", cmd.c_str());
            return true;
        }
    }
    // If cmd has a match, print help or execute command function.
    if (args.size() > 1 && (args.at(1) == "-help" || args.at(1) == "--help")) {
        PrintUsage(command->second.GetName(), command->second.GetUsage());
        con_print("%s\n", command->second.GetDescription().c_str());
        if (!command->second.GetDocumentation().empty()) {
            con_print("\n%s", command->second.GetDocumentation().c_str());
        }
        con_print("\n");
    } else {
        if ("quit" == command->first) {
            return false; // 'quit' signals main loop break
        }
        vector<string> vargs = args;
        vargs[0] = command->second.GetName();
        (this->*(command->second.GetCommandFunction()))(vargs);
    }

    return true;
}

std::vector<std::string> SDKSampleApp::GetCommandsMatch(const std::string &text)
{
    std::vector<std::string> res;

    CommandMap::const_iterator itr;
    for (itr = m_commands.begin(); itr != m_commands.end(); ++itr) {
        size_t pos = itr->first.find(text);
        if (pos == 0) {
            res.push_back(itr->first);
        }
    }

    return res;
}

std::string SDKSampleApp::GetCommandUsage(const std::string &command)
{
    CommandMap::const_iterator itr;
    for (itr = m_commands.begin(); itr != m_commands.end(); ++itr) {
        if (itr->first == command) {
            return itr->second.GetUsage();
        }
    }

    return std::string();
}

void SDKSampleApp::connect(const vector<string> &cmd)
{
    string connectorHandle;
    bool error = false;
    unsigned int configured_codecs = 0;

    for (vector<string>::const_iterator i = cmd.begin() + 1; i != cmd.end(); ++i) {
        if (*i == "-ch") {
            if (!nextArg(connectorHandle, cmd, i, error)) {
                break;
            }
        } else if (*i == "-c" || *i == "-codecs") {
            if (!nextArg(configured_codecs, cmd, i, error)) {
                break;
            }
        } else {
            error = true;
            break;
        }
    }
    if (error) {
        PrintUsage(cmd.at(0), m_commands.find(cmd.at(0))->second.GetUsage());
        return;
    }
    con_print("\r * Connecting to %s with connector handle %s...\n", m_server.c_str(), connectorHandle.empty() ? "<AUTOGENERATE>" : connectorHandle.c_str());
    connector_create(m_server, connectorHandle, configured_codecs);
    con_print("\n%s\n", "Cummunication Termination!");
    vxplatform::set_event(listenerEvent);
}

void SDKSampleApp::participanteffect(const vector<string> &cmd)
{
    string user;
    bool isOn = true;
    bool error = false;

    for (vector<string>::const_iterator i = cmd.begin() + 1; i != cmd.end(); ++i) {
        if (*i == "-u") {
            if (!nextArg(user, cmd, i, error)) {
                break;
            }
        } else if (*i == "-on" || *i == "-off") {
            isOn = (*i == "-on");
        } else {
            error = true;
            break;
        }
    }

    if (error || user.empty()) {
        PrintUsage(cmd.at(0), m_commands.find(cmd.at(0))->second.GetUsage());
        return;
    }

    string participant_uri = GetUserUri(user);

    if (isOn) {
        m_participantsEffectTimes[participant_uri] = 0;
    } else {
        auto iter = m_participantsEffectTimes.find(participant_uri);
        if (iter == m_participantsEffectTimes.end()) {
            con_print("\r * Error: Participant effect isn't activated: cannot switch off.\n");
            return;
        }

        m_participantsEffectTimes.erase(participant_uri);
    }

    con_print("\r * Setting Audio Effect for User %s to %d\n", participant_uri.c_str(), isOn);
}
void SDKSampleApp::crash(const vector<string> &cmd)
{
    if (!vx_get_crash_dump_generation()) {
        con_print("\r * Crash dumps disabled. Skipped.\n");

        return;
    }

    if (cmd.size() <= 1) {
        con_print("\r * Crashing... access zero pointer\n");
        vx_crash_test(vx_crash_access_zero_pointer);

        return;
    }

    for (vector<string>::const_iterator i = cmd.begin() + 1; i != cmd.end(); ++i) {
        if (*i == "-zp") {
            con_print("\r * Crashing... access zero pointer\n");
            vx_crash_test(vx_crash_access_zero_pointer);
        } else if (*i == "-av") {
            con_print("\r * Crashing... access violation\n");
            vx_crash_test(vx_crash_access_violation);
        } else if (*i == "-so") {
            con_print("\r * Crashing... stack overflow\n");
            vx_crash_test(vx_crash_stack_overflow);
        } else if (*i == "-hc") {
            con_print("\r * Crashing... heap corruption\n");
            vx_crash_test(vx_crash_heap_corruption);
#if defined(__clang__)
        } else if (*i == "-bt") {
            con_print("\r * Crashing... builtin trap\n");
            vx_crash_test(vx_crash_builtin_trap);
#endif  // defined(__clang__)
        } else {
            PrintUsage(cmd.at(0), m_commands.find(cmd.at(0))->second.GetUsage());
        }
    }
}

void SDKSampleApp::log(const vector<string> &cmd)
{
    bool error = false;
    bool logRequests = m_logRequests;
    bool logResponses = m_logResponses;
    bool logEvents = m_logEvents;
    bool logXml = m_logXml;
    bool logParticipantUpdate = m_logParticipantUpdate;

    for (vector<string>::const_iterator i = cmd.begin() + 1; i != cmd.end(); ++i) {
        if (*i == "-requests") {
            if (!nextArg(logRequests, cmd, i, error)) {
                break;
            }
        } else if (*i == "-responses") {
            if (!nextArg(logResponses, cmd, i, error)) {
                break;
            }
        } else if (*i == "-events") {
            if (!nextArg(logEvents, cmd, i, error)) {
                break;
            }
        } else if (*i == "-xml") {
            if (!nextArg(logXml, cmd, i, error)) {
                break;
            }
        } else if (*i == "-pu") {
            if (!nextArg(logParticipantUpdate, cmd, i, error)) {
                break;
            }
        } else {
            error = true;
            break;
        }
    }
    if (error) {
        PrintUsage(cmd.at(0), m_commands.find(cmd.at(0))->second.GetUsage());
        return;
    }

    m_logRequests = logRequests;
    m_logResponses = logResponses;
    m_logEvents = logEvents;
    m_logXml = logXml;
    m_logParticipantUpdate = logParticipantUpdate;

    con_print(
            "\r * Logging requests %s, responses %s, events %s, xml %s, participant updated events %s\n",
            m_logRequests ? "enabled" : "disabled",
            m_logResponses ? "enabled" : "disabled",
            m_logEvents ? "enabled" : "disabled",
            m_logXml ? "enabled" : "disabled",
            m_logParticipantUpdate ? "enabled" : "disabled");
}

void SDKSampleApp::state(const vector<string> &cmd)
{
    if (cmd.size() > 1) {
        PrintUsage(cmd.at(0), m_commands.find(cmd.at(0))->second.GetUsage());
        return;
    }

    con_print("\r * Backend Settings:\n");
    con_print("\r * \tRealm: '%s'\n", m_realm.c_str());
    con_print("\r * \tServer: '%s'\n", m_server.c_str());
    con_print("\r * \tToken Signing Issuer: '%s'\n", m_accessTokenIssuer.c_str());
    con_print("\r * \tToken Signing Key: '%s'\n", m_accessTokenKey.c_str());
    con_print("\r * \tMulti-tenancy mode: %s\n", m_isMultitenant ? "ON" : "OFF");

    con_print("\r * \n");
    con_print("\r * Vivox Service Connections:\n");
    if (m_connectorHandle.empty()) {
        con_print("\r * \t<none>\n");
    } else {
        con_print("\r * \tConnector Handle: '%s'\n", m_connectorHandle.c_str());
    }

    {
        lock_guard<mutex> lock(m_accountHandleUserNameMutex);
        con_print("\r * \n");
        con_print("\r * Signed in User:\n");
        if (m_accountHandles.empty()) {
            con_print("\r * \t<none>\n");
        } else {
            for (auto &it : m_accountHandles) {
                con_print("\r * \tUsername: '%s'\n", m_accountHandleUserNames[it].c_str());
                con_print("\r * \tAccount Handle: '%s'\n", it.c_str());
            }
        }
    }

    con_print("\r * \n");
    con_print("\r * Active Session Groups and their sessions:\n");
    if (m_SSGHandles.empty()) {
        con_print("\r * \t<none>\n");
    } else {
        vector<SSGPair> sortedHandles = m_SSGHandles;
        sort(sortedHandles.begin(), sortedHandles.end());
        string previousSessionGroup;
        for (vector<SSGPair>::const_iterator itr = sortedHandles.begin(); itr != sortedHandles.end(); ++itr) {
            if (previousSessionGroup != itr->first) {
                previousSessionGroup = itr->first;
                con_print("\r * \tSession Group Handle: '%s'\n", itr->first.c_str());
            }
            con_print("\r * \t\tSession Handle: '%s'\n", itr->second.c_str());
            auto session = m_sessions.find(itr->second);
            if (session != m_sessions.end()) {
                con_print("\r * \t\tChannel URI: '%s'\n", session->second->GetUri().c_str());
                session->second->PrintParticipants("\r * \t\t\t%s\n");
            }
        }
    }

    con_print("\r * \n");
    con_print("\r * If the -sh or -gh options are ommited from commands that take them, the most\n");
    con_print("\r * recently added session group and session handle will be used. These are:\n");
    con_print("\r * \tDefault Session Group: %s\n", DefaultSessionGroupHandle().empty() ? "<none>" : string("'" + DefaultSessionGroupHandle() + "'").c_str());
    con_print("\r * \tDefault Session: %s\n", DefaultSessionHandle().empty() ? "<none>" : string("'" + DefaultSessionHandle() + "'").c_str());
}

void SDKSampleApp::sstats(const vector<string> &cmd)
{
    if (cmd.size() > 1) {
        PrintUsage(cmd.at(0), m_commands.find(cmd.at(0))->second.GetUsage());
        return;
    }

    vx_system_stats_t ss;
    ss.ss_size = sizeof(ss);
    if (vx_get_system_stats(&ss) != 0) {
        con_print("Error\n");
        return;
    }

    con_print("clnt_count=%d\n", ss.clnt_count);
    con_print("lc_count=%d\n", ss.lc_count);
    con_print("mps_count=%d\n", ss.mps_count);
    con_print("mpsg_count=%d\n", ss.mpsg_count);
    con_print("strms_count=%d\n", ss.strms_count);
    con_print("strr_count=%d\n", ss.strr_count);
    con_print("strss_count=%d\n", ss.strss_count);
    con_print("vp_count=%d\n", ss.vp_count);
    con_print("msgovrld_count=%d\n", ss.msgovrld_count);
}

void SDKSampleApp::capturedevice(const vector<string> &cmd)
{
    string scmd = "get";
    string deviceId;
    string accountHandle;
    int deviceIndex = 0;
    bool error = false;

    for (vector<string>::const_iterator i = cmd.begin() + 1; i < cmd.end(); ++i) {
        if (*i == "-get") {
            scmd = "get";
        } else if (*i == "-set") {
            scmd = "set";
            if (!nextArg(deviceId, cmd, i, error)) {
                break;
            }
        } else if (*i == "-setindex" || *i == "-i") {
            scmd = "setindex";
            if (!nextArg(deviceIndex, cmd, i, error)) {
                break;
            }
            if (deviceIndex <= 0) {
                con_print("Error: device index must be positive. Use 'capturedevice -get' command to list available capture devices and their indicies.\n");
                error = true;
            }
        } else if (*i == "-ah") {
            if (!nextArg(accountHandle, cmd, i, error)) {
                break;
            }
        } else {
            error = true;
            break;
        }
    }
    if (error || (scmd == "set" && deviceId.empty())) {
        PrintUsage(cmd.at(0), m_commands.find(cmd.at(0))->second.GetUsage());
        return;
    }
    if (scmd == "get") {
        aux_get_capture_devices(accountHandle);
    } else if (scmd == "set") {
        aux_set_capture_device(accountHandle, deviceId);
    } else if (scmd == "setindex") {
        SetCaptureDeviceIndexByAccountHandle(accountHandle, deviceIndex);
        aux_get_capture_devices(accountHandle);
    } else {
        PrintUsage(cmd.at(0), m_commands.find(cmd.at(0))->second.GetUsage());
    }
}

void SDKSampleApp::renderdevice(const vector<string> &cmd)
{
    string scmd = "get";
    string deviceId;
    string accountHandle;
    int deviceIndex = 0;
    bool error = false;

    for (vector<string>::const_iterator i = cmd.begin() + 1; i < cmd.end(); ++i) {
        if (*i == "-get") {
            scmd = "get";
        } else if (*i == "-set") {
            scmd = "set";
            if (!nextArg(deviceId, cmd, i, error)) {
                break;
            }
        } else if (*i == "-setindex" || *i == "-i") {
            scmd = "setindex";
            if (!nextArg(deviceIndex, cmd, i, error)) {
                break;
            }
            if (deviceIndex <= 0) {
                con_print("Error: device index must be positive. Use 'renderdevice -get' command to list available render devices and their indicies.\n");
                error = true;
            }
        } else if (*i == "-ah") {
            if (!nextArg(accountHandle, cmd, i, error)) {
                break;
            }
        } else {
            error = true;
            break;
        }
    }
    if (error || (scmd == "set" && deviceId.empty())) {
        PrintUsage(cmd.at(0), m_commands.find(cmd.at(0))->second.GetUsage());
        return;
    }
    if (scmd == "get") {
        aux_get_render_devices(accountHandle);
    } else if (scmd == "set") {
        aux_set_render_device(accountHandle, deviceId);
    } else if (scmd == "setindex") {
        SetRenderDeviceIndexByAccountHandle(accountHandle, deviceIndex);
        aux_get_render_devices(accountHandle);
    } else {
        PrintUsage(cmd.at(0), m_commands.find(cmd.at(0))->second.GetUsage());
    }
}

void SDKSampleApp::login(const vector<string> &cmd)
{
    string username;
    string accountHandle;
    string displayName;
    int frequency = -1;
    string langs;
    bool autoAcceptBuddies = true;
    bool error = false;

    if (!CheckHasConnectorHandle()) {
        return;
    }

    {
        // if the user name is not set, get a random user
        char *tmp = vx_get_random_user_id_ex("sa_", m_isMultitenant ? m_accessTokenIssuer.c_str() : NULL);
        username = tmp;
        vx_free(tmp);
    }
    for (vector<string>::const_iterator i = cmd.begin() + 1; i != cmd.end(); ++i) {
        if (*i == "-u") {
            if (!nextArg(username, cmd, i, error)) {
                break;
            }
        } else if (*i == "-ah") {
            if (!nextArg(accountHandle, cmd, i, error)) {
                break;
            }
        } else if (*i == "-d" || *i == "-displayname") {
            if (!nextArg(displayName, cmd, i, error)) {
                break;
            }
        } else if (*i == "-freq") {
            if (!nextArg(frequency, cmd, i, error)) {
                break;
            }
        } else if (*i == "-langs") {
            if (!nextArg(langs, cmd, i, error)) {
                break;
            }
        } else if (*i == "-noautoaccept") {
            autoAcceptBuddies = false;
        } else {
            error = true;
            break;
        }
    }
    if (error || username.empty()) {
        PrintUsage(cmd.at(0), m_commands.find(cmd.at(0))->second.GetUsage());
        return;
    }
    if (username.at(0) != '.' || username.at(username.size() - 1) != '.') {
        con_print("\r * username must start and end with a '.'\n");
        return;
    }
    con_print("\r * Logging %s in with connector handle %s and account handle %s and display name %s\n", username.c_str(), m_connectorHandle.c_str(), accountHandle.empty() ? "<AUTOGENERATE>" : accountHandle.c_str(), displayName.empty() ? "<EMPTY>" : displayName.c_str());
    account_anonymous_login(username, m_connectorHandle, accountHandle, displayName, frequency, langs, autoAcceptBuddies);
}

void SDKSampleApp::addsession(const vector<string> &cmd)
{
    string channel;
    bool joinmuted = false;
    bool audio = true;
    bool text = true;
    string sessionHandle;
    string sessionGroupHandle;
    bool error = false;

    string accountHandle = CheckAndGetDefaultAccountHandle();
    if (accountHandle.empty()) {
        return;
    }
    {
        // if the channel is not set, get a random echo channel
        char *tmp = vx_get_random_channel_uri_ex("confctl-e-", m_realm.c_str(), m_isMultitenant ? m_accessTokenIssuer.c_str() : NULL);
        channel = tmp;
        vx_free(tmp);
    }
    sessionGroupHandle = DefaultSessionGroupHandle();

    for (vector<string>::const_iterator i = cmd.begin() + 1; i != cmd.end(); ++i) {
        if (*i == "-c") {
            if (!nextArg(channel, cmd, i, error)) {
                break;
            }
        } else if (*i == "-gh") {
            if (!nextArg(sessionGroupHandle, cmd, i, error)) {
                break;
            }
        } else if (*i == "-sh") {
            if (!nextArg(sessionHandle, cmd, i, error)) {
                break;
            }
        } else if (*i == "-joinmuted") {
            joinmuted = true;
        } else if (*i == "-audio") {
            if (!nextArg(audio, cmd, i, error)) {
                break;
            }
        } else if (*i == "-text") {
            if (!nextArg(text, cmd, i, error)) {
                break;
            }
        } else if (*i == "-ah") {
            if (!nextArg(accountHandle, cmd, i, error)) {
                break;
            }
        } else {
            error = true;
            break;
        }
    }
    if (sessionHandle.empty()) {
        sessionHandle = channel;
    }
    if (sessionGroupHandle.empty()) {
        sessionGroupHandle = "sg_" + sessionHandle;
    }
    if (error || channel.empty() || sessionGroupHandle.empty() || sessionHandle.empty()) {
        PrintUsage(cmd.at(0), m_commands.find(cmd.at(0))->second.GetUsage());
        return;
    }
    con_print(
            "\r * Adding channel %s (muted=%s) (audio=%s) (text=%s) to session group %s with handle %s\n",
            channel.c_str(),
            joinmuted ? "yes" : "no",
            audio ? "yes" : "no",
            text ? "yes" : "no",
            sessionGroupHandle.c_str(),
            sessionHandle.c_str());

    // note: we will add this pair to m_SSGHandles when handling a successful resp_sessiongroup_add_session
    sessiongroup_add_session(accountHandle, sessionGroupHandle, sessionHandle, channel, joinmuted, audio, text);
}

void SDKSampleApp::removesession(const vector<string> &cmd)
{
    string sessionHandle;
    string sessionGroupHandle;
    bool error = false;

    if (!CheckHasSessionHandle()) {
        return;
    }
    for (vector<string>::const_iterator i = cmd.begin() + 1; i != cmd.end(); ++i) {
        if (*i == "-sh") {
            if (!nextArg(sessionHandle, cmd, i, error)) {
                break;
            }
        } else {
            error = true;
            break;
        }
    }
    if (sessionHandle.empty()) {
        sessionGroupHandle = DefaultSessionGroupHandle();
        sessionHandle = DefaultSessionHandle();
    } else {
        sessionGroupHandle = FindSessionGroupHandleBySessionHandle(sessionHandle);
        if (sessionGroupHandle.empty()) {
            con_print("error: The object referred to by the parameter 'session_handle' does not exist.\n");
            return;
        }
    }
    if (error || sessionHandle.empty()) {
        PrintUsage(cmd.at(0), m_commands.find(cmd.at(0))->second.GetUsage());
        return;
    }
    con_print("\r * Removing session %s from session group %s\n", sessionHandle.c_str(), sessionGroupHandle.c_str());
    // note: we will remove this pair from m_SSGHandles when handling a successful resp_sessiongroup_remove_session
    sessiongroup_remove_session(sessionGroupHandle, sessionHandle);
}

static string FormatCoordinate(double x)
{
    if (x < 0.001 && x > -0.001 && x != 0) {
        return "approximately 0";
    } else {
        stringstream ss;
        ss << x;
        return ss.str();
    }
}

void SDKSampleApp::move(const vector<string> &cmd)
{
    string scmd;
    double delta = 0;
    string sessionHandle;
    bool error = false;

    if (!CheckHasSessionHandle()) {
        return;
    }
    for (vector<string>::const_iterator i = cmd.begin() + 1; i != cmd.end(); ++i) {
        if (*i == "-origin") {
            scmd = *i;
        } else if (*i == "-forward" || *i == "-back" || *i == "-left" || *i == "-right" || *i == "-up" || *i == "-down" || *i == "-turnleft" || *i == "-turnright") {
            scmd = *i;
            if (!nextArg(delta, cmd, i, error)) {
                break;
            }
        } else if (*i == "-sh") {
            if (!nextArg(sessionHandle, cmd, i, error)) {
                break;
            }
        } else {
            error = true;
            break;
        }
    }
    if (sessionHandle.empty()) {
        sessionHandle = DefaultSessionHandle();
    }
    if (error || sessionHandle.empty() || (!scmd.empty() && scmd != "-origin" && delta == 0)) {
        PrintUsage(cmd.at(0), m_commands.find(cmd.at(0))->second.GetUsage());
        return;
    }

    if (scmd == "-origin") {
        con_print("\r * Moving Listener Position to the origin\n");
        req_move_origin(sessionHandle);
    } else if (scmd == "-forward") {
        con_print("\r * Moving Listener Position Forward by %f\n", delta);
        req_move_forward(sessionHandle, delta);
    } else if (scmd == "-back") {
        con_print("\r * Moving Listener Position Back by %f\n", delta);
        req_move_back(sessionHandle, delta);
    } else if (scmd == "-left") {
        con_print("\r * Moving Listener Position Left by %f\n", delta);
        req_move_left(sessionHandle, delta);
    } else if (scmd == "-right") {
        con_print("\r * Moving Listener Position Right by %f\n", delta);
        req_move_right(sessionHandle, delta);
    } else if (scmd == "-up") {
        con_print("\r * Moving Listener Position Up by %f\n", delta);
        req_move_up(sessionHandle, delta);
    } else if (scmd == "-down") {
        con_print("\r * Moving Listener Position Down by %f\n", delta);
        req_move_down(sessionHandle, delta);
    } else if (scmd == "-turnleft") {
        con_print("\r * Turning Listener Left (Anti-Clockwise) by %f\n", delta);
        req_turn_left(sessionHandle, delta);
    } else if (scmd == "-turnright") {
        con_print("\r * Turning Listener Right (Clockwise) by %f\n", delta);
        req_turn_right(sessionHandle, delta);
    } else {
        SampleAppPosition position;
        double headingDegrees;
        GetListenerPosition(sessionHandle, position);
        GetListenerHeadingDegrees(sessionHandle, headingDegrees);
        con_print("\r * Position: {%s, %s, %s}\n", FormatCoordinate(position.x).c_str(), FormatCoordinate(position.y).c_str(), FormatCoordinate(position.z).c_str());
        con_print("\r * Heading Degrees: %s\n", FormatCoordinate(headingDegrees).c_str());
    }
}

void SDKSampleApp::dance(const vector<string> &cmd)
{
    string scmd;
    string sessionHandle;
    bool error = false;
    bool stop = false;
    double x0 = 0;
    double y0 = 0;
    double z0 = 0;
    double rmin = 1;
    double rmax = 10;
    double angularVelocityDegPerSec = 10; // 36 seconds for a full circle
    double oscillationPeriodSeconds = 9; // rmin -> rmax -> rmin in 9 seconds
    int updateMilliseconds = 100;


    if (!CheckHasSessionHandle()) {
        return;
    }
    for (vector<string>::const_iterator i = cmd.begin() + 1; i != cmd.end(); ++i) {
        if (*i == "-sh") {
            if (!nextArg(sessionHandle, cmd, i, error)) {
                break;
            }
        } else if (*i == "-stop") {
            stop = true;
        } else if (*i == "-c") {
            if (
                !nextArg(x0, cmd, i, error) ||
                !nextArg(y0, cmd, i, error) ||
                !nextArg(z0, cmd, i, error))
            {
                break;
            }
        } else if (*i == "-r") {
            if (!nextArg(rmin, cmd, i, error)) {
                break;
            }
            rmax = rmin;
        } else if (*i == "-rmin") {
            if (!nextArg(rmin, cmd, i, error)) {
                break;
            }
        } else if (*i == "-rmax") {
            if (!nextArg(rmax, cmd, i, error)) {
                break;
            }
        } else if (*i == "-a") {
            if (!nextArg(angularVelocityDegPerSec, cmd, i, error)) {
                break;
            }
        } else if (*i == "-o") {
            if (!nextArg(oscillationPeriodSeconds, cmd, i, error)) {
                break;
            }
        } else if (*i == "-ms") {
            if (!nextArg(updateMilliseconds, cmd, i, error)) {
                break;
            }
            if (updateMilliseconds <= 0) {
                con_print("error: %s: -ms argument must be a positive integer\n", cmd[0].c_str());
                error = true;
                break;
            }
        } else {
            error = true;
            break;
        }
    }

    if (sessionHandle.empty()) {
        sessionHandle = DefaultSessionHandle();
    }
    if (error || sessionHandle.empty()) {
        PrintUsage(cmd.at(0), m_commands.find(cmd.at(0))->second.GetUsage());
        return;
    }

    // Find the referenced session
    auto session_it = m_sessions.find(sessionHandle);
    if (session_it == m_sessions.end()) {
        con_print("error: session with handle '%s' not found\n", sessionHandle.c_str());
        return;
    }

    // Start dancing
    if (stop) {
        session_it->second->DanceStop();
    } else {
        session_it->second->DanceStart(sessionHandle, x0, y0, z0, rmin, rmax, angularVelocityDegPerSec, oscillationPeriodSeconds, updateMilliseconds);
    }
}

void SDKSampleApp::focus(const vector<string> &cmd)
{
    string scmd;
    string sessionHandle;
    string sessionGroupHandle;
    bool error = false;

    if (!CheckHasSessionOrSessionGroupHandle()) {
        return;
    }
    for (vector<string>::const_iterator i = cmd.begin() + 1; i != cmd.end(); ++i) {
        if (*i == "-set" || *i == "-reset") {
            scmd = *i;
        } else if (*i == "-gh") {
            if (!nextArg(sessionGroupHandle, cmd, i, error)) {
                break;
            }
        } else if (*i == "-sh") {
            if (!nextArg(sessionHandle, cmd, i, error)) {
                break;
            }
        } else {
            error = true;
            break;
        }
    }
    if (sessionGroupHandle.empty()) {
        sessionGroupHandle = DefaultSessionGroupHandle();
    }
    if (sessionHandle.empty()) {
        sessionHandle = DefaultSessionHandle();
    }
    if (error || scmd.empty() || (scmd == "-reset" && sessionGroupHandle.empty()) || (scmd != "-reset" && sessionHandle.empty())) {
        PrintUsage(cmd.at(0), m_commands.find(cmd.at(0))->second.GetUsage());
        return;
    }
    if (scmd == "-set") {
        con_print("\r * Setting focus for session handle %s\n", sessionHandle.c_str());
        sessiongroup_set_focus(sessionHandle);
    } else if (scmd == "-reset") {
        con_print("\r * Resetting focus for session group handle %s\n", sessionHandle.c_str());
        sessiongroup_reset_focus(sessionGroupHandle);
    }
}

void SDKSampleApp::inject(const vector<string> &cmd)
{
    string scmd;
    string file;
    string sessionGroupHandle;
    bool error = false;

    if (!CheckHasSessionGroupHandle()) {
        return;
    }
    for (vector<string>::const_iterator i = cmd.begin() + 1; i != cmd.end(); ++i) {
        if (*i == "-start") {
            if (!nextArg(file, cmd, i, error)) {
                break;
            }
            scmd = "start";
        } else if (*i == "-gh") {
            if (!nextArg(sessionGroupHandle, cmd, i, error)) {
                break;
            }
        } else if (*i == "-stop") {
            scmd = "stop";
        } else {
            error = true;
            break;
        }
    }
    if (sessionGroupHandle.empty()) {
        sessionGroupHandle = DefaultSessionGroupHandle();
    }
    if (error || scmd.empty() || sessionGroupHandle.empty() || (scmd == "start" && file.empty())) {
        PrintUsage(cmd.at(0), m_commands.find(cmd.at(0))->second.GetUsage());
        return;
    }
    if (scmd == "start") {
        con_print("\r * Start audio file injection %s into session group %s\n", file.c_str(), sessionGroupHandle.c_str());
        sessiongroup_inject_audio_start(sessionGroupHandle, file);
    } else {
        con_print("\r * Stop audio file injection into session group %s\n", sessionGroupHandle.c_str());
        sessiongroup_inject_audio_stop(sessionGroupHandle);
    }
}

void SDKSampleApp::transmit(const vector<string> &cmd)
{
    string scmd;
    string sessionHandle;
    string sessionGroupHandle;
    bool error = false;

    if (!CheckHasSessionOrSessionGroupHandle()) {
        return;
    }
    for (vector<string>::const_iterator i = cmd.begin() + 1; i != cmd.end(); ++i) {
        if (*i == "-none") {
            scmd = "none";
        } else if (*i == "-all") {
            scmd = "all";
        } else if (*i == "-sh") {
            if (!nextArg(sessionHandle, cmd, i, error)) {
                break;
            }
        } else if (*i == "-gh") {
            if (!nextArg(sessionGroupHandle, cmd, i, error)) {
                break;
            }
        } else {
            error = true;
            break;
        }
    }
    if (sessionGroupHandle.empty()) {
        sessionGroupHandle = DefaultSessionGroupHandle();
    }
    if (sessionHandle.empty()) {
        sessionHandle = DefaultSessionHandle();
    }
    if (error || (!scmd.empty() && sessionGroupHandle.empty()) || (scmd.empty() && sessionHandle.empty())) {
        PrintUsage(cmd.at(0), m_commands.find(cmd.at(0))->second.GetUsage());
        return;
    }
    if (scmd == "all") {
        con_print("\r * Setting transmission to all sessions in session group %s\n", sessionGroupHandle.c_str());
        sessiongroup_set_tx_all_sessions(sessionGroupHandle);
    } else if (scmd == "none") {
        con_print("\r * Setting transmission to no sessions in session group %s\n", sessionGroupHandle.c_str());
        sessiongroup_set_tx_no_session(sessionGroupHandle);
    } else {
        con_print("\r * Setting transmission to only session %s in session group %s\n", sessionHandle.c_str(), FindSessionGroupHandleBySessionHandle(sessionHandle).c_str());
        sessiongroup_set_tx_session(sessionHandle);
    }
}

void SDKSampleApp::message(const vector<string> &cmd)
{
    string message;
    string sessionHandle;
    string user;
    int numberOfMsgs = 1;
    string customMetadataNS;
    string customMetadata;
    bool error = false;

    string accountHandle = CheckAndGetDefaultAccountHandle();
    if (accountHandle.empty()) {
        return;
    }

    for (vector<string>::const_iterator i = cmd.begin() + 1; i != cmd.end(); ++i) {
        if (*i == "-m") {
            if (!nextArg(message, cmd, i, error)) {
                break;
            }
        } else if (*i == "-sh") {
            if (!nextArg(sessionHandle, cmd, i, error)) {
                break;
            }
        } else if (*i == "-u") {
            if (!nextArg(user, cmd, i, error)) {
                break;
            }
        } else if (*i == "-n") {
            if (!nextArg(numberOfMsgs, cmd, i, error)) {
                break;
            }
        } else if (*i == "-appns") {
            if (!nextArg(customMetadataNS, cmd, i, error)) {
                break;
            }
        } else if (*i == "-appdata") {
            if (!nextArg(customMetadata, cmd, i, error)) {
                break;
            }
        } else if (*i == "-ah") {
            if (!nextArg(accountHandle, cmd, i, error)) {
                break;
            }
        } else {
            error = true;
            break;
        }
    }

    if (error || (message.empty() && customMetadata.empty())) {
        PrintUsage(cmd.at(0), m_commands.find(cmd.at(0))->second.GetUsage());
        return;
    }

    bool sendSessionMsg = user.empty();

    if (sendSessionMsg) {
        if (!CheckHasSessionHandle()) {
            return;
        }
        if (sessionHandle.empty()) {
            sessionHandle = DefaultSessionHandle();
        }
        if (sessionHandle.empty()) {
            PrintUsage(cmd.at(0), m_commands.find(cmd.at(0))->second.GetUsage());
            return;
        }
    }

    for (int i = 1; i <= numberOfMsgs; i++) {
        string msgText;
        if (numberOfMsgs > 1) {
            stringstream ss;
            ss << message;
            ss << i;
            msgText = ss.str();
        } else {
            msgText = message;
        }
        if (sendSessionMsg) {
            con_print("\r * Sending message '%s' to session handle %s\n", msgText.c_str(), sessionHandle.c_str());
            session_send_message(sessionHandle, msgText, customMetadataNS, customMetadata);
        } else {
            con_print("\r * Sending directed message '%s' to user %s\n", msgText.c_str(), user.c_str());
            account_send_message(accountHandle, msgText.c_str(), user.c_str(), customMetadataNS, customMetadata);
        }
    }
}

void SDKSampleApp::media(const vector<string> &cmd)
{
    string scmd;
    string sessionHandle;
    bool error = false;

    if (!CheckHasSessionHandle()) {
        return;
    }
    for (vector<string>::const_iterator i = cmd.begin() + 1; i != cmd.end(); ++i) {
        if (*i == "-connect" || *i == "-disconnect") {
            scmd = *i;
        } else if (*i == "-sh") {
            if (!nextArg(sessionHandle, cmd, i, error)) {
                break;
            }
        } else {
            error = true;
            break;
        }
    }
    if (sessionHandle.empty()) {
        sessionHandle = DefaultSessionHandle();
    }
    if (error || scmd.empty() || sessionHandle.empty()) {
        PrintUsage(cmd.at(0), m_commands.find(cmd.at(0))->second.GetUsage());
        return;
    }
    if (scmd == "-connect") {
        con_print("\r * Connecting Media on session handle %s\n", sessionHandle.c_str());
        session_media_connect(sessionHandle);
    } else {
        con_print("\r * Disconnecting Media on session handle %s\n", sessionHandle.c_str());
        session_media_disconnect(sessionHandle);
    }
}

void SDKSampleApp::text(const vector<string> &cmd)
{
    string scmd;
    string sessionHandle;
    bool error = false;

    if (!CheckHasSessionHandle()) {
        return;
    }
    for (vector<string>::const_iterator i = cmd.begin() + 1; i != cmd.end(); ++i) {
        if (*i == "-connect" || *i == "-disconnect") {
            scmd = *i;
        } else if (*i == "-sh") {
            if (!nextArg(sessionHandle, cmd, i, error)) {
                break;
            }
        } else {
            error = true;
            break;
        }
    }
    if (sessionHandle.empty()) {
        sessionHandle = DefaultSessionHandle();
    }
    if (error || scmd.empty() || sessionHandle.empty()) {
        PrintUsage(cmd.at(0), m_commands.find(cmd.at(0))->second.GetUsage());
        return;
    }
    if (scmd == "-connect") {
        con_print("\r * Connecting Text on session handle %s\n", sessionHandle.c_str());
        session_text_connect(sessionHandle);
    } else {
        con_print("\r * Disconnecting Text on session handle %s\n", sessionHandle.c_str());
        session_text_disconnect(sessionHandle);
    }
}

void SDKSampleApp::muteforme(const vector<string> &cmd)
{
    string user;
    string sessionHandle;

    bool mute = true;
    bool error = false;

    string scope = "all";
    vx_mute_scope s = mute_scope_all;

    checkScopeArg(scope, &s);

    if (!CheckHasSessionHandle()) {
        return;
    }
    for (vector<string>::const_iterator i = cmd.begin() + 1; i != cmd.end(); ++i) {
        if (*i == "-mute" || *i == "-unmute") {
            mute = *i == "-mute";
        } else if (*i == "-sh") {
            if (!nextArg(sessionHandle, cmd, i, error)) {
                break;
            }
        } else if (*i == "-u") {
            if (!nextArg(user, cmd, i, error)) {
                break;
            }
        } else if (*i == "-scope") {
            if (!nextArg(scope, cmd, i, error)) {
                break;
            }
            if (!checkScopeArg(scope, &s)) {
                error = true;
                break;
            }
        } else {
            error = true;
            break;
        }
    }
    if (sessionHandle.empty()) {
        sessionHandle = DefaultSessionHandle();
    }
    if (error || sessionHandle.empty() || user.empty()) {
        PrintUsage(cmd.at(0), m_commands.find(cmd.at(0))->second.GetUsage());
        return;
    }

    con_print("\r * Setting Mute for User %s in session handle %s to %s, scope '%s'\n", user.c_str(), sessionHandle.c_str(), mute ? "muted" : "unmuted", scope.c_str());
    session_set_participant_mute_for_me(sessionHandle, user, mute, s);
}

void SDKSampleApp::blockusers(const vector<string> &cmd)
{
    list<string> users;
    vx_control_communications_operation operation = vx_control_communications_operation_block;
    bool error = false;
    bool users_can_be_empty = false;

    string accountHandle = CheckAndGetDefaultAccountHandle();
    if (accountHandle.empty()) {
        return;
    }

    for (vector<string>::const_iterator i = cmd.begin() + 1; i != cmd.end(); ++i) {
        if (*i == "-block") {
            operation = vx_control_communications_operation_block;
            users_can_be_empty = false;
        } else if (*i == "-unblock") {
            operation = vx_control_communications_operation_unblock;
            users_can_be_empty = false;
        } else if (*i == "-mute") {
            operation = vx_control_communications_operation_mute;
            users_can_be_empty = false;
        } else if (*i == "-unmute") {
            operation = vx_control_communications_operation_unmute;
            users_can_be_empty = false;
        } else if (*i == "-listblocked") {
            operation = vx_control_communications_operation_block_list;
            users_can_be_empty = true;
        } else if (*i == "-listmuted") {
            operation = vx_control_communications_operation_mute_list;
            users_can_be_empty = true;
        } else if (*i == "-clearblocked") {
            operation = vx_control_communications_operation_clear_block_list;
            users_can_be_empty = true;
        } else if (*i == "-clearmuted") {
            operation = vx_control_communications_operation_clear_mute_list;
            users_can_be_empty = true;
        } else if (*i == "-ah") {
            if (!nextArg(accountHandle, cmd, i, error)) {
                break;
            }
        } else if (*i == "-u") {
            string user;
            if (!nextArg(user, cmd, i, error)) {
                break;
            }
            users.push_back(user);
        } else {
            error = true;
            break;
        }
    }
    if (error || accountHandle.empty() || (users.empty() && !users_can_be_empty)) {
        PrintUsage(cmd.at(0), m_commands.find(cmd.at(0))->second.GetUsage());
        return;
    }

    switch (operation) {
        case vx_control_communications_operation_block:
            con_print("\r * Blocking %s\n", (users.size() == 1) ? users.front().c_str() : "several users");
            break;
        case vx_control_communications_operation_unblock:
            con_print("\r * Unblocking %s\n", (users.size() == 1) ? users.front().c_str() : "several users");
            break;
        case vx_control_communications_operation_mute:
            con_print("\r * Muting %s\n", (users.size() == 1) ? users.front().c_str() : "several users");
            break;
        case vx_control_communications_operation_unmute:
            con_print("\r * Unmuting %s\n", (users.size() == 1) ? users.front().c_str() : "several users");
            break;
        case vx_control_communications_operation_block_list:
            con_print("\r * Getting list of blocked users\n");
            break;
        case vx_control_communications_operation_mute_list:
            con_print("\r * Getting list of muted users\n");
            break;
        case vx_control_communications_operation_clear_block_list:
            con_print("\r * Clearing list of blocked users (unblock all)\n");
            break;
        case vx_control_communications_operation_clear_mute_list:
            con_print("\r * Clearing list of muted users (unmute all, except blocked)\n");
            break;
        default:
            con_print("\r * Error: wrong operation\n");
            PrintUsage(cmd.at(0), m_commands.find(cmd.at(0))->second.GetUsage());
            return;
    }
    account_blockusers(accountHandle, users, operation);
}

void SDKSampleApp::muteuser(const vector<string> &cmd)
{
    string user;
    string channel;

    bool mute = true;
    bool error = false;

    string scope = "all";
    vx_mute_scope s = mute_scope_all;

    checkScopeArg(scope, &s);

    string accountHandle = CheckAndGetDefaultAccountHandle();
    if (accountHandle.empty()) {
        return;
    }
    for (vector<string>::const_iterator i = cmd.begin() + 1; i != cmd.end(); ++i) {
        if (*i == "-mute" || *i == "-unmute") {
            mute = *i == "-mute";
        } else if (*i == "-c") {
            if (!nextArg(channel, cmd, i, error)) {
                break;
            }
        } else if (*i == "-u") {
            if (!nextArg(user, cmd, i, error)) {
                break;
            }
        } else if (*i == "-ah") {
            if (!nextArg(accountHandle, cmd, i, error)) {
                break;
            }
        } else if (*i == "-scope") {
            if (!nextArg(scope, cmd, i, error)) {
                break;
            }
            if (!checkScopeArg(scope, &s)) {
                error = true;
                break;
            }
        } else {
            error = true;
            break;
        }
    }
    if (error || user.empty() || channel.empty()) {
        PrintUsage(cmd.at(0), m_commands.find(cmd.at(0))->second.GetUsage());
        return;
    }

    con_print("\r * Setting Mute for User %s in channel %s to %s on account %s, scope %s\n", user.c_str(), channel.c_str(), mute ? "muted" : "unmuted", accountHandle.c_str(), scope.c_str());
    channel_mute_user(accountHandle, channel, user, mute, s);
}

void SDKSampleApp::muteall(const vector<string> &cmd)
{
    string channel;
    string sessionHandle;

    bool mute = true;
    bool error = false;

    string scope = "all";
    vx_mute_scope s = mute_scope_all;

    checkScopeArg(scope, &s);

    string accountHandle = CheckAndGetDefaultAccountHandle();
    if (accountHandle.empty()) {
        return;
    }
    for (vector<string>::const_iterator i = cmd.begin() + 1; i != cmd.end(); ++i) {
        if (*i == "-mute" || *i == "-unmute") {
            mute = *i == "-mute";
        } else if (*i == "-c") {
            if (!nextArg(channel, cmd, i, error)) {
                break;
            }
        } else if (*i == "-sh") {
            if (!nextArg(sessionHandle, cmd, i, error)) {
                break;
            }
        } else if (*i == "-ah") {
            if (!nextArg(accountHandle, cmd, i, error)) {
                break;
            }
        } else if (*i == "-scope") {
            if (!nextArg(scope, cmd, i, error)) {
                break;
            }
            if (!checkScopeArg(scope, &s)) {
                error = true;
                break;
            }
        } else {
            error = true;
            break;
        }
    }

    if (channel.empty()) {
        if (sessionHandle.empty()) {
            sessionHandle = DefaultSessionHandle();
        }
        auto i = m_sessions.find(sessionHandle);
        if (i != m_sessions.end()) {
            channel = i->second->GetUri();
        }
    }

    if (error || channel.empty()) {
        PrintUsage(cmd.at(0), m_commands.find(cmd.at(0))->second.GetUsage());
        return;
    }

    con_print("\r * Setting Mute for all other users in channel %s to %s on account %s, scope %s\n",  channel.c_str(), mute ? "muted" : "unmuted", accountHandle.c_str(), scope.c_str());
    channel_mute_all_users(accountHandle, channel, mute, s);
}

void SDKSampleApp::mutelocal(const vector<string> &cmd)
{
    bool speaker = false;
    bool mute = true;
    string accountHandle;
    bool error = false;

    for (vector<string>::const_iterator i = cmd.begin() + 1; i != cmd.end(); ++i) {
        if (*i == "-mic" || *i == "-speaker") {
            speaker = *i == "-speaker";
        } else if (*i == "-mute" || *i == "-unmute") {
            mute = *i == "-mute";
        } else if (*i == "-ah") {
            if (!nextArg(accountHandle, cmd, i, error)) {
                break;
            }
        } else {
            error = true;
            break;
        }
    }
    if (error) {
        PrintUsage(cmd.at(0), m_commands.find(cmd.at(0))->second.GetUsage());
        return;
    }
    con_print("\r * Setting Master %s Muted to %s\n", speaker ? "Speaker" : "Mic", mute ? "muted" : "unmuted");
    if (speaker) {
        connector_mute_local_speaker(accountHandle, mute);
    } else {
        connector_mute_local_mic(accountHandle, mute);
    }
}

void SDKSampleApp::audioinfo(const vector<string> &cmd)
{
    string accountHandle;
    bool error = false;
    if (cmd.size() > 1) {
        PrintUsage(cmd.at(0), m_commands.find(cmd.at(0))->second.GetUsage());
    }
    for (vector<string>::const_iterator i = cmd.begin() + 1; i != cmd.end(); ++i) {
        if (*i == "-ah") {
            if (!nextArg(accountHandle, cmd, i, error)) {
                break;
            }
        } else {
            error = true;
            break;
        }
    }
    connector_get_local_audio_info(accountHandle, m_connectorHandle);
}

void SDKSampleApp::volumelocal(const vector<string> &cmd)
{
    bool get = true;
    bool speaker = true;
    int volume = 50;
    string accountHandle;
    bool error = false;

    for (vector<string>::const_iterator i = cmd.begin() + 1; i != cmd.end(); ++i) {
        if (*i == "-set" || *i == "-get") {
            get = *i == "-get";
        } else if (*i == "-mic" || *i == "-speaker") {
            speaker = *i == "-speaker";
        } else if (*i == "-v") {
            if (!nextArg(volume, cmd, i, error)) {
                break;
            }
        } else if (*i == "-ah") {
            if (!nextArg(accountHandle, cmd, i, error)) {
                break;
            }
        } else {
            error = true;
            break;
        }
    }
    if (error) {
        PrintUsage(cmd.at(0), m_commands.find(cmd.at(0))->second.GetUsage());
        return;
    }
    if (get) {
        con_print("\r * Getting Master %s Volume\n", speaker ? "Speaker" : "Mic");
        if (speaker) {
            aux_get_speaker_level(accountHandle);
        } else {
            aux_get_mic_level(accountHandle);
        }
    } else {
        con_print("\r * Setting Master %s Volume to %d\n", speaker ? "Speaker" : "Mic", volume);
        if (speaker) {
            aux_set_speaker_level(accountHandle, volume);
        } else {
            aux_set_mic_level(accountHandle, volume);
        }
    }
}

void SDKSampleApp::volumeforme(const vector<string> &cmd)
{
    string user;
    int volume = 50;
    string sessionHandle;
    bool error = false;

    if (!CheckHasSessionHandle()) {
        return;
    }
    for (vector<string>::const_iterator i = cmd.begin() + 1; i != cmd.end(); ++i) {
        if (*i == "-v") {
            if (!nextArg(volume, cmd, i, error)) {
                break;
            }
        } else if (*i == "-sh") {
            if (!nextArg(sessionHandle, cmd, i, error)) {
                break;
            }
        } else if (*i == "-u") {
            if (!nextArg(user, cmd, i, error)) {
                break;
            }
        } else {
            error = true;
            break;
        }
    }
    if (sessionHandle.empty()) {
        sessionHandle = DefaultSessionHandle();
    }
    if (error || sessionHandle.empty() || user.empty()) {
        PrintUsage(cmd.at(0), m_commands.find(cmd.at(0))->second.GetUsage());
        return;
    }
    con_print("\r * Setting Volume for User %s in session handle %s to %d\n", user.c_str(), sessionHandle.c_str(), volume);
    session_set_participant_volume_for_me(sessionHandle, user, volume);
}

void SDKSampleApp::sessionvolumeforme(const vector<string> &cmd)
{
    string user;
    int volume = 50;
    string sessionHandle;
    bool error = false;

    if (!CheckHasSessionHandle()) {
        return;
    }
    for (vector<string>::const_iterator i = cmd.begin() + 1; i != cmd.end(); ++i) {
        if (*i == "-v") {
            if (!nextArg(volume, cmd, i, error)) {
                break;
            }
        } else if (*i == "-sh") {
            if (!nextArg(sessionHandle, cmd, i, error)) {
                break;
            }
        } else {
            error = true;
            break;
        }
    }
    if (sessionHandle.empty()) {
        sessionHandle = DefaultSessionHandle();
    }
    if (error || sessionHandle.empty()) {
        PrintUsage(cmd.at(0), m_commands.find(cmd.at(0))->second.GetUsage());
        return;
    }
    con_print("\r * Setting Volume for all Users in session handle %s to %d\n", sessionHandle.c_str(), volume);
    session_set_local_render_volume(sessionHandle, volume);
}

void SDKSampleApp::kick(const vector<string> &cmd)
{
    string user;
    string channel;
    bool error = false;

    string accountHandle = CheckAndGetDefaultAccountHandle();
    if (accountHandle.empty()) {
        return;
    }
    for (vector<string>::const_iterator i = cmd.begin() + 1; i != cmd.end(); ++i) {
        if (*i == "-c") {
            if (!nextArg(channel, cmd, i, error)) {
                break;
            }
        } else if (*i == "-u") {
            if (!nextArg(user, cmd, i, error)) {
                break;
            }
        } else if (*i == "-ah") {
            if (!nextArg(accountHandle, cmd, i, error)) {
                break;
            }
        } else {
            error = true;
            break;
        }
    }
    if (error || channel.empty() || user.empty()) {
        PrintUsage(cmd.at(0), m_commands.find(cmd.at(0))->second.GetUsage());
        return;
    }
    con_print("\r * Kicking user %s in channel %s on account handle %s\n", user.c_str(), channel.c_str(), accountHandle.c_str());
    channel_kick_user(accountHandle, channel, user);
}

void SDKSampleApp::getstats(const vector<string> &cmd)
{
    string sessionGroupHandle;
    bool error = false;

    if (!CheckHasSessionGroupHandle()) {
        return;
    }
    for (vector<string>::const_iterator i = cmd.begin() + 1; i != cmd.end(); ++i) {
        if (*i == "-gh") {
            if (!nextArg(sessionGroupHandle, cmd, i, error)) {
                break;
            }
        } else {
            error = true;
            break;
        }
    }
    if (sessionGroupHandle.empty()) {
        sessionGroupHandle = DefaultSessionGroupHandle();
    }
    if (error || sessionGroupHandle.empty()) {
        PrintUsage(cmd.at(0), m_commands.find(cmd.at(0))->second.GetUsage());
        return;
    }
    con_print("\r * Getting call statistics for session group %s\n", sessionGroupHandle.c_str());
    sessiongroup_get_stats(sessionGroupHandle);
}

void SDKSampleApp::blockrule(const vector<string> &cmd)
{
    string scmd = "list";
    string uri;
    bool error = false;

    string accountHandle = CheckAndGetDefaultAccountHandle();
    if (accountHandle.empty()) {
        return;
    }
    for (vector<string>::const_iterator i = cmd.begin() + 1; i != cmd.end(); ++i) {
        if (*i == "-create") {
            if (!nextArg(uri, cmd, i, error)) {
                break;
            }
            scmd = "create";
        } else if (*i == "-delete") {
            if (!nextArg(uri, cmd, i, error)) {
                break;
            }
            scmd = "delete";
        } else if (*i == "-list") {
            scmd = "list";
        } else if (*i == "-ah") {
            if (!nextArg(accountHandle, cmd, i, error)) {
                break;
            }
        } else {
            error = true;
            break;
        }
    }
    if (error || (uri.empty() && scmd != "list")) {
        PrintUsage(cmd.at(0), m_commands.find(cmd.at(0))->second.GetUsage());
        return;
    }
    if (scmd == "create" || scmd == "delete") {
        con_print("\r * %s user %s on account handle %s\n", scmd == "create" ? "Blocking" : "Unblocking", uri.c_str(), accountHandle.c_str());
        if (scmd == "create") {
            account_create_block_rule(accountHandle, uri);
        } else {
            account_delete_block_rule(accountHandle, uri);
        }
    } else {
        con_print("\r * Listing block rules for account handle %s\n", accountHandle.c_str());
        account_list_block_rules(accountHandle);
    }
}

void SDKSampleApp::acceptrule(const vector<string> &cmd)
{
    string scmd = "list";
    string uri;
    bool error = false;

    string accountHandle = CheckAndGetDefaultAccountHandle();
    if (accountHandle.empty()) {
        return;
    }
    for (vector<string>::const_iterator i = cmd.begin() + 1; i != cmd.end(); ++i) {
        if (*i == "-create") {
            if (!nextArg(uri, cmd, i, error)) {
                break;
            }
            scmd = "create";
        } else if (*i == "-delete") {
            if (!nextArg(uri, cmd, i, error)) {
                break;
            }
            scmd = "delete";
        } else if (*i == "-list") {
            scmd = "list";
        } else if (*i == "-ah") {
            if (!nextArg(accountHandle, cmd, i, error)) {
                break;
            }
        } else {
            error = true;
            break;
        }
    }
    if (error || (uri.empty() && scmd != "list")) {
        PrintUsage(cmd.at(0), m_commands.find(cmd.at(0))->second.GetUsage());
        return;
    }
    if (scmd == "create" || scmd == "delete") {
        con_print("\r * %s user %s on account handle %s\n", scmd == "create" ? "Accepting" : "No longer Accepting", uri.c_str(), accountHandle.c_str());
        if (scmd == "create") {
            account_create_auto_accept_rule(accountHandle, uri);
        } else {
            account_delete_auto_accept_rule(accountHandle, uri);
        }
    } else {
        con_print("\r * Listing accept rules for account handle %s\n", accountHandle.c_str());
        account_list_auto_accept_rules(accountHandle);
    }
}

void SDKSampleApp::captureaudio(const vector<string> &cmd)
{
    string scmd;
    string accountHandle;
    bool error = false;

    for (vector<string>::const_iterator i = cmd.begin() + 1; i != cmd.end(); ++i) {
        if (*i == "-start") {
            scmd = "start";
        } else if (*i == "-stop") {
            scmd = "stop";
        } else if (*i == "-ah") {
            if (!nextArg(accountHandle, cmd, i, error)) {
                break;
            }
        } else {
            error = true;
            break;
        }
    }
    if (error || scmd.empty()) {
        PrintUsage(cmd.at(0), m_commands.find(cmd.at(0))->second.GetUsage());
        return;
    }
    if (scmd == "start") {
        con_print("\r * Start audio capture\n");
        aux_capture_audio_start(accountHandle);
    } else {
        con_print("\r * Stop audio capture\n");
        aux_capture_audio_stop(accountHandle);
    }
}

void SDKSampleApp::renderaudio(const vector<string> &cmd)
{
    string scmd;
    string filename;
    bool loop = false;
    string accountHandle;
    bool error = false;

    for (vector<string>::const_iterator i = cmd.begin() + 1; i != cmd.end(); ++i) {
        if (*i == "-start") {
            if (!nextArg(filename, cmd, i, error)) {
                break;
            }
            scmd = "start";
        } else if (*i == "-stop") {
            scmd = "stop";
        } else if (*i == "-loop") {
            if (!nextArg(loop, cmd, i, error)) {
                break;
            }
        } else if (*i == "-ah") {
            if (!nextArg(accountHandle, cmd, i, error)) {
                break;
            }
        } else {
            error = true;
            break;
        }
    }
    if (error || scmd.empty()) {
        PrintUsage(cmd.at(0), m_commands.find(cmd.at(0))->second.GetUsage());
        return;
    }
    if (scmd == "start") {
        con_print("\r * Start audio render\n");
        aux_render_audio_start(accountHandle, filename, loop);
    } else {
        con_print("\r * Stop audio render\n");
        aux_render_audio_stop(accountHandle);
    }
}

void SDKSampleApp::logout(const vector<string> &cmd)
{
    bool error = false;

    string accountHandle = CheckAndGetDefaultAccountHandle();
    if (accountHandle.empty()) {
        return;
    }
    for (vector<string>::const_iterator i = cmd.begin() + 1; i != cmd.end(); ++i) {
        if (*i == "-ah") {
            if (!nextArg(accountHandle, cmd, i, error)) {
                break;
            }
        } else {
            error = true;
            break;
        }
    }
    if (error) {
        PrintUsage(cmd.at(0), m_commands.find(cmd.at(0))->second.GetUsage());
        return;
    }
    con_print("\r * Logging out account handle %s\n", accountHandle.c_str());
    account_logout(accountHandle);
    InternalLoggedOut(accountHandle);
}

void SDKSampleApp::loginprops(const vector<string> &cmd)
{
    string accountHandle = CheckAndGetDefaultAccountHandle();
    if (accountHandle.empty()) {
        return;
    }

    int frequency = -1;
    bool error = false;

    for (vector<string>::const_iterator i = cmd.begin() + 1; i != cmd.end(); ++i) {
        if (*i == "-freq") {
            if (!nextArg(frequency, cmd, i, error)) {
                break;
            }
        } else if (*i == "-ah") {
            if (!nextArg(accountHandle, cmd, i, error)) {
                break;
            }
        } else {
            error = true;
            break;
        }
    }
    if (error) {
        PrintUsage(cmd.at(0), m_commands.find(cmd.at(0))->second.GetUsage());
        return;
    }

    con_print("\r * setting login properties %s\n", accountHandle.c_str());
    account_set_login_properties(accountHandle, frequency);
}

void SDKSampleApp::InternalLoggedOut(string accountHandle)
{
    lock_guard<mutex> lock(m_accountHandleUserNameMutex);
    if (m_accountHandles.find(accountHandle) != m_accountHandles.end()) {
        m_accountHandles.erase(accountHandle);
        m_accountHandleUserNames.erase(accountHandle);
    }
}

void SDKSampleApp::shutdown(const vector<string> &cmd)
{
    if (!CheckHasConnectorHandle()) {
        return;
    }
    if (cmd.size() > 1) {
        PrintUsage(cmd.at(0), m_commands.find(cmd.at(0))->second.GetUsage());
        return;
    }
    con_print("\r * Shutting down connector handle %s\n", m_connectorHandle.c_str());
    connector_initiate_shutdown(m_connectorHandle);

    lock_guard<mutex> lock(m_accountHandleUserNameMutex);
    m_SSGHandles.clear();
    m_sessions.clear();
    m_accountHandles.clear();
    m_accountHandleUserNames.clear();
    m_connectorHandle.clear();
}

void SDKSampleApp::buddy(const vector<string> &cmd)
{
    string scmd = "list";
    string uri;
    bool error = false;

    string accountHandle = CheckAndGetDefaultAccountHandle();
    if (accountHandle.empty()) {
        return;
    }
    for (vector<string>::const_iterator i = cmd.begin() + 1; i != cmd.end(); ++i) {
        if (*i == "-set") {
            if (!nextArg(uri, cmd, i, error)) {
                break;
            }
            scmd = "set";
        } else if (*i == "-delete") {
            if (!nextArg(uri, cmd, i, error)) {
                break;
            }
            scmd = "delete";
        } else if (*i == "-list") {
            scmd = "list";
        } else if (*i == "-ah") {
            if (!nextArg(accountHandle, cmd, i, error)) {
                break;
            }
        } else {
            error = true;
            break;
        }
    }
    if (error || (uri.empty() && scmd != "list")) {
        PrintUsage(cmd.at(0), m_commands.find(cmd.at(0))->second.GetUsage());
        return;
    }
    if (scmd == "set" || scmd == "delete") {
        con_print("\r * %s user %s as buddy on account handle %s\n", scmd == "set" ? "Setting " : "Deleting", uri.c_str(), accountHandle.c_str());
        if (scmd == "set") {
            account_buddy_set(accountHandle, uri);
        } else {
            account_buddy_delete(accountHandle, uri);
        }
    } else {
        con_print("\r * Listing buddies for account handle %s\n", accountHandle.c_str());
        account_list_buddies_and_groups(accountHandle);
    }
}

void SDKSampleApp::presence(const vector<string> &cmd)
{
    vx_buddy_presence_state state = buddy_presence_online;
    string message;
    bool error = false;

    string accountHandle = CheckAndGetDefaultAccountHandle();
    if (accountHandle.empty()) {
        return;
    }
    for (vector<string>::const_iterator i = cmd.begin() + 1; i != cmd.end(); ++i) {
        if (*i == "-available") {
            state = buddy_presence_online;
        } else if (*i == "-dnd") {
            state = buddy_presence_busy;
        } else if (*i == "-chat") {
            state = buddy_presence_chat;
        } else if (*i == "-away") {
            state = buddy_presence_away;
        } else if (*i == "-ea") {
            state = buddy_presence_extended_away;
        } else if (*i == "-m") {
            if (!nextArg(message, cmd, i, error)) {
                break;
            }
        } else {
            error = true;
            break;
        }
    }
    if (error) {
        PrintUsage(cmd.at(0), m_commands.find(cmd.at(0))->second.GetUsage());
        return;
    }
    con_print("\r * Setting Presence to %s for account handle %s\n", vx_get_presence_state_string(state), accountHandle.c_str());
    account_set_presence(accountHandle, state, message);
}

void SDKSampleApp::mutesession(const vector<string> &cmd)
{
    bool mute  = true;
    bool error = false;

    string sessionHandle;

    string scope = "all";
    vx_mute_scope s = mute_scope_all;

    checkScopeArg(scope, &s);

    for (vector<string>::const_iterator i = cmd.begin() + 1; i != cmd.end(); ++i) {
        if (*i == "-mute" || *i == "-unmute") {
            mute = *i == "-mute";
        } else if (*i == "-sh") {
            if (!nextArg(sessionHandle, cmd, i, error)) {
                break;
            }
        } else if (*i == "-scope") {
            if (!nextArg(scope, cmd, i, error)) {
                break;
            }
            if (!checkScopeArg(scope, &s)) {
                error = true;
                break;
            }
        } else {
            error = true;
            break;
        }
    }

    if (error) {
        PrintUsage(cmd.at(0), m_commands.find(cmd.at(0))->second.GetUsage());
        return;
    }
    if (sessionHandle.empty()) {
        sessionHandle = DefaultSessionHandle();
    }

    auto i = m_sessions.find(sessionHandle);
    if (i == m_sessions.end()) {
        con_print("\r * Error: on such session\n");
        return;
    }

    con_print("\r * %s session %s, scope = %s\n", mute ? "mute" : "unmute", sessionHandle.c_str(), scope.c_str());
    session_mute_local_speaker(sessionHandle, mute, s);
}

void SDKSampleApp::vadproperties(const vector<string> &cmd)
{
    bool get = true;
    bool reset = true;
    int hangover = m_vadHangover;
    int sensitivity = m_vadSensitivity;
    int noisefloor = m_vadNoiseFloor;
    int isAuto = m_vadAuto;
    string accountHandle;
    bool error = false;

    for (vector<string>::const_iterator i = cmd.begin() + 1; i != cmd.end(); ++i) {
        if (*i == "-set" || *i == "-get") {
            get = *i == "-get";
        } else if (*i == "-h") {
            reset = false;
            if (!nextArg(hangover, cmd, i, error)) {
                break;
            }
        } else if (*i == "-s") {
            reset = false;
            if (!nextArg(sensitivity, cmd, i, error)) {
                break;
            }
        } else if (*i == "-n") {
            reset = false;
            if (!nextArg(noisefloor, cmd, i, error)) {
                break;
            }
        } else if (*i == "-a") {
            reset = false;
            if (!nextArg(isAuto, cmd, i, error)) {
                break;
            }
        } else if (*i == "-ah") {
            if (!nextArg(accountHandle, cmd, i, error)) {
                break;
            }
        } else {
            error = true;
            break;
        }
    }
    if (error) {
        PrintUsage(cmd.at(0), m_commands.find(cmd.at(0))->second.GetUsage());
        return;
    }
    if (get) {
        con_print("\r * Getting VAD Properties\n");
        aux_get_vad_properties(accountHandle);
    } else {
        if (reset) {
            hangover = 2000;
            sensitivity = 43;
            noisefloor = 576;
            isAuto = 0;
        }
        con_print("\r * Setting VAD Properties (hangover=%d) (sensitivity=%d) (noisefloor=%d) (auto=%d)\n", hangover, sensitivity, noisefloor, isAuto);
        m_vadHangover = hangover;
        m_vadSensitivity = sensitivity;
        m_vadNoiseFloor = noisefloor;
        m_vadAuto = isAuto;
        aux_set_vad_properties(accountHandle, hangover, sensitivity, noisefloor, isAuto);
    }
}

void SDKSampleApp::derumblerproperties(const vector<string> &cmd)
{
    bool get = true;
    bool reset = true;
    int enabled = 1;
    int stopband_corner_frequency = 60;
    string accountHandle;
    bool error = false;

    for (vector<string>::const_iterator i = cmd.begin() + 1; i != cmd.end(); ++i) {
        if (*i == "-set" || *i == "-get") {
            get = *i == "-get";
        } else if (*i == "-on") {
            reset = false;
            enabled = 1;
        } else if (*i == "-off") {
            reset = false;
            enabled = 0;
        } else if (*i == "-freq") {
            reset = false;
            if (!nextArg(stopband_corner_frequency, cmd, i, error)) {
                break;
            }
        } else if (*i == "-ah") {
            if (!nextArg(accountHandle, cmd, i, error)) {
                break;
            }
        } else {
            error = true;
            break;
        }
    }
    if (error) {
        PrintUsage(cmd.at(0), m_commands.find(cmd.at(0))->second.GetUsage());
        return;
    }
    if (get) {
        con_print("\r * Getting Derumbler Properties\n");
        aux_get_derumbler_properties(accountHandle);
    } else {
        if (reset) {
            enabled = 1;
            stopband_corner_frequency = 60;
        }
        con_print("\r * Setting Derumbler Properties (enabled=%d) (stopband_corner_frequency=%d)\n", enabled, stopband_corner_frequency);
        aux_set_derumbler_properties(accountHandle, enabled, stopband_corner_frequency);
    }
}

void SDKSampleApp::archive(const vector<string> &cmd)
{
    bool isUserSpecified = false;
    string user;
    bool isChannelSpecified = false;
    string channel;
    int max = 10;
    string time_start;
    string time_end;
    string text;
    string after;
    string before;
    int index = -1;
    bool error = false;

    string accountHandle = CheckAndGetDefaultAccountHandle();
    if (accountHandle.empty()) {
        return;
    }
    for (vector<string>::const_iterator i = cmd.begin() + 1; i != cmd.end(); ++i) {
        if (*i == "-c") {
            if (isUserSpecified) {
                con_print("Error: %s: -u and -c are mutually exclusive.\n", cmd[0].c_str());
                error = true;
                break;
            }
            if (!nextArg(channel, cmd, i, error)) {
                break;
            }
            isChannelSpecified = true;
        } else if (*i == "-u") {
            if (isChannelSpecified) {
                con_print("Error: %s: -u and -c are mutually exclusive.\n", cmd[0].c_str());
                error = true;
                break;
            }
            if (!nextArg(user, cmd, i, error)) {
                break;
            }
            isUserSpecified = true;
        } else if (*i == "-max") {
            if (!nextArg(max, cmd, i, error)) {
                break;
            }
        } else if (*i == "-start") {
            if (!nextArg(time_start, cmd, i, error)) {
                break;
            }
        } else if (*i == "-end") {
            if (!nextArg(time_end, cmd, i, error)) {
                break;
            }
        } else if (*i == "-search") {
            if (!nextArg(text, cmd, i, error)) {
                break;
            }
        } else if (*i == "-after") {
            if (!nextArg(after, cmd, i, error)) {
                break;
            }
        } else if (*i == "-before") {
            if (!nextArg(before, cmd, i, error)) {
                break;
            }
        } else if (*i == "-index") {
            if (!nextArg(index, cmd, i, error)) {
                break;
            }
        } else {
            error = true;
            break;
        }
    }

    if (error) {
        PrintUsage(cmd.at(0), m_commands.find(cmd.at(0))->second.GetUsage());
        return;
    }

    con_print("\r * Querying archived messages for account handle %s\n", accountHandle.c_str());
    account_archive_query(accountHandle, (unsigned int)max, time_start, time_end, text, before, after, index, channel, user);
}

void SDKSampleApp::history(const vector<string> &cmd)
{
    string sessionHandle;
    string user;
    int max = 10;
    string time_start;
    string time_end;
    string text;
    string after;
    string before;
    int index = -1;
    bool error = false;

    if (!CheckHasSessionHandle()) {
        return;
    }
    for (vector<string>::const_iterator i = cmd.begin() + 1; i != cmd.end(); ++i) {
        if (*i == "-sh") {
            if (!nextArg(sessionHandle, cmd, i, error)) {
                break;
            }
        } else if (*i == "-u") {
            if (!nextArg(user, cmd, i, error)) {
                break;
            }
        } else if (*i == "-max") {
            if (!nextArg(max, cmd, i, error)) {
                break;
            }
        } else if (*i == "-start") {
            if (!nextArg(time_start, cmd, i, error)) {
                break;
            }
        } else if (*i == "-end") {
            if (!nextArg(time_end, cmd, i, error)) {
                break;
            }
        } else if (*i == "-search") {
            if (!nextArg(text, cmd, i, error)) {
                break;
            }
        } else if (*i == "-after") {
            if (!nextArg(after, cmd, i, error)) {
                break;
            }
        } else if (*i == "-before") {
            if (!nextArg(before, cmd, i, error)) {
                break;
            }
        } else if (*i == "-index") {
            if (!nextArg(index, cmd, i, error)) {
                break;
            }
        } else {
            error = true;
            break;
        }
    }

    if (sessionHandle.empty()) {
        sessionHandle = DefaultSessionHandle();
    }

    if (error || sessionHandle.empty()) {
        PrintUsage(cmd.at(0), m_commands.find(cmd.at(0))->second.GetUsage());
        return;
    }

    con_print("\r * Querying message history for session handle %s\n", sessionHandle.c_str());
    session_archive_messages(sessionHandle, (unsigned int)max, time_start, time_end, text, before, after, index, user);
}



void SDKSampleApp::sleep(const std::vector<std::string> &cmd)
{
    if (cmd.size() < 2) {
        PrintUsage(cmd.at(0), m_commands.find(cmd.at(0))->second.GetUsage());
        return;
    }

    int sleepSecs = 0;
    std::stringstream s(cmd[1]);
    s >> sleepSecs;
    std::this_thread::sleep_for(std::chrono::seconds(sleepSecs));
}

void SDKSampleApp::transcription(const std::vector<std::string> &cmd)
{
    string sessionHandle;
    int on = -1;
    bool error = false;

    if (!CheckHasSessionHandle()) {
        return;
    }
    for (vector<string>::const_iterator i = cmd.begin() + 1; i != cmd.end(); ++i) {
        if (*i == "-sh") {
            if (!nextArg(sessionHandle, cmd, i, error)) {
                break;
            }
        } else if (*i == "-on") {
            if (on != -1) {
                error = true;
            }
            on = 1;
        } else if (*i == "-off") {
            if (on != -1) {
                error = true;
            }
            on = 0;
        } else {
            error = true;
            break;
        }
    }

    if (on == -1) {
        error = true;
    }

    if (sessionHandle.empty()) {
        sessionHandle = DefaultSessionHandle();
    }

    if (error || sessionHandle.empty()) {
        PrintUsage(cmd.at(0), m_commands.find(cmd.at(0))->second.GetUsage());
        return;
    }

    string channel;
    auto i = m_sessions.find(sessionHandle);
    if (i != m_sessions.end()) {
        channel = i->second->GetUri();
    }
    if (channel.empty()) {
        con_print("Wrong session handle\n");
        return;
    }

    con_print("\r * Querying voice transcription %s (%s)\n", (on == 1) ? "on" : "off", sessionHandle.c_str());
    session_transcription_control(sessionHandle, channel, on == 1);
}

void SDKSampleApp::SetListenerPosition(const std::string session_handle, SampleAppPosition &position)
{
    m_listenerPositions[session_handle] = position;
}

void SDKSampleApp::GetListenerPosition(const std::string session_handle, SampleAppPosition &position)
{
    if (m_listenerPositions.find(session_handle) == m_listenerPositions.end()) {
        m_listenerPositions[session_handle] = { 0.0, 0.0, 0.0 };
    }

    position = m_listenerPositions[session_handle];
}

void SDKSampleApp::SetListenerOrientation(const std::string session_handle, SampleAppOrientation &orientation)
{
    m_listenerOrientations[session_handle] = orientation;
}

void SDKSampleApp::GetListenerOrientation(const std::string session_handle, SampleAppOrientation &orientation)
{
    if (m_listenerOrientations.find(session_handle) == m_listenerOrientations.end()) {
        double headingDegrees;
        GetListenerHeadingDegrees(session_handle, headingDegrees);
        m_listenerOrientations[session_handle] = {
            1.0 * sin(2 * SDK_SAMPLE_APP_PI * (headingDegrees / 360.0)), 0.0, -1.0 * cos(2 * SDK_SAMPLE_APP_PI * (headingDegrees / 360.0)),
            0.0, 1.0, 0.0  // SDKSampleApp does not support tilting. The 'up' vector is fixed to [0.0, 1.0, 0.0]
        };
    }

    orientation = m_listenerOrientations[session_handle];
}

void SDKSampleApp::GetListenerHeadingDegrees(const std::string session_handle, double &headingDegrees)
{
    if (m_listenerHeadingDegrees.find(session_handle) == m_listenerHeadingDegrees.end()) {
        m_listenerHeadingDegrees[session_handle] = 0.0;
    }

    headingDegrees = m_listenerHeadingDegrees[session_handle];
}

void SDKSampleApp::SetListenerHeadingDegrees(const std::string session_handle, double headingDegrees)
{
    m_listenerHeadingDegrees[session_handle] = headingDegrees;
}

void SDKSampleApp::Set3DPositionRequestFields(vx_req_session_set_3d_position *req, SampleAppPosition &position, SampleAppOrientation &orientation)
{
    req->listener_position[0] = position.x;
    req->listener_position[1] = position.y;
    req->listener_position[2] = position.z;

    req->listener_at_orientation[0] = orientation.at_x;
    req->listener_at_orientation[1] = orientation.at_y;
    req->listener_at_orientation[2] = orientation.at_z;

    req->listener_up_orientation[0] = orientation.up_x;
    req->listener_up_orientation[1] = orientation.up_y;
    req->listener_up_orientation[2] = orientation.up_z;

    req->speaker_position[0] = position.x;
    req->speaker_position[1] = position.y;
    req->speaker_position[2] = position.z;
}

extern "C" {
__declspec(dllimport) int vx_set_rtp_enabled(int inbound, int outbound);
__declspec(dllimport) int vx_get_rtp_enabled_inbound(void);
__declspec(dllimport) int vx_get_rtp_enabled_outbound(void);
}

void SDKSampleApp::rtp(const vector<string> &cmd)
{
    if (cmd.size() == 1) {
        // no arguments => display current status
        con_print(
                "\rRTP sending %s, receiving %s\n",
                vx_get_rtp_enabled_outbound() ? "enabled" : "disabled",
                vx_get_rtp_enabled_inbound() ? "enabled" : "disabled"
                );
        return;
    }
    bool error = false;
    int iSend = -1;
    int iRecv = -1;

    for (vector<string>::const_iterator i = cmd.begin() + 1; i != cmd.end(); ++i) {
        if (*i == "-send") {
            if (!nextArg(iSend, cmd, i, error)) {
                break;
            }
            if (iSend != 0 && iSend != 1) {
                error = true;
                break;
            }
        } else if (*i == "-recv") {
            if (!nextArg(iRecv, cmd, i, error)) {
                break;
            }
            if (iRecv != 0 && iRecv != 1) {
                error = true;
                break;
            }
        } else {
            error = true;
            break;
        }
    }

    if (error) {
        PrintUsage(cmd.at(0), m_commands.find(cmd.at(0))->second.GetUsage());
        return;
    }

    if (iSend != -1) {
        vx_set_rtp_enabled(vx_get_rtp_enabled_inbound(), iSend);
        con_print("\r%s RTP sending\n", iSend ? "Enabled" : "Disabled");
    }
    if (iRecv != -1) {
        vx_set_rtp_enabled(iRecv, vx_get_rtp_enabled_outbound());
        con_print("\r%s RTP receiving\n", iRecv ? "Enabled" : "Disabled");
    }
}


void SDKSampleApp::aec(const std::vector<std::string> &cmd)
{
    if (cmd.size() < 2) {
        int isEnabled;
        int result = vx_get_vivox_aec_enabled(&isEnabled);
        if (0 == result) {
            con_print("Echo Cancellation is %s\n", (isEnabled != 0) ? "enabled" : "disabled");
        } else {
            con_print("Error: %d (%s)\n", result, vx_get_error_string(result));
        }
    } else {
        bool enabled = false;
        if (cmd.at(1) == "-on") {
            enabled = true;
        } else if (cmd.at(1) == "-off") {
            enabled = false;
        } else {
            PrintUsage(cmd.at(0), m_commands.find(cmd.at(0))->second.GetUsage());
            return;
        }

        con_print("%s Echo Cancellation\n", enabled ? "Enabling" : "Disabling");

        vx_set_vivox_aec_enabled(enabled ? 1 : 0);
    }
}

void SDKSampleApp::agc(const std::vector<std::string> &cmd)
{
    if (cmd.size() < 2) {
        int isEnabled;
        int result = vx_get_agc_enabled(&isEnabled);
        if (0 == result) {
            con_print("Automatic Gain Control is %s\n", (isEnabled != 0) ? "enabled" : "disabled");
        } else {
            con_print("Error: %d (%s)\n", result, vx_get_error_string(result));
        }
    } else {
        bool enabled = false;
        if (cmd.at(1) == "-on") {
            enabled = true;
        } else if (cmd.at(1) == "-off") {
            enabled = false;
        } else {
            PrintUsage(cmd.at(0), m_commands.find(cmd.at(0))->second.GetUsage());
            return;
        }

        con_print("%s Automatic Gain Control\n", enabled ? "Enabling" : "Disabling");

        vx_set_agc_enabled(enabled ? 1 : 0);
    }
}

#ifndef SAMPLEAPP_DISABLE_TTS
vx_tts_status SDKSampleApp::InitializeTTSManagerIfNeeded()
{
    vx_tts_status statusCode;
    if (m_ttsManagerId == NULL) {
        vx_tts_manager_id tempId;
        statusCode = vx_tts_initialize(tts_engine_vivox_default, &tempId);
        if (statusCode != tts_status_success) {
            con_print("%s\n", vx_get_tts_status_string(statusCode));
            return statusCode;
        }
        m_ttsManagerId = new vx_tts_manager_id;
        *m_ttsManagerId = tempId;
    }
    return tts_status_success;
}

void SDKSampleApp::ttsspeak(const std::vector<std::string> &cmd)
{
    if (cmd.size() != 3 && cmd.size() < 5) {
        PrintUsage(cmd.at(0), m_commands.find(cmd.at(0))->second.GetUsage());
        return;
    }

    std::string text;
    std::string destString;

    bool argError = false;
    bool destEnabled = false;
    bool textEnabled = false;
    for (vector<string>::const_iterator i = cmd.begin() + 1; i != cmd.end(); ++i) {
        if (*i == "-text") {
            textEnabled = true;
            std::string word;
            bool foundDest = false;
            bool error = false;
            while (nextArg(word, cmd, i, error)) {
                if (word == "-dest") {
                    foundDest = true;
                    break;
                }
                text = text.empty() ? word : (text + " " + word);
            }
            if (!foundDest) {
                break;
            }
        }
        if (*i == "-dest") {
            destEnabled = true;
            bool error = false;
            if (!nextArg(destString, cmd, i, error)) {
                break;
            }
        } else {
            argError = true;
            break;
        }
    }

    if (argError || !destEnabled) {
        PrintUsage(cmd.at(0), m_commands.find(cmd.at(0))->second.GetUsage());
        return;
    }

    if (!textEnabled) {
        text = "Hello, this is an example sentence.";
    }

    // check if destination is a digit
    std::string::const_iterator it = destString.begin();
    while (it != destString.end() && std::isdigit(*it)) {
        ++it;
    }

    if (destString.empty() || it != destString.end()) {
        PrintUsage(cmd.at(0), m_commands.find(cmd.at(0))->second.GetUsage());
        return;
    }

    int destInt = (vx_tts_utterance_id)std::stoul(destString);
    if (destInt < 0 || destInt > 6) {
        con_print("Destination parameter is out of range.\n");
        return;
    }

    vx_tts_destination dest = static_cast<vx_tts_destination>(destInt);

    vx_tts_status statusCode = InitializeTTSManagerIfNeeded();
    if (statusCode != tts_status_success) {
        con_print("%s\n", vx_get_tts_status_string(statusCode));
        return;
    }

    vx_tts_utterance_id utteranceId;
    statusCode = vx_tts_speak(*m_ttsManagerId, m_ttsVoiceID, text.c_str(), dest, &utteranceId);
    con_print("%s\n", vx_get_tts_status_string(statusCode));
    if (statusCode == tts_status_success || statusCode == tts_status_input_text_was_enqueued) {
        con_print("%s %d\n", "Utterance ID is:", utteranceId);
    }
}

void SDKSampleApp::ttscancel(const std::vector<std::string> &cmd)
{
    if (cmd.size() != 1 && cmd.size() != 3) {
        PrintUsage(cmd.at(0), m_commands.find(cmd.at(0))->second.GetUsage());
        return;
    }

    if (m_ttsManagerId == NULL) {
        con_print("Cannot cancel TTS. Text-to-speech manager is not initialized\n");
        return;
    }

    if (cmd.size() == 1) {
        vx_tts_status statusCode = vx_tts_cancel_all(*m_ttsManagerId);
        con_print("%s\n", vx_get_tts_status_string(statusCode));
        return;
    }

    std::string destString;
    std::string idString;
    bool error = false;
    bool destEnabled = false;
    bool idEnabled = false;
    for (vector<string>::const_iterator i = cmd.begin() + 1; i != cmd.end(); ++i) {
        if (*i == "-dest") {
            destEnabled = true;
            if (!nextArg(destString, cmd, i, error)) {
                break;
            }
        } else if (*i == "-id") {
            idEnabled = true;
            if (!nextArg(idString, cmd, i, error)) {
                break;
            }
        } else {
            error = true;
            break;
        }
    }

    if (error || (!destEnabled && !idEnabled)) {
        PrintUsage(cmd.at(0), m_commands.find(cmd.at(0))->second.GetUsage());
        return;
    }

    if (destEnabled) {
        std::string::const_iterator it = destString.begin();
        while (it != destString.end() && std::isdigit(*it)) {
            ++it;
        }

        if (destString.empty() || it != destString.end()) {
            PrintUsage(cmd.at(0), m_commands.find(cmd.at(0))->second.GetUsage());
            return;
        }

        int destInt = (vx_tts_utterance_id)std::stoul(destString);
        if (destInt < 0 || destInt > 6) {
            con_print("Destination parameter is out of range.\n");
            return;
        }

        vx_tts_destination dest = static_cast<vx_tts_destination>(destInt);

        vx_tts_status statusCode = vx_tts_cancel_all_in_dest(*m_ttsManagerId, dest);
        con_print("%s\n", vx_get_tts_status_string(statusCode));
    } else if (idEnabled) {
        std::string::const_iterator it = idString.begin();
        while (it != idString.end() && std::isdigit(*it)) {
            ++it;
        }

        if (idString.empty() || it != idString.end()) {
            PrintUsage(cmd.at(0), m_commands.find(cmd.at(0))->second.GetUsage());
            return;
        }

        vx_tts_utterance_id id = (vx_tts_utterance_id)std::stoul(idString);
        vx_tts_status statusCode = vx_tts_cancel_utterance(*m_ttsManagerId, id);
        con_print("%s\n", vx_get_tts_status_string(statusCode));
    } else {
        PrintUsage(cmd.at(0), m_commands.find(cmd.at(0))->second.GetUsage());
    }
}

void SDKSampleApp::ttsshutdown(const std::vector<std::string> & /*cmd*/)
{
    if (m_ttsManagerId != NULL) {
        vx_tts_status statusCode = vx_tts_shutdown(m_ttsManagerId);
        con_print("%s\n", vx_get_tts_status_string(statusCode));
        if (statusCode == tts_status_success) {
            delete m_ttsManagerId;
            m_ttsManagerId = NULL;
        }
    } else {
        con_print("TTS engine cannot be shutdown because it is not initialized.\n");
    }
}

void SDKSampleApp::ttsvoice(const std::vector<std::string> &cmd)
{
    if (cmd.size() != 1 && cmd.size() != 2) {
        PrintUsage(cmd.at(0), m_commands.find(cmd.at(0))->second.GetUsage());
        return;
    }
    vx_tts_status statusCode = InitializeTTSManagerIfNeeded();
    if (statusCode != tts_status_success) {
        return;
    }
    int numberOfVoices;
    vx_tts_voice_t *voices;
    statusCode = vx_tts_get_voices(*m_ttsManagerId, &numberOfVoices, &voices);

    if (cmd.size() == 1) {
        for (int i = 0; i < numberOfVoices; i++) {
            con_print("id: %i \t name: %s\n", voices->voice_id, voices->name);
            voices++;
        }
        con_print("%s\n", vx_get_tts_status_string(statusCode));
    } else {
        std::string voiceIDString = cmd.at(1);

        std::string::const_iterator it = voiceIDString.begin();
        while (it != voiceIDString.end() && std::isdigit(*it)) {
            ++it;
        }
        if (voiceIDString.empty() || it != voiceIDString.end()) {
            PrintUsage(cmd.at(0), m_commands.find(cmd.at(0))->second.GetUsage());
            return;
        }

        vx_tts_voice_id voiceID = (vx_tts_voice_id)std::stoul(voiceIDString);
        m_ttsVoiceID = voiceID;
    }
}

void SDKSampleApp::ttsspeaktextchannel(const std::vector<std::string> &cmd)
{
    string sessionHandle;
    int on = -1;
    bool error = false;

    if (!CheckHasSessionHandle()) {
        return;
    }
    for (vector<string>::const_iterator i = cmd.begin() + 1; i != cmd.end(); ++i) {
        if (*i == "-sh") {
            if (!nextArg(sessionHandle, cmd, i, error)) {
                break;
            }
        } else if (*i == "-on") {
            if (on != -1) {
                error = true;
            }
            on = 1;
        } else if (*i == "-off") {
            if (on != -1) {
                error = true;
            }
            on = 0;
        } else {
            error = true;
            break;
        }
    }

    if (on == -1) {
        error = true;
    }

    if (sessionHandle.empty()) {
        sessionHandle = DefaultSessionHandle();
    }

    if (error || sessionHandle.empty()) {
        PrintUsage(cmd.at(0), m_commands.find(cmd.at(0))->second.GetUsage());
        return;
    }

    string channel;
    auto i = m_sessions.find(sessionHandle);
    if (i != m_sessions.end()) {
        channel = i->second->GetUri();
    }
    if (channel.empty()) {
        con_print("Wrong session handle\n");
        return;
    }

    vx_tts_status statusCode = InitializeTTSManagerIfNeeded();
    if (statusCode != tts_status_success) {
        return;
    }

    if (on == 1) {
        con_print("\r * Enabling text-to-speech for channel (%s)\n", sessionHandle.c_str());
        m_ttsSessionHandles.insert(sessionHandle);
    } else {
        con_print("\r * Disabling text-to-speech for channel (%s)\n", sessionHandle.c_str());
        m_ttsSessionHandles.erase(sessionHandle);
    }
}
#endif


void SDKSampleApp::backend(const std::vector<std::string> &cmd)
{
    bool error = false;

    if (cmd.size() > 1 && !CheckDisconnected()) {
        return;
    }

    string realm = m_realm;
    string server = m_server;
    string accessTokenIssuer = m_accessTokenIssuer;
    string accessTokenKey = m_accessTokenKey;
    bool isMultitenant = m_isMultitenant;

    for (vector<string>::const_iterator i = cmd.begin() + 1; i != cmd.end(); ++i) {
        if (*i == "-s") {
            if (!nextArg(server, cmd, i, error)) {
                break;
            }
        } else if (*i == "-r") {
            if (!nextArg(realm, cmd, i, error)) {
                break;
            }
        } else if (*i == "-k") {
            if (!nextArg(accessTokenKey, cmd, i, error)) {
                break;
            }
        } else if (*i == "-i") {
            if (!nextArg(accessTokenIssuer, cmd, i, error)) {
                break;
            }
        } else if (*i == "-m") {
            if (!nextArg(isMultitenant, cmd, i, error)) {
                break;
            }
        } else {
            error = true;
            break;
        }
    }

    if (error) {
        PrintUsage(cmd.at(0), m_commands.find(cmd.at(0))->second.GetUsage());
        return;
    }

    m_realm = realm;
    m_server = server;
    m_accessTokenIssuer = accessTokenIssuer;
    m_accessTokenKey = accessTokenKey;
    m_isMultitenant = isMultitenant;

    con_print("Backend Settings\n");
    con_print("%-16s %s\n", " * Server: ", m_server.c_str());
    con_print("%-16s %s\n", " * Realm: ", m_realm.c_str());
    con_print("%-16s %s\n", " * Issuer: ", m_accessTokenIssuer.c_str());
    con_print("%-16s %s\n", " * Key: ", m_accessTokenKey.c_str());
    con_print("%-16s %s\n", " * Multitenant: ", m_isMultitenant ? "enabled" : "disabled");
}
