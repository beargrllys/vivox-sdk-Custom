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

#include "joinchannelapp.h"
#include "resource.h"
#include <sstream>

using namespace std;

//
// Total Number of Players
// This is arbitrarily set to 8 - Vivox can support millions of players total, and hundreds in conferences
// If this is changed, then the IDD_MAINDIALOG resource will need to be edited to add more controls
//
#define NUM_PLAYERS 8

//
// Total number of Teams (channels)
// This is arbitrarily set for this demo Vivox can support millions of channels
// If this is changed, then the IDD_MAINDIALOG resource will need to be edited to add more controls
//
#define NUM_TEAMS 4

//
// The Player structure contains the Vivox username.  In Vivox version 5,  player names
// consist of the issuer key and a game server assigned identifier.  Usernames must also start with a period (.),
// followed by your issuer, followed by a period(.).For example, if your issuer name is
// "crux", the following is a valid username :
//   ".crux.0verl0rd."  for a player with the game server identifier of '0verl0rd'.

// Additionally, events about a particular Vivox ID, in voice, will be accompanied by a SIP URI.
// This URI is formulated from the account name.
//
typedef struct {
    char Displayname[50];  // only used in the main window
    char VivoxAccountName[100];
    char VivoxAccountUri[200];
} Player;

//
// This is the global list of players.
// The game will need to maintain this mapping for all players that are in a channel.
// (The password for all but one account are included here for demonstration purposes, but are not necessary in an actual game)
//
// For the purposes of this demonstration application, these accounts have been pre-created. In an actual game scenario
// the game server would create the accounts either ahead of time, or on demand.
//

static Player Players[NUM_PLAYERS];  // The list of players is larger than it needs to be for this sample application.

//
// The Team contains the mapping between the team name or game construct, and the Vivox channel resource that
// provides the voice associated with that resource. In this case, the game construct is a team, but it might
// be a region in 3D space, a guild, or other construct.
//
typedef struct {
    char TeamName[500];
    char VivoxChannelUri[500];
} Team;

//
// This is a global list of voice channels.
// Game servers determine the URI that is associated with the game voice chat for matches and parties
// and then send those URIs to game clients, usually with an access token for joining to the channel.
//
// For the purposes of this demonstration application, these resources have been pre-created. In an actual game scenario
// the game server would create the resources either ahead of time, or on demand.
//

static Team Teams[NUM_TEAMS] = {   // The Uris are initialized in the JoinChannelApp constructor
    { "Ephemeral Channel", "" },
    { "Echo Channel", "" },
    { "3D Channel", "" },
    { "No Channel", "" }
};

//
// This is list of user interface controls for team selection
// This particular application determines which player is in which channel using these controls.
// In a typical game, some other game initiated action would be used to determine which channels are joined under what circumstances.
//
static int teamControlIds[NUM_TEAMS] = {
    IDC_TEAM1,
    IDC_TEAM2,
    IDC_TEAM3
};
#define INDEX_3D_CHANNEL  2   // the index of the only 3d channel in the list of test channels
//
// This is list of user interface controls for player selection
// This particular application determines which player is in which channel using these controls.
// In a typical game, some other game initiated action would be used to determine which channels are joined under what circumstances.
//
static int playerControlIds[NUM_PLAYERS] = {
    IDC_RADIO1,
    IDC_RADIO2,
    IDC_RADIO3,
    IDC_RADIO4,
    IDC_RADIO5,
    IDC_RADIO6,
    IDC_RADIO7,
    IDC_RADIO8
};

//
// This is the list of user interface controls to show the user which player is logged in to Vivox
// This is specific to this demonstration application. In a game application, being able to log into the game, but not being
// able to login to Vivox is an exception condition. It's unlikely that a game would want to show
// the login status of the Vivox account independent of the game login status.
//
static int loginStatusControlIds[NUM_PLAYERS] = {
    IDC_LS1,
    IDC_LS2,
    IDC_LS3,
    IDC_LS4,
    IDC_LS5,
    IDC_LS6,
    IDC_LS7,
    IDC_LS8
};

//
// This is the list of controls that is used to show which player is which channel.
// This is specific to this demonstration application. In a game application, it's likely that some
// other user interface artifact would be used to show that the user is actually in voice.
// This might be replaced by a roster list for teams and guild channels, or this might
// be manifested as avatars that are visible to the user in a 3D MMORPG.
//
static int channelListControlIds[NUM_PLAYERS] = {
    IDC_CL1,
    IDC_CL2,
    IDC_CL3,
    IDC_CL4,
    IDC_CL5,
    IDC_CL6,
    IDC_CL7,
    IDC_CL8
};

//
// This is the list of controls that is used to show which players are speaking.
// A game should have a some artifact to visually cue a user which specific players are speaking.
// This helps the user mentally associated a voice with a player id.
//
static int speakingStateControlIds[NUM_PLAYERS] = {
    IDC_SS1,
    IDC_SS2,
    IDC_SS3,
    IDC_SS4,
    IDC_SS5,
    IDC_SS6,
    IDC_SS7,
    IDC_SS8
};

//
// This is the list of controls that is used to show which players are muted for all other players by a moderator.
// This may be useful to visually cue one user that another user is unable to speak
//
static int mutedForAllControlIds[NUM_PLAYERS] = {
    IDC_MM1,
    IDC_MM2,
    IDC_MM3,
    IDC_MM4,
    IDC_MM5,
    IDC_MM6,
    IDC_MM7,
    IDC_MM8
};

//
// This is the list of controls that is used to moderator mute another user
// This may be useful to visually cue one user that another user is unable to speak
//
static int muteForAllControlIds[NUM_PLAYERS] = {
    IDC_BTNMM1,
    IDC_BTNMM2,
    IDC_BTNMM3,
    IDC_BTNMM4,
    IDC_BTNMM5,
    IDC_BTNMM6,
    IDC_BTNMM7,
    IDC_BTNMM8
};

//
// This is the list of controls that is used to show which players are muted for all others players by a moderator.
// This may be useful to visually cue one user that another user is unable to speak
//
static int muteForMeControlIds[NUM_PLAYERS] = {
    IDC_CHECK1,
    IDC_CHECK2,
    IDC_CHECK3,
    IDC_CHECK4,
    IDC_CHECK5,
    IDC_CHECK6,
    IDC_CHECK7,
    IDC_CHECK8
};

//
// Given a Vivox channel URI, find the matching Team record
//
static int GetTeamIndex(const Uri &uri)
{
    for (int i = 0; i < NUM_TEAMS; ++i) {
        if (!strcmp(uri.ToString(), Teams[i].VivoxChannelUri)) {
            return i;
        }
    }
    return -1;
}

//
// Given a Vivox account name, find the matching Player record
//
static int GetPlayerIndex(const AccountName &accountName)
{
    for (int i = 0; i < NUM_PLAYERS; ++i) {
        if (!strcmp(accountName.ToString(), Players[i].VivoxAccountName)) {
            return i;
        }
    }
    return -1;
}

//
// Given a Vivox user URI, find the matching Player record
/*
static int GetPlayerIndex(const Uri &uri)
{
    for (int i = 0; i < NUM_PLAYERS; ++i) {
        if (!strcmp(uri.ToString(), Players[i].VivoxAccountUri)) {
            return i;
        }
    }
    return -1;
}
*/

#define APP_ISSUER "vivoxsampleapps-simple-dev"  // cut and paste the 'issuer' value from the 'API Keys' for your application to this #define
#define APP_KEY    "5hNGZZsish2TGk1RmFFaizIf6PkQMK0E"           // Get your application specific 'secret key' and set it here.
#define APP_DOMAIN "@vdx5.vivox.com"

static char *GenerateToken(const char *operation, const char *subjectUri, const char *fromName, const char *toUri)
{
    // In a production game client, the encoded token would come from a game server
    // None of the fields in the token would be visible strings.
    static unsigned long long serial = 1;
    std::string fromUri = std::string("sip:") + fromName + APP_DOMAIN;
    return vx_debug_generate_token(APP_ISSUER, (time_t) -1, operation, serial++, subjectUri, fromUri.c_str(), toUri, (const unsigned char *)APP_KEY, strlen(APP_KEY));
}

//
// JoinChannelApp Constructor
//
// @param hInst - the application HINSTANCE (specific to this demonstration application)
// @param hMainDialog - the main dialog window handle (specific to this demonstration application
//
JoinChannelApp::JoinChannelApp(HINSTANCE hInst, HWND hMainDialog, HWND hAudioSetup) :
    base(hInst),
    m_hwndMainDialog(hMainDialog),
    m_hwndAudioSetup(hAudioSetup),
    m_logLines(0)
{
    // Initialize the table of player names.  Most are not used.
    for (int x = 0; x < NUM_PLAYERS; x++) {
        sprintf_s(Players[x].Displayname, sizeof(Players[x].Displayname), "player%d",  x);
        sprintf_s(Players[x].VivoxAccountName, sizeof(Players[x].VivoxAccountName), ".%s.%s.", APP_ISSUER, Players[x].Displayname);
        sprintf_s(Players[x].VivoxAccountUri, sizeof(Players[x].VivoxAccountUri), "sip:%s%s", Players[x].VivoxAccountName, APP_DOMAIN);
    }


    sprintf_s(Teams[0].VivoxChannelUri, sizeof(Teams[0].VivoxChannelUri), "sip:confctl-g-%s.test123%s", APP_ISSUER, APP_DOMAIN);
    sprintf_s(Teams[1].VivoxChannelUri, sizeof(Teams[1].VivoxChannelUri), "sip:confctl-e-%s.echo%s", APP_ISSUER, APP_DOMAIN);
    sprintf_s(Teams[2].VivoxChannelUri, sizeof(Teams[2].VivoxChannelUri), "sip:confctl-d-%s.3dsimpleapi%s", APP_ISSUER, APP_DOMAIN);
    sprintf_s(Teams[3].VivoxChannelUri, sizeof(Teams[3].VivoxChannelUri), "sip:confctl-d-%s.3dsimpleapi%s", APP_ISSUER, APP_DOMAIN);


    SetClientConnection(&m_vivox);

    // Setup the control handles for the Audio Settings Dialog
    m_hwndOsSystemOutputRadio = GetDlgItem(m_hwndAudioSetup, IDC_RADIO_OUTPUT_OS_SYSTEM);
    m_hwndOsCommOutputRadio = GetDlgItem(m_hwndAudioSetup, IDC_RADIO_OUTPUT_OS_COMM);
    m_hwndAppOutputRadio = GetDlgItem(m_hwndAudioSetup, IDC_RADIO_OUTPUT_CHOOSE);
    m_hwndOsSystemInputRadio = GetDlgItem(m_hwndAudioSetup, IDC_RADIO_INPUT_OS_SYSTEM);
    m_hwndOsCommInputRadio = GetDlgItem(m_hwndAudioSetup, IDC_RADIO_INPUT_OS_COMM);

    m_hwndInputDevices = GetDlgItem(m_hwndAudioSetup, IDC_CHOOSE_INPUT);
    m_hwndOutputDevices = GetDlgItem(m_hwndAudioSetup, IDC_CHOOSE_OUTPUT);
    m_hwndInputVolume = GetDlgItem(m_hwndAudioSetup, IDC_INPUT_VOLUME);
    m_hwndOutputVolume = GetDlgItem(m_hwndAudioSetup, IDC_OUTPUT_VOLUME);
    m_hwndTestInputRecord = GetDlgItem(m_hwndAudioSetup, IDC_RECORD);
    m_hwndTestInputPlayback = GetDlgItem(m_hwndAudioSetup, IDC_PLAY_RECORDING);
    m_hwndTestOutput = GetDlgItem(m_hwndAudioSetup, IDC_TEST_OUTPUT);

    m_hwndMuteInput = GetDlgItem(m_hwndMainDialog, IDC_MUTEINPUT);
    m_hwndMuteOutput = GetDlgItem(m_hwndMainDialog, IDC_MUTEOUTPUT);

    InternalSendMessage(m_hwndInputVolume, TBM_SETRANGE, FALSE, (VIVOX_MAX_VOL << 16) | VIVOX_MIN_VOL);
    InternalSendMessage(m_hwndOutputVolume, TBM_SETRANGE, FALSE, (VIVOX_MAX_VOL << 16) | VIVOX_MIN_VOL);
    InternalSendMessage(m_hwndInputVolume, TBM_SETPOS, TRUE, m_vivox.GetMasterAudioInputDeviceVolume());
    InternalSendMessage(m_hwndOutputVolume, TBM_SETPOS, TRUE, m_vivox.GetMasterAudioOutputDeviceVolume());


// Initialize all the controls on the main screen properly
    HWND h;
    for (int i = 0; i < NUM_PLAYERS; ++i) {
        h = GetDlgItem(m_hwndMainDialog, playerControlIds[i]);
        InternalSendMessage(h, WM_SETTEXT, 0, (LPARAM)Players[i].Displayname);
        h = GetDlgItem(m_hwndMainDialog, loginStatusControlIds[i]);
        InternalSendMessage(h, WM_SETTEXT, 0, (LPARAM)"Logged Out");
        h = GetDlgItem(m_hwndMainDialog, channelListControlIds[i]);
        InternalSendMessage(h, WM_SETTEXT, 0, (LPARAM)"");
        h = GetDlgItem(m_hwndMainDialog, speakingStateControlIds[i]);
        InternalSendMessage(h, WM_SETTEXT, 0, (LPARAM)"");
        h = GetDlgItem(m_hwndMainDialog, mutedForAllControlIds[i]);
        InternalSendMessage(h, WM_SETTEXT, 0, (LPARAM)"");
        h = GetDlgItem(m_hwndMainDialog, muteForMeControlIds[i]);
        Button_Enable(h, FALSE);
        h = GetDlgItem(m_hwndMainDialog, muteForAllControlIds[i]);
        Button_Enable(h, FALSE);
    }
    h = GetDlgItem(m_hwndMainDialog, IDC_TEAM4);
    InternalSendMessage(h, BM_SETCHECK, BST_CHECKED, 0);

    Button_SetCheck(m_hwndMuteInput, m_vivox.GetAudioInputDeviceMuted() ? TRUE : FALSE);
    Button_SetCheck(m_hwndMuteOutput, m_vivox.GetAudioOutputDeviceMuted() ? TRUE : FALSE);

    this->onAvailableAudioDevicesChanged();

    this->UpdateAudioTestButtons();
}

//
// JoinChannelApp destructor
//
JoinChannelApp::~JoinChannelApp()
{
}

//
// JoinChannelApp::Connect
//
// Starts the connection process to the specified Vivox provisioning server.
// This is a non-blocking operation, but could take seconds to complete.
//
// @param strBackend - a pointer to the URL of the Vivox provisioning server e.g. https://www.vd1.vivox.com/api2
// @return - 0 if successful, otherwise, an error code. Returned errors are usually a failure to provide proper arguments to the method.
//
VCSStatus JoinChannelApp::Connect(const char *strBackend)
{
    VCSStatus vcs;
    Uri backend(strBackend);

    stringstream ss;
    ss << "Connecting to " << strBackend << "...\r\n";
    WriteStatus(ss.str().c_str());
    vcs = m_vivox.Connect(backend);
    return vcs;
}

//
// JoinChannelApp::Update
//
// Given the state of the controls on the main screen, determines which user needs to be in which channel (if any)
//
// @return - 0 if successful, otherwise, an error code. Returned errors are usually a failure to provide proper arguments to the method.
//
VCSStatus JoinChannelApp::Update()
{
    int playerIndex = -1;
    int teamIndex = -1;

    // Find the selected team, the last team in the list has to the the 'no channel'
    for (int i = 0; i < NUM_TEAMS - 1; ++i) {
        HWND h = GetDlgItem(m_hwndMainDialog, teamControlIds[i]);
        if (InternalSendMessage(h, BM_GETCHECK, 0, 0) == BST_CHECKED) {
            teamIndex = i;
            break;
        }
    }

    // Find the selected player
    for (int i = 0; i < NUM_PLAYERS; ++i) {
        HWND h = GetDlgItem(m_hwndMainDialog, playerControlIds[i]);
        if (InternalSendMessage(h, BM_GETCHECK, 0, 0) == BST_CHECKED) {
            playerIndex = i;
            break;
        }
    }

    // If no player is selected, there is nothing to do
    if (playerIndex < 0) {
        return 0;
    }

    VCSStatus vcs;
    AccountName accountName(Players[playerIndex].VivoxAccountName);

    if (teamIndex >= 0) {
        // If we have a team, then log the user in and put them in the channel
        Uri channelUri(Teams[teamIndex].VivoxChannelUri);
        stringstream ss;
        ss << "Putting " << accountName.ToString() << " into channel " << Teams[teamIndex].TeamName << "...\r\n";
        WriteStatus(ss.str().c_str());

        char *accessTokenLogin = GenerateToken("login", NULL, accountName.ToString(), NULL);
        vcs = m_vivox.Login(accountName, accessTokenLogin);
        vx_free(accessTokenLogin);
        if (vcs != 0) {
            return vcs;
        }

        char *accessTokenJoin = GenerateToken("join", NULL, accountName.ToString(), channelUri.ToString());
        vcs = m_vivox.JoinChannel(accountName, channelUri, accessTokenJoin);
        vx_free(accessTokenJoin);
        if (vcs != 0) {
            m_vivox.Logout(accountName);
            return vcs;
        }
    } else {
        // No team, then just make sure the selected user is logged in and not in any channels
        stringstream ss;
        ss << "Putting " << accountName.ToString() << " into no channels\r\n";
        WriteStatus(ss.str().c_str());
        char *accessTokenLogin = GenerateToken("login", NULL, accountName.ToString(), NULL);
        vcs = m_vivox.Login(accountName, accessTokenLogin);
        vx_free(accessTokenLogin);
        if (vcs != 0) {
            return vcs;
        }
        vcs = m_vivox.LeaveAll(accountName);
        if (vcs != 0) {
            return vcs;
        }
    }
    return vcs;
}

//
// JoinChannelApp::WriteStatus
//
// Adds a log line to the listbox on the main dialog window.
// This is specific to this demonstration application to aid developers in understanding the application.
// This overrides the WriteStatus method of the DebugClientApiEventHandler class.
//
// @param msg - the message to write
//
void JoinChannelApp::WriteStatus(const char *msg) const
{
    HWND hWndLog = GetDlgItem(m_hwndMainDialog, IDC_LIST_LOG);
    InternalSendMessage(hWndLog, LB_ADDSTRING, 0, (LPARAM)msg);
    InternalSendMessage(hWndLog, LB_SETTOPINDEX, m_logLines, 0);
    m_logLines++;
}

//
// JoinChannelApp::onLoginCompleted
//
// This is called when a user logs in successfully.
//
// @param accountName - the Vivox account name of the logged in user
//
void JoinChannelApp::onLoginCompleted(const AccountName &accountName)
{
    // update the login status control for that user
    int idx = GetPlayerIndex(accountName);
    HWND h = GetDlgItem(m_hwndMainDialog, loginStatusControlIds[idx]);
    InternalSendMessage(h, WM_SETTEXT, 0, (LPARAM)"Logged In");
}

//
// JoinChannelApp::onLogoutCompleted
//
// This is called when a user logs out successfully.
//
// @param accountName - the Vivox account name of the logged in user
//
void JoinChannelApp::onLogoutCompleted(const AccountName &accountName)
{
    // update the login status control, the channel list control and speaking state control
    // for this user.
    int idx = GetPlayerIndex(accountName);
    HWND h = GetDlgItem(m_hwndMainDialog, loginStatusControlIds[idx]);
    InternalSendMessage(h, WM_SETTEXT, 0, (LPARAM)"Logged Out");
    h = GetDlgItem(m_hwndMainDialog, speakingStateControlIds[idx]);
    InternalSendMessage(h, WM_SETTEXT, 0, (LPARAM)"");
    h = GetDlgItem(m_hwndMainDialog, channelListControlIds[idx]);
    InternalSendMessage(h, WM_SETTEXT, 0, (LPARAM)"");
    h = GetDlgItem(m_hwndMainDialog, mutedForAllControlIds[idx]);
    InternalSendMessage(h, WM_SETTEXT, 0, (LPARAM)"");
    h = GetDlgItem(m_hwndMainDialog, muteForMeControlIds[idx]);
    Button_Enable(h, FALSE);
    h = GetDlgItem(m_hwndMainDialog, muteForAllControlIds[idx]);
    Button_Enable(h, FALSE);
}

//
// JoinChannelApp::onChannelJoined
//
// This method is here so that the debug message for channel joining is not presented.
//
// The participant added events are what games should use as a indication that the user is
// successfully in a channel.
//
void JoinChannelApp::onChannelJoined(const AccountName &accountName, const Uri &channelUri)
{
    (void)accountName;
    (void)channelUri;
}

//
// JoinChannelApp::onChannelExited
//
// onChannelExited is only of use to applications where there is a non-zero reason code.
// This reason code may indicated a provisioning error, a game application error, or a transient network error
// which prevented the voice session from being or staying established.
//
// @param accountName - the logged in account
// @param channelUri - the URI of the channel that was exited
// @param reasonCode - why the channel was exited
//
void JoinChannelApp::onChannelExited(const AccountName &accountName, const Uri &channelUri, VCSStatus reasonCode)
{
    //
    if (reasonCode != 0) {
        base::onChannelExited(accountName, channelUri, reasonCode);
    }
}

//
// JoinChannelApp::onParticipantAdded
//
// When a participant joins a channel, we'll get this notification. Use this to build/update the roster list for the channel.
//
// @param accountName - the name of the logged in account
// @param channelUri - the URI of the channel that had a participant added to it
// @param participantUri - the URI of the participant added.
// @param isLoggedInUser - if this participant is the logged in user, this will ben true. This is often helpful
// when the logged in user does not appear in the roster list, but has a separate affordance indicating that they are "in voice".
//
void JoinChannelApp::onParticipantAdded(const AccountName &accountName, const Uri &channelUri, const Uri &participantUri, bool isLoggedInUser)
{
    base::onParticipantAdded(accountName, channelUri, participantUri, isLoggedInUser);
    int playerIdx = GetPlayerIndex(accountName);
    if (playerIdx < 0) {
        std::stringstream ss;
        ss << "WARNING: UNEXPECTED CHANNEL PARTICIPANT ADDED: " << participantUri.ToString() << "\r\n";
        WriteStatus(ss.str().c_str());
        return;
    }
    HWND h = GetDlgItem(m_hwndMainDialog, channelListControlIds[playerIdx]);
    int teamIdx = GetTeamIndex(channelUri);
    InternalSendMessage(h, WM_SETTEXT, 0, (LPARAM)Teams[teamIdx].TeamName);
    h = GetDlgItem(m_hwndMainDialog, muteForMeControlIds[playerIdx]);
    Button_Enable(h, isLoggedInUser ? FALSE : TRUE);
    h = GetDlgItem(m_hwndMainDialog, muteForAllControlIds[playerIdx]);
    Button_Enable(h, isLoggedInUser ? FALSE : TRUE);
}

//
// JoinChannelApp::onParticipantLeft
//
// When a participant joins a channel, we'll get this notification. Use this to build/update the roster list for the channel.
//
// @param accountName - the name of the logged in account
// @param channelUri - the URI of the channel that had a participant added to it
// @param participantUri - the URI of the participant added.
// @param isLoggedInUser - if this participant is the logged in user, this will ben true. This is often helpful
// when the logged in user does not appear in the roster list, but has a separate affordance indicating that they are "in voice".
// @param reason - the reason why the user left. For most game applications, the most important reason is ReasonNetwork - this indicates that
// the user lost connectivity to the conferencing server.
//
void JoinChannelApp::onParticipantLeft(const AccountName &accountName, const Uri &channelUri, const Uri &participantUri, bool isLoggedInUser, ParticipantLeftReason reason)
{
    base::onParticipantLeft(accountName, channelUri, participantUri, isLoggedInUser, reason);
    int idx = GetPlayerIndex(accountName);
    if (idx < 0) {
        std::stringstream ss;
        ss << "WARNING: UNEXPECTED CHANNEL PARTICIPANT LEFT: " << participantUri.ToString() << "\r\n";
        WriteStatus(ss.str().c_str());
        return;
    }

    char oldText[80];
    HWND h = GetDlgItem(m_hwndMainDialog, channelListControlIds[idx]);
    InternalSendMessage(h, WM_GETTEXT, sizeof(oldText), (LPARAM)oldText);
    idx = GetTeamIndex(channelUri);
    if (!strcmp(oldText, Teams[idx].TeamName)) {
        // only clear out the controls if we left the last channel
        InternalSendMessage(h, WM_SETTEXT, 0, (LPARAM)"");
        h = GetDlgItem(m_hwndMainDialog, speakingStateControlIds[idx]);
        InternalSendMessage(h, WM_SETTEXT, 0, (LPARAM)"");
        h = GetDlgItem(m_hwndMainDialog, mutedForAllControlIds[idx]);
        InternalSendMessage(h, WM_SETTEXT, 0, (LPARAM)"");
        h = GetDlgItem(m_hwndMainDialog, muteForMeControlIds[idx]);
        Button_Enable(h, FALSE);
        h = GetDlgItem(m_hwndMainDialog, muteForAllControlIds[idx]);
        Button_Enable(h, FALSE);
    }
}

//
// JoinChannelApp::onParticipantLeft
//
// When a participant joins a channel, we'll get this notification. Use this to build/update the roster list for the channel.
//
// @param accountName - the name of the logged in account. In this application only one account at a time is logged in so this parameter is ignored.
// @param channelUri - the URI of the channel that had a participant added to it. In this application only one channel is joined at a time so this parameter is ignored.
// @param participantUri - the URI of the participant added.
// @param isLoggedInUser - if this participant is the logged in user, this will ben true. This is often helpful
// when the logged in user does not appear in the roster list, but has a separate affordance indicating that they are "in voice".
// @speaking - whether the participant is speaking or silent
// @vuMeterEnergy - a value between 0 and 1 indicating the relative volume of the speech. This particular application
// has elected to not receive update events when vuMeterEnergy changes, so this value is ignored.
//
void JoinChannelApp::onParticipantUpdated(const AccountName &accountName, const Uri &channelUri, const Uri &participantUri, bool isLoggedInUser, bool speaking, double vuMeterEnergy, bool isMutedForAll)
{
    (void)vuMeterEnergy;
    (void)channelUri;
    (void)accountName;
    int idx = GetPlayerIndex(accountName);
    if (idx < 0) {
        std::stringstream ss;
        ss << "WARNING: UNEXPECTED CHANNEL PARTICIPANT UPDATED: " << participantUri.ToString() << "\r\n";
        WriteStatus(ss.str().c_str());
        return;
    }
    HWND h = GetDlgItem(m_hwndMainDialog, speakingStateControlIds[idx]);
    const char *strSpeaking = "";
    if (speaking) {
        if (isLoggedInUser) {
            strSpeaking = "I'm speaking";
        } else {
            strSpeaking = "They're speaking";
        }
    }
    InternalSendMessage(h, WM_SETTEXT, 0, (LPARAM)strSpeaking);

    h = GetDlgItem(m_hwndMainDialog, mutedForAllControlIds[idx]);
    const char *strMuted = "";
    if (isMutedForAll) {
        strMuted = "Mod Muted";
    }
    Static_SetText(h, strMuted);

    h = GetDlgItem(m_hwndMainDialog, muteForAllControlIds[idx]);
    const char *strMute = "Mod Mute";
    if (isMutedForAll) {
        strMute = "Mod Unmute";
    }
    Button_SetText(h, strMute);
}

///
/// Called when a checkbox is clicked on the main screen to mute the user
///
/// @param controlId - the numeric id of the checkbox
void JoinChannelApp::MuteForMe(int controlId)
{
    int playerIndex = -1;
    int teamIndex = -1;

    // Find the selected team avoid the last team/channel which is the 'no channel'
    for (int i = 0; i < NUM_TEAMS - 1; ++i) {
        HWND h = GetDlgItem(m_hwndMainDialog, teamControlIds[i]);
        if (InternalSendMessage(h, BM_GETCHECK, 0, 0) == BST_CHECKED) {
            teamIndex = i;
            break;
        }
    }

    // Find the selected player
    for (int i = 0; i < NUM_PLAYERS; ++i) {
        HWND h = GetDlgItem(m_hwndMainDialog, playerControlIds[i]);
        if (InternalSendMessage(h, BM_GETCHECK, 0, 0) == BST_CHECKED) {
            playerIndex = i;
            break;
        }
    }

    // If no player is selected, there is nothing to do
    if (playerIndex < 0) {
        return;
    }

    HWND h = GetDlgItem(m_hwndMainDialog, controlId);
    for (int i = 0; i < NUM_PLAYERS; ++i) {
        if (controlId == muteForMeControlIds[i]) {
            bool mute = Button_GetCheck(h) != FALSE;

            m_vivox.SetParticipantMutedForMe(
                    AccountName(Players[playerIndex].VivoxAccountName),
                    Uri(Players[i].VivoxAccountUri),
                    Uri(Teams[teamIndex].VivoxChannelUri),
                    mute);
            break;
        }
    }
}

///
/// Called when a button on the main screen is clicked to mute the user in the cnannel for all other users.
///
/// @param controlId - the numeric id of the checkbox
void JoinChannelApp::MuteForAll(int controlId)
{
    int playerIndex = -1;
    int teamIndex = -1;

    // Find the selected team, don't check for the last channel entry which is 'no channel'
    for (int i = 0; i < NUM_TEAMS - 1; ++i) {
        HWND h = GetDlgItem(m_hwndMainDialog, teamControlIds[i]);
        if (InternalSendMessage(h, BM_GETCHECK, 0, 0) == BST_CHECKED) {
            teamIndex = i;
            break;
        }
    }

    // Find the selected player
    for (int i = 0; i < NUM_PLAYERS; ++i) {
        HWND h = GetDlgItem(m_hwndMainDialog, playerControlIds[i]);
        if (InternalSendMessage(h, BM_GETCHECK, 0, 0) == BST_CHECKED) {
            playerIndex = i;
            break;
        }
    }

    // If no player is selected, there is nothing to do
    if (playerIndex < 0) {
        return;
    }

    for (int i = 0; i < NUM_PLAYERS; ++i) {
        if (controlId == muteForAllControlIds[i]) {
            bool mutedForAll = m_vivox.GetParticipantMutedForAll(
                    AccountName(Players[playerIndex].VivoxAccountName),
                    Uri(Players[i].VivoxAccountUri),
                    Uri(Teams[teamIndex].VivoxChannelUri));
            char *accessTokenMute = GenerateToken(
                    "mute",
                    Uri(Players[i].VivoxAccountUri).ToString(),
                    AccountName(Players[playerIndex].VivoxAccountName).ToString(),
                    Uri(Teams[teamIndex].VivoxChannelUri).ToString());
            m_vivox.SetParticipantMutedForAll(
                    AccountName(Players[playerIndex].VivoxAccountName),
                    Uri(Players[i].VivoxAccountUri),
                    Uri(Teams[teamIndex].VivoxChannelUri),
                    !mutedForAll,
                    accessTokenMute);
            vx_free(accessTokenMute);
            break;
        }
    }
}

/// Called when the 'Apply' button is pressed on the main dialog to set a position in the 3d channel.
/// The button is dumb in that it is does not check to see if the channel have been joined or if a player is logged in.

void JoinChannelApp::SetPositionIn3dChannel()
{
    int playerIndex = -1;

    // Check to see that a player is selected
    // Find the selected player
    for (int i = 0; i < NUM_PLAYERS; ++i) {
        HWND h = GetDlgItem(m_hwndMainDialog, playerControlIds[i]);
        if (InternalSendMessage(h, BM_GETCHECK, 0, 0) == BST_CHECKED) {
            playerIndex = i;
            break;
        }
    }

    // Get the new coordinates from the X, Y, and Z textboxes.
    // Empty values for the text boxes will mean zero
    double X, Y, Z;


    X = (int)GetDlgItemInt(m_hwndMainDialog, IDC_EDIT1, false, true);   // position right (positive numbers) or left (negative numbers)
    Y = (int)GetDlgItemInt(m_hwndMainDialog, IDC_EDIT2, false, true);   // position up (positive) or down (negative)
    Z = (int)GetDlgItemInt(m_hwndMainDialog, IDC_EDIT3, false, true);   // position backward (positive) or forward (negative)


    // Set the listener AT orientation to the default setting for the purposes of this demo application.
    // The setting for the the player 'up' vector is not set in this API.
    VCSStatus vcs;
    std::stringstream ss;
    vcs = m_vivox.Set3DPosition(AccountName(Players[playerIndex].VivoxAccountName), Uri(Teams[INDEX_3D_CHANNEL].VivoxChannelUri), X, Y, Z, 0, 0, -1);
    if (vcs != VXA_SUCCESS) {
        ss << "Setting the Channel position returned --> " << vcs;
        ss << "    Check that a player has been selected and that they are in the 3d channel.";
        WriteStatus(ss.str().c_str());
    } else {
        ss << "Position set to --> " << X << ", " << Y << ", " << Z;
        WriteStatus(ss.str().c_str());
    }
}

///
/// JoinChannelApp::onAvailableAudioDevicesChanged
///
/// When an audio device is either removed or added, we'll get this notification.
///
/// In this function, we completely update the audio setting screen. This include:
/// 1. Indicating to the user whether we are letting the operating system select the audio devices or whether the application (user) has selected to use a particular audio device
///    independent of the operating systems choice.
/// 2. Updating the dropdown controls with the list of available devices
/// 3. Updating what the operating system is or would use for the audio devices
/// 4. Updating the lists of available devices
///
/// Note that this function is also called when the audio settings dialog is created.
///

void JoinChannelApp::onAvailableAudioDevicesChanged()
{
    base::onAvailableAudioDevicesChanged();

    ComboBox_ResetContent(m_hwndInputDevices);
    for (std::vector<AudioDeviceId>::const_iterator i = m_vivox.GetAvailableAudioInputDevices().begin();
         i != m_vivox.GetAvailableAudioInputDevices().end(); ++i)
    {
        ComboBox_AddString(m_hwndInputDevices, i->GetAudioDeviceDisplayName().c_str());
    }

    ComboBox_ResetContent(m_hwndOutputDevices);
    for (std::vector<AudioDeviceId>::const_iterator i = m_vivox.GetAvailableAudioOutputDevices().begin();
         i != m_vivox.GetAvailableAudioOutputDevices().end(); ++i)
    {
        ComboBox_AddString(m_hwndOutputDevices, i->GetAudioDeviceDisplayName().c_str());
    }
    ComboBox_SetMinVisible(m_hwndOutputDevices, m_vivox.GetAvailableAudioOutputDevices().size());

    {
        {
            std::stringstream ss;
            if (m_vivox.GetDefaultSystemAudioOutputDevice().GetAudioDeviceDisplayName().empty()) {
                ss << "Use Default System Device -  - WARNING: NO AUDIO OUTPUT DEVICE AVAILABLE";
            } else {
                ss << "Use Default System Device - Currently " << m_vivox.GetDefaultSystemAudioOutputDevice().GetAudioDeviceDisplayName();
            }
            Button_SetText(m_hwndOsSystemOutputRadio, ss.str().c_str());
        }

        {
            std::stringstream ss;
            if (m_vivox.GetDefaultSystemAudioOutputDevice().GetAudioDeviceDisplayName().empty()) {
                ss << "Use Default Communication Device -  - WARNING: NO AUDIO OUTPUT DEVICE AVAILABLE";
            } else {
                ss << "Use Default Communication Device - Currently " << m_vivox.GetDefaultCommunicationAudioOutputDevice().GetAudioDeviceDisplayName();
            }
            Button_SetText(m_hwndOsCommOutputRadio, ss.str().c_str());
        }

        if (m_vivox.IsUsingDefaultSystemAudioOutputDevice()) {
            Button_SetCheck(m_hwndOsSystemOutputRadio, TRUE);
            Button_SetCheck(m_hwndOsCommOutputRadio, FALSE);
            Button_SetCheck(m_hwndAppOutputRadio, FALSE);
            ComboBox_Enable(m_hwndOutputDevices, FALSE);
        } else if (m_vivox.IsUsingDefaultCommunicationAudioOutputDevice()) {
            Button_SetCheck(m_hwndOsSystemOutputRadio, FALSE);
            Button_SetCheck(m_hwndOsCommOutputRadio, TRUE);
            Button_SetCheck(m_hwndAppOutputRadio, FALSE);
            ComboBox_Enable(m_hwndOutputDevices, FALSE);
        } else {
            Button_SetCheck(m_hwndOsSystemOutputRadio, FALSE);
            Button_SetCheck(m_hwndOsCommOutputRadio, FALSE);
            Button_SetCheck(m_hwndAppOutputRadio, TRUE);
            ComboBox_Enable(m_hwndOutputDevices, TRUE);
            int idx = ComboBox_FindStringExact(m_hwndOutputDevices, 0, m_vivox.GetAudioOutputDevice().GetAudioDeviceDisplayName().c_str());
            ComboBox_SetCurSel(m_hwndOutputDevices, idx);
        }
    }

    {
        {
            std::stringstream ss;
            if (m_vivox.GetDefaultSystemAudioInputDevice().GetAudioDeviceDisplayName().empty()) {
                ss << "Use Default System Device - WARNING: NO AUDIO INPUT DEVICE AVAILABLE";
            } else {
                ss << "Use Default System Device - Currently " << m_vivox.GetDefaultSystemAudioInputDevice().GetAudioDeviceDisplayName();
            }
            Button_SetText(m_hwndOsSystemInputRadio, ss.str().c_str());
        }

        {
            std::stringstream ss;
            if (m_vivox.GetDefaultCommunicationAudioInputDevice().GetAudioDeviceDisplayName().empty()) {
                ss << "Use Default Communication Device - WARNING: NO AUDIO INPUT DEVICE AVAILABLE";
            } else {
                ss << "Use Default Communication Device - Currently " << m_vivox.GetDefaultCommunicationAudioInputDevice().GetAudioDeviceDisplayName();
            }
            Button_SetText(m_hwndOsCommInputRadio, ss.str().c_str());
        }

        if (m_vivox.IsUsingDefaultSystemAudioInputDevice()) {
            Button_SetCheck(m_hwndOsSystemInputRadio, TRUE);
            Button_SetCheck(m_hwndOsCommInputRadio, FALSE);
            Button_SetCheck(m_hwndAppInputRadio, FALSE);
            ComboBox_Enable(m_hwndInputDevices, FALSE);
        } else if (m_vivox.IsUsingDefaultCommunicationAudioInputDevice()) {
            Button_SetCheck(m_hwndOsSystemInputRadio, FALSE);
            Button_SetCheck(m_hwndOsCommInputRadio, TRUE);
            Button_SetCheck(m_hwndAppInputRadio, FALSE);
            ComboBox_Enable(m_hwndInputDevices, FALSE);
        } else {
            Button_SetCheck(m_hwndOsSystemInputRadio, FALSE);
            Button_SetCheck(m_hwndOsCommInputRadio, FALSE);
            Button_SetCheck(m_hwndAppInputRadio, TRUE);
            ComboBox_Enable(m_hwndInputDevices, TRUE);
            int idx = ComboBox_FindStringExact(m_hwndInputDevices, 0, m_vivox.GetAudioInputDevice().GetAudioDeviceDisplayName().c_str());
            ComboBox_SetCurSel(m_hwndInputDevices, idx);
        }
    }
}

void JoinChannelApp::onDefaultSystemAudioInputDeviceChanged(const AudioDeviceId &deviceId)
{
    base::onDefaultSystemAudioInputDeviceChanged(deviceId);
    onAvailableAudioDevicesChanged();
}

void JoinChannelApp::onDefaultCommunicationAudioInputDeviceChanged(const AudioDeviceId &deviceId)
{
    base::onDefaultCommunicationAudioInputDeviceChanged(deviceId);
    onAvailableAudioDevicesChanged();
}

void JoinChannelApp::onSetAudioInputDeviceCompleted(const AudioDeviceId &deviceId)
{
    base::onSetAudioInputDeviceCompleted(deviceId);
    onAvailableAudioDevicesChanged();
}

void JoinChannelApp::onSetAudioInputDeviceFailed(const AudioDeviceId &deviceId, VCSStatus status)
{
    base::onSetAudioInputDeviceFailed(deviceId, status);
    onAvailableAudioDevicesChanged();
}

void JoinChannelApp::onDefaultSystemAudioOutputDeviceChanged(const AudioDeviceId &deviceId)
{
    base::onDefaultSystemAudioOutputDeviceChanged(deviceId);
    onAvailableAudioDevicesChanged();
}

void JoinChannelApp::onDefaultCommunicationAudioOutputDeviceChanged(const AudioDeviceId &deviceId)
{
    base::onDefaultCommunicationAudioOutputDeviceChanged(deviceId);
    onAvailableAudioDevicesChanged();
}

void JoinChannelApp::onSetAudioOutputDeviceCompleted(const AudioDeviceId &deviceId)
{
    base::onSetAudioOutputDeviceCompleted(deviceId);
    onAvailableAudioDevicesChanged();
}

void JoinChannelApp::onSetAudioOutputDeviceFailed(const AudioDeviceId &deviceId, VCSStatus status)
{
    base::onSetAudioOutputDeviceFailed(deviceId, status);
    onAvailableAudioDevicesChanged();
}

void JoinChannelApp::UpdateAudioTestButtons()
{
    if (m_vivox.AudioInputDeviceTestIsRecording()) {
        Button_SetText(m_hwndTestInputRecord, "Stop Recording");
    } else {
        Button_SetText(m_hwndTestInputRecord, "Record");
    }

    if (m_vivox.AudioInputDeviceTestIsPlayingBack()) {
        Button_SetText(m_hwndTestInputPlayback, "Stop Playback");
    } else {
        Button_SetText(m_hwndTestInputPlayback, "Playback");
    }

    if (m_vivox.AudioOutputDeviceTestIsRunning()) {
        Button_SetText(m_hwndTestOutput, "Stop Test");
    } else {
        Button_SetText(m_hwndTestOutput, "Test Output Device");
    }
    Button_Enable(m_hwndTestOutput, m_vivox.AudioOutputDeviceTestIsRunning() || (!m_vivox.AudioInputDeviceTestIsRecording() && !m_vivox.AudioInputDeviceTestIsPlayingBack()));
    Button_Enable(m_hwndTestInputRecord, m_vivox.AudioInputDeviceTestIsRecording() || (!m_vivox.AudioOutputDeviceTestIsRunning() && !m_vivox.AudioInputDeviceTestIsPlayingBack()));
    Button_Enable(
            m_hwndTestInputPlayback,
            m_vivox.AudioInputDeviceTestIsPlayingBack() ||
            (m_vivox.AudioInputDeviceTestHasAudioToPlayback() && !m_vivox.AudioInputDeviceTestIsRecording() && !m_vivox.AudioOutputDeviceTestIsRunning()));
}

void JoinChannelApp::onAudioInputDeviceTestPlaybackCompleted()
{
    base::onAudioInputDeviceTestPlaybackCompleted();
    UpdateAudioTestButtons();
}

INT_PTR CALLBACK JoinChannelApp::AudioDialog(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    (void)lParam;
    switch (message) {
        case WM_INITDIALOG:
        {
            RECT rc;
            GetWindowRect(hDlg, &rc);

            int xPos = (GetSystemMetrics(SM_CXSCREEN) - rc.right) / 2;
            int yPos = (GetSystemMetrics(SM_CYSCREEN) - rc.bottom) / 2;

            SetWindowPos(hDlg, 0, xPos, yPos, 0, 0, SWP_NOZORDER | SWP_NOSIZE);

            break;
        }
        case WM_HSCROLL:
            if (LOWORD(wParam) == TB_ENDTRACK) {
                if (lParam == (LPARAM)m_hwndInputVolume) {
                    m_vivox.SetMasterAudioInputDeviceVolume((int)InternalSendMessage(m_hwndInputVolume, TBM_GETPOS, 0, 0));
                } else if (lParam == (LPARAM)m_hwndOutputVolume) {
                    m_vivox.SetMasterAudioOutputDeviceVolume((int)InternalSendMessage(m_hwndOutputVolume, TBM_GETPOS, 0, 0));
                } else {
                    *(int *)0 = 1;
                }
            }
            break;
        case WM_COMMAND:
            if (HIWORD(wParam) == CBN_SELCHANGE) {
                switch (LOWORD(wParam)) {
                    case IDC_CHOOSE_INPUT:
                        m_vivox.SetAudioInputDevice(m_vivox.GetAvailableAudioInputDevices().at(ComboBox_GetCurSel(m_hwndInputDevices)));
                        break;
                    case IDC_CHOOSE_OUTPUT:
                        m_vivox.SetAudioOutputDevice(m_vivox.GetAvailableAudioOutputDevices().at(ComboBox_GetCurSel(m_hwndOutputDevices)));
                        break;
                }
            } else if (HIWORD(wParam) == BN_CLICKED) {
                switch (LOWORD(wParam)) {
                    case IDC_RADIO_OUTPUT_OS_SYSTEM:
                        m_vivox.UseDefaultSystemAudioOutputDevice();
                        break;
                    case IDC_RADIO_OUTPUT_OS_COMM:
                        m_vivox.UseDefaultCommunicationAudioOutputDevice();
                        break;
                    case IDC_RADIO_OUTPUT_CHOOSE:
                        // If the OS can't choose a device, neither can we (this is the case when there are no devices)
                        if (m_vivox.GetDefaultSystemAudioOutputDevice().IsValid()) {
                            m_vivox.SetAudioOutputDevice(m_vivox.GetDefaultSystemAudioOutputDevice());
                        }
                        this->onAvailableAudioDevicesChanged();
                        break;
                    case IDC_RADIO_INPUT_OS_SYSTEM:
                        m_vivox.UseDefaultSystemAudioInputDevice();
                        break;
                    case IDC_RADIO_INPUT_OS_COMM:
                        m_vivox.UseDefaultCommunicationAudioInputDevice();
                        break;
                    case IDC_RADIO_INPUT_CHOOSE:
                        // If the OS can't choose a device, neither can we (this is the case when there are no devices)
                        if (m_vivox.GetDefaultSystemAudioInputDevice().IsValid()) {
                            m_vivox.SetAudioInputDevice(m_vivox.GetDefaultSystemAudioInputDevice());
                        }
                        this->onAvailableAudioDevicesChanged();
                        break;
                    case IDC_TEST_OUTPUT:
                        if (!m_vivox.AudioOutputDeviceTestIsRunning()) {
                            m_vivox.StartAudioOutputDeviceTest("audio_output_test.wav");
                        } else {
                            m_vivox.StopAudioOutputDeviceTest();
                        }
                        UpdateAudioTestButtons();
                        break;
                    case IDC_RECORD:
                        if (!m_vivox.AudioInputDeviceTestIsRecording()) {
                            m_vivox.StartAudioInputDeviceTestRecord();
                        } else {
                            m_vivox.StopAudioInputDeviceTestRecord();
                        }
                        UpdateAudioTestButtons();
                        break;
                    case IDC_PLAY_RECORDING:
                        if (!m_vivox.AudioInputDeviceTestIsPlayingBack()) {
                            m_vivox.StartAudioInputDeviceTestPlayback();
                        } else {
                            m_vivox.StopAudioInputDeviceTestPlayback();
                        }
                        UpdateAudioTestButtons();
                        break;
                }
            }
            break;
        case WM_DESTROY:
            CloseWindow(hDlg);
            break;
        case WM_CLOSE:
            m_vivox.StopAudioOutputDeviceTest();
            Button_SetText(m_hwndTestOutput, "Test Output Device");
            m_vivox.StopAudioInputDeviceTestRecord();
            Button_SetText(m_hwndTestOutput, "Record");
            m_vivox.StopAudioInputDeviceTestPlayback();
            Button_SetText(m_hwndTestOutput, "Playback");
            ShowWindow(hDlg, 0);
            break;
    }
    return (INT_PTR)FALSE;
}

INT_PTR CALLBACK JoinChannelApp::MainDialog(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    UNREFERENCED_PARAMETER(hDlg);
    VCSStatus vcs;
    switch (message) {
        case WM_COMMAND:
            if (LOWORD(wParam) == IDCANCEL) {
                DestroyWindow(hDlg);
            } else if (wParam == ID_TOOLS_AUDIOSETUP) {
                ShowWindow(m_hwndAudioSetup, 1);
            } else if (HIWORD(wParam) == BN_CLICKED) {
                switch (LOWORD(wParam)) {
                    case IDC_TEAM1: // Ephemeral Channel
                    case IDC_TEAM2: // Echo Channel
                    case IDC_TEAM3: // 3d channel
                    case IDC_TEAM4: // No Channel
                    case IDC_RADIO1: // Players
                    case IDC_RADIO2:
                    case IDC_RADIO3:
                    case IDC_RADIO4:
                    case IDC_RADIO5:
                    case IDC_RADIO6:
                    case IDC_RADIO7:
                    case IDC_RADIO8:
                        // BEGINVIVOX: If a checkbox changes on the main dialog, then call my Update() function,
                        // which will then put me in the right channel with the right user.
                        vcs = Update();
                        if (vcs != 0) {
                            // We should never actually see this. If so, it means there's a programming error somewhere.
                            MessageBox(NULL, VivoxClientApi::GetErrorString(vcs), "Update() Error", MB_ICONERROR | MB_OK);
                        }
                        // ENDVIVOX
                        break;
                    case IDC_CHECK1:
                    case IDC_CHECK2:
                    case IDC_CHECK3:
                    case IDC_CHECK4:
                    case IDC_CHECK5:
                    case IDC_CHECK6:
                    case IDC_CHECK7:
                    case IDC_CHECK8:
                        MuteForMe(LOWORD(wParam));
                        break;
                    case IDC_BTNMM1:
                    case IDC_BTNMM2:
                    case IDC_BTNMM3:
                    case IDC_BTNMM4:
                    case IDC_BTNMM5:
                    case IDC_BTNMM6:
                    case IDC_BTNMM7:
                    case IDC_BTNMM8:
                        MuteForAll(LOWORD(wParam));
                        break;
                    case IDC_MUTEINPUT:
                        m_vivox.SetAudioInputDeviceMuted(Button_GetCheck(m_hwndMuteInput) == TRUE);
                        break;
                    case IDC_MUTEOUTPUT:
                        m_vivox.SetAudioOutputDeviceMuted(Button_GetCheck(m_hwndMuteOutput) == TRUE);
                        break;
                    case IDC_SET_POSITION:
                        SetPositionIn3dChannel();
                }
            }
            break;
        case WM_DESTROY:
            PostQuitMessage(0);
            break;
    }
    return (INT_PTR)FALSE;
}

LRESULT JoinChannelApp::InternalSendMessage(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) const
{
    if (IsWindow(hDlg)) {
        return SendMessage(hDlg, message, wParam, lParam);
    }
    return NULL;
}
