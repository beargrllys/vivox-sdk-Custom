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

// NOTE: always include system headers first
#include <locale>
#include <deque>
#include <iostream>
#include <fstream>
#include <iterator>
#include <algorithm>
#include <cstdlib>
#include <ctime>

#include <stdio.h>

#include <codecvt>
#define VIVOX_USE_SDK_BROWSER 1

#include "SDKSampleApp.h"
#include "getopt.h"
#include "vxplatform/vxcplatformmain.h"
#include "vxplatform/vxcplatform.h"
#include <VxcRequests.h>

#include <windows.h>
#include "TestUDPFrameCallbacks.h"
#include <io.h>    // for _setmode
#include <fcntl.h> // for _O_U16TEXT

#include <string.h>
#include <stdarg.h>
// define this to test accidental file close(by calling close() instead of socketclose() on sockets) on PS/3 platform
#undef DEBUG_FIOS

#if VIVOX_USE_SDK_BROWSER
#include "SDKBrowserWin.h"
#endif

#include "ParanoidAllocator.h"

#define snprintf _snprintf
#define sscanf sscanf_s

using namespace std;

#define SYS_APP_HOME ""

#ifndef SCE_OK
#define SCE_OK 0
#endif

static void GenericSleepMilliSeconds(int ms)
{
    Sleep((DWORD)ms);
}

// for running on systems without pty drivers.
static std::string handleSpecialCharacters(const std::string &s)
{
    std::deque<char> items;

    for (size_t i = 0; i < s.size(); ++i)
    {
        if (s.at(i) == 8)
        {
            // control h;
            if (!items.empty())
            {
                items.pop_back();
            }
        }
        else if (s.at(i) == 3)
        {
            // control c
            return "";
        }
        else
        {
            items.push_back(s.at(i));
        }
    }

    std::string tmp;
    for (size_t i = 0; i < items.size(); ++i)
    {
        tmp.push_back(items.at(i));
    }
    return tmp;
}

const char *GetLogLevelName(vx_log_level level)
{
    switch (level)
    {
    case log_error:
        return "ERROR";
    case log_warning:
        return "WARNING";
    case log_info:
        return "INFO";
    case log_debug:
        return "DEBUG";
    case log_trace:
        return "TRACE";
    case log_all:
        return "ALL";
    case log_none:
        return "NONE";
    }
    return "???";
}

static std::string s_logFileName;
static FILE *s_pLogFile = NULL;

void (*g_sdk_sampleapp_log_callback)(const char *level, const char *source, const char *message) = NULL;
bool (*g_sdk_sampleapp_getline_callback)(std::string &line) = NULL;
// *INDENT-OFF*
int(ATTRIBUTE_FORMAT(printf, 1, 0) * g_printf_wrapper)(const char *format, const va_list &vl) = NULL;
// *INDENT-ON*
void (*g_sdk_sampleapp_exit)() = NULL;

static bool g_exit = false;

void cbExit()
{
    g_exit = true;
}

bool OpenLogFile()
{
    if (s_pLogFile || s_logFileName.empty())
    {
        return true;
    }
    s_pLogFile = fopen(s_logFileName.c_str(), "w");
    return s_pLogFile != NULL;
}

void OnLog(void *callback_handle, vx_log_level level, const char *source, const char *message)
{
    (void)callback_handle;

    OpenLogFile();
    if (s_pLogFile)
    {
        time_t currentTime;
        currentTime = time(NULL);
        struct tm tmCurrent;
        struct tm *ptm = nullptr;
#if defined(__STDC_LIB_EXT1__)
        ptm = gmtime_s(&currentTime, &tmCurrent);
#else
        // return errno_t = 0 indicates success
        ptm = gmtime_s(&tmCurrent, &currentTime) ? nullptr : &tmCurrent;
#endif
        char aBuf[128];
        if (ptm)
        {
            strftime(aBuf, sizeof(aBuf), "%Y-%m-%d %H:%M:%S", ptm);
        }
        else
        {
            aBuf[0] = 0;
        }
        fprintf(s_pLogFile, "%s %-7s : %s : %s\n", aBuf, GetLogLevelName(level), source, message);
        fflush(s_pLogFile);
    }
    if (g_sdk_sampleapp_log_callback)
    {
        g_sdk_sampleapp_log_callback(GetLogLevelName(level), source, message);
    }
}

void CloseLog()
{
    if (s_pLogFile)
    {
        fclose(s_pLogFile);
        s_pLogFile = NULL;
    }
}

void OnSdkMessageAvailable(void *callbackHandle)
{
    SDKSampleApp *app = reinterpret_cast<SDKSampleApp *>(callbackHandle);
    app->Wakeup();
}

void OnBeforeReceivedAudioMixed(void *callback_handle, const char *session_group_handle, const char *initial_target_uri, vx_before_recv_audio_mixed_participant_data_t *participants_data, size_t num_participants)
{
    SDKSampleApp *app = reinterpret_cast<SDKSampleApp *>(callback_handle);
    app->OnBeforeReceivedAudioMixed(session_group_handle, initial_target_uri, participants_data, num_participants);
}

int usage()
{
    SDKSampleApp::con_print("Usage: SDKSampleApp OPTION...\n");
    SDKSampleApp::con_print("Demonstrates a base implementation of functionality using the Vivox Communications API.\n");
    SDKSampleApp::con_print("\n");
    SDKSampleApp::con_print("Mandatory arguments to long options are mandatory for short options too.\n");
    SDKSampleApp::con_print("  -s, --server=URL      Vivox API URL, e.g. https://vdx5.www.vivox.com/api2\n");
    SDKSampleApp::con_print("  -r, --realm=STRING    Vivox domain, e.g. vdx5.vivox.com\n");
    SDKSampleApp::con_print("  -i, --issuer=STRING   token signing issuer\n");
    SDKSampleApp::con_print("  -k, --key=STRING      token signing key\n");
    SDKSampleApp::con_print("  -h, --help            display this help text and exit\n");
    SDKSampleApp::con_print("  -v, --version         display version information and exit\n");
    SDKSampleApp::con_print("  -m, --multitenant     use issuer prefix when generating random user and channel IDs\n");
    SDKSampleApp::con_print("  -e, --execute=FILE    execute script from FILE\n");
    SDKSampleApp::con_print("  -c, --defcodecs=MASK  default codecs MASK (1 - PCMU, 2 - Siren7, 4 - siren14, 16 - Opus8, 32 - Opus40, 64 - Opus57, 128 - Opus72\n");
    SDKSampleApp::con_print("  -n, --neverrtp=MS     \"never RTP timeout\" in milliseconds. Specify 0 or negative value to turn this guard off. Prelogin can override this.\n");
    SDKSampleApp::con_print("  -x, --lostrtp=MS      \"lost RTP timeout\" in milliseconds. Specify 0 or negative value to turn this guard off. Prelogin can override this.\n");
    SDKSampleApp::con_print("  -t, --tcpport=PORTNUM tcp control port, default 9876\n");
#if VIVOX_USE_SDK_BROWSER
    SDKSampleApp::con_print("  -b, --browser         Opens SDK Browser window to visualize SDK objects and behavior\n");
#endif
    SDKSampleApp::con_print("  -p, --affinity=MASK   processor affinity mask (platform specific)\n");
    SDKSampleApp::con_print("  -a, --pooledalloc     use SDK internal pooled alloc instead of ParanoidAllocator\n");
    SDKSampleApp::con_print("\n");
    SDKSampleApp::con_print("Options for server, realm, issuer, and key are required but may be given in any order.\n");

    return 1;
}

bool parse_log_level(const char *log_level_name, vx_log_level &log_level)
{
    if (0 == stricmp(log_level_name, "none"))
    {
        log_level = log_none;
    }
    else if (0 == stricmp(log_level_name, "error"))
    {
        log_level = log_error;
    }
    else if (0 == stricmp(log_level_name, "warning"))
    {
        log_level = log_warning;
    }
    else if (0 == stricmp(log_level_name, "info"))
    {
        log_level = log_info;
    }
    else if (0 == stricmp(log_level_name, "debug"))
    {
        log_level = log_debug;
    }
    else if (0 == stricmp(log_level_name, "trace"))
    {
        log_level = log_trace;
    }
    else if (0 == stricmp(log_level_name, "all"))
    {
        log_level = log_all;
    }
    else
    {
        log_level = log_error; // default
        return false;
    }
    return true;
}

static inline void trim(string &s)
{
    if (s.empty())
    {
        return;
    }

    // http://en.cppreference.com/w/cpp/string/byte/isspace
    const string whitespace = " \f\n\r\t\v";

    // trim left
    size_t pos = s.find_first_not_of(whitespace);
    if (string::npos != pos)
    {
        s = s.substr(pos);
    }

    // trim right
    pos = s.find_last_not_of(whitespace) + 1;
    if (string::npos != pos)
    {
        s = s.substr(0, pos);
    }
}

static std::string GetCodecsInfoString(unsigned int codecs_mask)
{
    std::stringstream ss;

    struct CodecName
    {
        unsigned int flag;
        const char *name;
    } codec_names[] = {
        {VIVOX_VANI_PCMU, "PCMU"},
        {VIVOX_VANI_SIREN7, "Siren7"},
        {VIVOX_VANI_SIREN14, "Siren14"},
        {VIVOX_VANI_OPUS8, "Opus8"},
        {VIVOX_VANI_OPUS40, "Opus40"},
        {VIVOX_VANI_OPUS57, "Opus57"},
        {VIVOX_VANI_OPUS72, "Opus72"},
    };

    ss << vxplatform::string_format("Default codecs mask %u (0x%02x)", codecs_mask, codecs_mask);
    if (0 != codecs_mask)
    {
        ss << ": ";
        const char *separator = "";
        for (unsigned int codec = 1; codec != 0; codec <<= 1)
        {
            if (codec & codecs_mask)
            {
                int i;
                for (i = 0; i < sizeof(codec_names) / sizeof(codec_names[0]); i++)
                {
                    if (codec_names[i].flag == codec)
                    {
                        break;
                    }
                }
                if (i < sizeof(codec_names) / sizeof(codec_names[0]))
                {
                    ss << separator << codec_names[i].name;
                }
                else
                {
                    ss << vxplatform::string_format("%s0x%02x", separator, codec);
                }
                separator = ", ";
            }
        }
    }

    return ss.str();
}

static vx_sdk_config_t MakeConfig(const SDKSampleApp &app, unsigned int default_codecs_mask, vx_log_level log_level = log_error)
{
    vx_sdk_config_t config;
    vx_get_default_config3(&config, sizeof(config));

    config.pf_logging_callback = OnLog;
    config.pf_sdk_message_callback = OnSdkMessageAvailable;
    config.callback_handle = const_cast<void *>(reinterpret_cast<const void *>(&app)); // TODO: Should we make callback_handle "const void *"?
    config.pf_on_audio_unit_before_recv_audio_mixed = OnBeforeReceivedAudioMixed;

#if VIVOX_SDK_HAS_ADVANCED_AUDIO_LEVELS
    config.enable_advanced_auto_levels = 1;
#endif

#ifdef VX_HAS_UDP_CALLBACKS
    vx_test_set_udp_frame_callbacks(&config);
#endif
    config.initial_log_level = log_level;
#ifdef THIS_APP_UNIQUE_3_LETTERS_USER_AGENT_ID_STRING
    memcpy(config.app_id, THIS_APP_UNIQUE_3_LETTERS_USER_AGENT_ID_STRING, sizeof(config.app_id));
#endif
    config.default_codecs_mask = default_codecs_mask;
    if (app.GetNeverRtpTimeout() >= 0)
    {
        config.never_rtp_timeout_ms = app.GetNeverRtpTimeout();
    }
    if (app.GetLostRtpTimeout() >= 0)
    {
        config.lost_rtp_timeout_ms = app.GetLostRtpTimeout();
    }

    if (config.never_rtp_timeout_ms <= 0)
    {
        app.con_print("Never RTP timeout DISABLED. Prelogin can override this.\n");
    }
    else
    {
        app.con_print("Never RTP timeout: %d ms. Prelogin can override this.\n", config.never_rtp_timeout_ms);
    }
    if (config.lost_rtp_timeout_ms <= 0)
    {
        app.con_print("Lost RTP timeout DISABLED. Prelogin can override this.\n");
    }
    else
    {
        app.con_print("Lost RTP timeout: %d ms. Prelogin can override this.\n", config.lost_rtp_timeout_ms);
    }

    if (app.GetProcessorAffinityMask() != 0)
    {
        config.processor_affinity_mask = app.GetProcessorAffinityMask();
    }

    return config;
}

int main_proc(int argc, char *argv[], string cmdList[])
{
    UINT consoleOutputCP = GetConsoleOutputCP();
    if (consoleOutputCP != CP_UTF8)
    {
        SetConsoleOutputCP(CP_UTF8);
    }
    UINT consoleCP = GetConsoleCP();
    if (consoleCP != CP_UTF8)
    {
        SetConsoleCP(CP_UTF8);
    }
    _setmode(_fileno(stdin), _O_U16TEXT);
    _setmode(_fileno(stdout), _O_U16TEXT);

    ParanoidAllocator::GetInstance().SetOutputFunction(&SDKSampleApp::con_print);

    SDKSampleApp app(g_printf_wrapper, (g_sdk_sampleapp_exit == NULL) ? cbExit : g_sdk_sampleapp_exit);
    vx_log_level log_level = log_error;
    bool is_multitenant = false;
    bool is_multitenant_set = false;
    bool is_log_level_specified = false;
    std::string script;
    unsigned int default_codecs_mask = vx_get_default_codecs_mask();
#if VIVOX_USE_SDK_BROWSER
    bool show_sdk_browser = false;
#endif
    int never_rtp_timeout_ms = 0;
    int lost_rtp_timeout_ms = 0;
    unsigned short tcp_control_port = 0;
    long long processor_affinity_mask = 0;
    bool useParanoidAllocator = true;

    app.con_print("%s", SDKSampleApp::getVersionAndCopyrightText().c_str());

    while (1)
    {
        static struct option long_options[] =
        {
            /* options are distinguished by index. */
            {"server", REQUIRED_ARG, 0, 's'},
            {"realm", REQUIRED_ARG, 0, 'r'},
            {"issuer", REQUIRED_ARG, 0, 'i'},
            {"key", REQUIRED_ARG, 0, 'k'},
            {"ignore", NO_ARG, 0, 'g'},
            {"help", NO_ARG, 0, 'h'},
            {"version", NO_ARG, 0, 'v'},
            {"loglevel", REQUIRED_ARG, 0, 'l'},
            {"logfile", REQUIRED_ARG, 0, 'f'},
            {"multitenant", NO_ARG, 0, 'm'},
            {"execute", REQUIRED_ARG, 0, 'e'},
            {"defcodecs", REQUIRED_ARG, 0, 'c'},
            {"neverrtp", REQUIRED_ARG, 0, 'n'},
            {"lostrtp", REQUIRED_ARG, 0, 'x'},
            {"tcpport", REQUIRED_ARG, 0, 't'},
#if VIVOX_USE_SDK_BROWSER
            {"browser", NO_ARG, 0, 'b'},
#endif
            {"affinity", REQUIRED_ARG, 0, 'p'},
            {"pooledalloc", NO_ARG, 0, 'a'},
            {0, 0, 0, 0}
        };
        /* getopt_long stores the option index here. */
        int option_index = 0;

        int c = getopt_long(
            argc,
            argv,
            "s:r:i:k:hvl:f:me:c:n:x:t:"
#if VIVOX_USE_SDK_BROWSER
            "b"
#endif
            "p:a",
            long_options,
            &option_index);

        // Detect the end of the options.
        if (c == -1)
        {
            break;
        }

        switch (c)
        {
        case 'v':
        {
            // already output above
            return 0;
        }
        case 's':
        {
            app.SetServer(optarg);
            break;
        }
        case 'r':
        {
            app.SetRealm(optarg);
            break;
        }
        case 'i':
        {
            app.SetIssuer(optarg);
            break;
        }
        case 'k':
        {
            app.SetKey(optarg);
            break;
        }
        case 'l':
        {
            if (!parse_log_level(optarg, log_level))
            {
                app.con_print("Error: unsupported loglevel: %s\nSupported log levels are none, error, warning, info, debug, trace, all\n", optarg);
                return 1;
            }
            is_log_level_specified = true;
            break;
        }
        case 'f':
        {
            // app.con_print("option -f with value `%s'\n", optarg);
            s_logFileName = optarg;
            break;
        }
        case 'm':
        {
            is_multitenant = true;
            is_multitenant_set = true;
            break;
        }
        case 'e':
        {
            script = optarg;
            break;
        }
        case 'c':
        {
            if (1 != sscanf(optarg, "%u", &default_codecs_mask))
            {
                app.con_print("Error: unsigned integer expected for --defcodecs option, got %s\n", optarg);
                return 1;
            }
            break;
        }
#if VIVOX_USE_SDK_BROWSER
        case 'b':
        {
            show_sdk_browser = true;
            break;
        }
#endif
        case 'n':
        {
            if (1 != sscanf(optarg, "%d", &never_rtp_timeout_ms))
            {
                app.con_print("Error: integer expected for --neverrtp option, got %s\n", optarg);
                return 1;
            }
            app.SetNeverRtpTimeout(never_rtp_timeout_ms);
            break;
        }
        case 'x':
        {
            if (1 != sscanf(optarg, "%d", &lost_rtp_timeout_ms))
            {
                app.con_print("Error: integer expected for --lostrtp option, got %s\n", optarg);
                return 1;
            }
            app.SetLostRtpTimeout(lost_rtp_timeout_ms);
            break;
        }
        case 't':
        {
            if (1 != sscanf(optarg, "%hu", &tcp_control_port))
            {
                app.con_print("Error: unsigned short expected for --tcpport option, got %s\n", optarg);
                return 1;
            }
            app.SetTcpControlPort(tcp_control_port);
            break;
        }
        case 'p':
        {
            if (1 != sscanf(optarg, "%lld", &processor_affinity_mask))
            {
                app.con_print("Error: unsigned integer expected for --affinity option, got %s\n", optarg);
                return 1;
            }
            else
            {
                app.con_print("Using processor affinity mask %lld (0x%llx)\n", processor_affinity_mask, processor_affinity_mask);
                app.SetProcessorAffinityMask(processor_affinity_mask);
            }
            break;
        }
        case 'a':
        {
            useParanoidAllocator = false;
            break;
        }
        case 'h':
        default:
        {
            return usage();
        }
        }
    }

    if (app.GetServer().empty() && app.GetRealm().empty() && app.GetIssuer().empty() && app.GetKey().empty() && !is_multitenant_set)
    {
        app.SetServer("https://vdx5.www.vivox.com/api2/");
        app.SetRealm("vdx5.vivox.com");
        app.SetIssuer("vivoxsampleapps-ssa-dev");
        app.SetKey("Jqqh9k7mgB2wVtSun09HW8ZP4nBK2tff");
        is_multitenant = true;
    }

    if (app.GetServer().empty() || app.GetRealm().empty() || app.GetIssuer().empty() || app.GetKey().empty())
    {
        return usage();
    }

    app.SetServerMultitenant(is_multitenant);

    int status;
    app.con_print("Type 'help' for a quickstart guide and full list of available commands.\n");

    if (is_log_level_specified)
    {
        if (s_logFileName.empty())
        {
            app.con_print("Error: --loglevel command line option has no effect without '--logfile filename.log'\n");
            return 1;
        }
        else if (!OpenLogFile())
        {
            app.con_print("Error: unable to create/open log file for writing: \"%s\"'\n", s_logFileName.c_str());
            return 1;
        }
        app.con_print("Log level: %s\nLog file: %s\n", GetLogLevelName(log_level), s_logFileName.c_str());
    }
    else
    {
        app.con_print("Logging is off.\n");
    }

    app.con_print("[Using server %s, realm %s and issuer %s, multi-tenancy mode %s]\n", app.GetServer().c_str(), app.GetRealm().c_str(), app.GetIssuer().c_str(), is_multitenant ? "ON" : "OFF");

    vx_sdk_config_t config = MakeConfig(app, default_codecs_mask, log_level);

    if (useParanoidAllocator)
    {
        ParanoidAllocator::ConfigureHooksIfRequired(config);
    }

    std::string codecsInfo = GetCodecsInfoString(config.default_codecs_mask);

    status = vx_initialize3(&config, sizeof(config));
    if (status != 0)
    {
        cerr << "Error " << status << " returned from vx_initialize3()" << endl;
        return 1;
    }

    app.con_print("%s\n", codecsInfo.c_str());

#if VIVOX_USE_SDK_BROWSER
    SDKBrowserWin *pBrowser = NULL;
    if (show_sdk_browser)
    {
        pBrowser = new SDKBrowserWin;
        app.SetMessageObserver(pBrowser);
    }
#endif

    vxplatform::os_event_handle semaphore;
    vxplatform::create_event(&semaphore);
    bool isMicAuthorized = true;

    auto cbStartWithMicAuthorization = [&isMicAuthorized, &semaphore, &app](bool isAuthorized)
    {
        isMicAuthorized = isAuthorized;
        if (isAuthorized)
        {
            // Start the event loop on another thread
            app.Start();
        }
        else
        {
            cerr << "No permission to access the microphone." << endl;
            app.Shutdown();
        }
        vxplatform::set_event(semaphore);
    };
    cbStartWithMicAuthorization(true);

    vxplatform::wait_event(semaphore);
    vxplatform::delete_event(semaphore);
    semaphore = NULL;

    std::ifstream fin; // used to read from a file.
    if (!script.empty())
    {
        fin.open(script.c_str());
        if (!fin.is_open())
        {
            cerr << "Error opening input file: " << script << endl;
            g_exit = true;
        }
    }

    // Begin main input loop
    const char *prompt = "[SDKSampleApp]: ";
    setbuf(stdout, NULL);

    int cmdIdx = 0;

    while (!g_exit)
    {
        std::string tmpLine;
        if (g_sdk_sampleapp_getline_callback)
        {
            if (!g_sdk_sampleapp_getline_callback(tmpLine))
            {
                break;
            }
        }
        else if (!fin.is_open())
        {
            app.con_print("%s", prompt);
            cout.flush();

            vxplatform::create_event(app.ListenerEventLink());
            vxplatform::wait_event(*(app.ListenerEventLink()));
            if (cmdIdx < 3) {
                tmpLine = cmdList[cmdIdx];
                app.con_print("%s", tmpLine);
                cmdIdx++;
            }
            else {
                std::wstring line;
                std::getline(std::wcin, line);
                std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t> converterX;
                tmpLine = converterX.to_bytes(line);
            }

            vxplatform::delete_event(*(app.ListenerEventLink()));

            tmpLine = handleSpecialCharacters(tmpLine);
            if (std::wcin.eof())
            {
                break;
            }
        }
        else
        {
            tmpLine.clear();
            std::getline(fin, tmpLine);

            app.con_print("%s\n", tmpLine.c_str());
            if (fin.eof())
            {
                fin.close();
            }
        }

        trim(tmpLine);
        if (tmpLine.empty() || tmpLine[0] == '#')
        {
            continue;
        }

        // Get the user input and determine the desired action
        std::vector<std::string> cmd = SDKSampleApp::splitCmdLine(tmpLine);

        if (cmd.empty())
        {
            continue;
        }

        if (stricmp(cmd[0].c_str(), "file") == 0)
        {
            std::string inputfilename;
            if (cmd.size() < 2)
            {
                cerr << "Error opening input file, no file name given" << endl;

                continue;
            }
            inputfilename = SYS_APP_HOME + cmd[1];
            if (fin.is_open())
            {
                fin.close(); // close any open file, breaks nesting
            }
            fin.clear();
            fin.open(inputfilename.c_str());
            if (!fin.is_open())
            {
                cerr << "Error opening input file: " << inputfilename << endl;
            }

            continue;
            // for diagnostic purposes only
        }
        else if (cmd[0] == "sleep" || cmd[0] == "sleepms")
        {
            if (cmd.size() == 2)
            {
                int ms = atoi(cmd.at(1).c_str());
                if (cmd[0] == "sleep")
                {
                    ms *= 1000;
                }
                app.con_print("Sleeping %d ms.\n", ms);
                GenericSleepMilliSeconds(ms);
            }
            else
            {
                app.con_print("usage: sleep <number> of seconds to sleep\n");
            }

            continue;
            // for diagnostic purposes only
        }
        else if (cmd[0] == "echo")
        {
            std::stringstream ss;
            if (cmd.size() > 1)
            {
                for (size_t i = 1; i < cmd.size(); i++)
                {
                    ss << cmd[i] << " ";
                }
            }
            app.con_print("%s\n", ss.str().c_str());

            continue;
        }
        else if (cmd[0] == "pause")
        {
            // wait for a return
            std::wstring line;
            std::getline(std::wcin, line);

            continue;
        }

        app.Lock();
        if (!app.ProcessCommand(cmd))
        {
            app.Unlock();
            break; // 'quit' signals break
        }
        app.Unlock();
    }

#if VIVOX_USE_SDK_BROWSER
    app.SetMessageObserver(NULL);
    if (pBrowser)
    {
        delete pBrowser;
        pBrowser = NULL;
    }
#endif

    // Stop the event loop
    app.Stop();

    app.con_print("Starting shutdown...\n");

    app.con_print("Calling vx_uninitialize()...");
    fflush(stdout);
    vx_uninitialize();
    app.con_print("Done.\n");
    cout.flush();
    CloseLog();

    app.con_print("Shutdown complete.\n");

    if (consoleOutputCP != CP_UTF8)
    {
        SetConsoleOutputCP(consoleOutputCP);
    }
    if (consoleCP != CP_UTF8)
    {
        SetConsoleCP(consoleCP);
    }

    return 0;
}

void random_name(string *name)
{
    int randTemp;
    int numRange_MIN = 48, numRange_MAX = 57;
    int lAphRange_MIN = 65, lAphRange_MAX = 90;
    int sAphRange_MIN = 97, sAphRange_MAX = 122;

    string output;

    srand((unsigned int)time(NULL));

    for (int i = 0; i < 10; i++)
    {
        randTemp = rand() % 3;
        switch (randTemp)
        {
        case 0:
            randTemp = (rand() % 10) + numRange_MIN;
            break;
        case 1:
            randTemp = (rand() % 26) + lAphRange_MIN;
            break;
        case 2:
            randTemp = (rand() % 26) + sAphRange_MIN;
            break;
        }
        (*name).append(1, randTemp);
    }
    (*name).append(1, '.');
}

#if defined(VX_CALL_MAIN_FROM_UI)
void run_ext_main(const std::string &cmd)
{
    std::vector<std::string> string_args;

    if (cmd.size() > 0)
    {
        string_args.clear();
        size_t start = 0;
        size_t stop = 0;
        for (size_t i = 0; i <= cmd.size(); i++)
        {
            if (i < cmd.size())
            {
                if (cmd[i] == L'\"')
                {
                    i++;
                    for (; i < cmd.size(); i++)
                    {
                        if (cmd[i] == L'\"')
                        {
                            break;
                        }
                    }
                }
                if (i >= cmd.size())
                {
                    break;
                }
                if (cmd[i] == L'\'')
                {
                    i++;
                    for (; i < cmd.size(); i++)
                    {
                        if (cmd[i] == L'\'')
                        {
                            break;
                        }
                    }
                }
                if (i >= cmd.size())
                {
                    break;
                }
            }
            if (i == cmd.size() || cmd[i] == ' ')
            {
                stop = i;
                if (stop - start > 1)
                {
                    std::string p = cmd.substr(start, stop - start);
                    string_args.push_back(p);
                }
                start = i + 1;
            }
        }
    }

    std::vector<std::string> tmp(string_args);
    std::vector<char *> args;
    args.reserve(string_args.size() + 1);
    args.resize(0);
    std::transform(
        std::begin(tmp),
        std::end(tmp),
        std::back_inserter(args),
        [](std::string &s)
        {
            return &s[0];
        });
    args.push_back(nullptr);

    main_proc((int)args.size() - 1, args.data());
}
#else
int main(int argc, char *argv[])
{
    string cmdList[3];
    cmdList[0] = "conn";
    string name = "\0";
    random_name(&name);
    cmdList[1].append("login -u .jisang2336-te04-dev.");
    cmdList[1].append(name);
    cmdList[2] = "addsession -c sip:confctl-g-jisang2336-te04-dev.CLPA@mt1s.vivox.com";

    return main_proc(argc, argv, cmdList);
}
#endif
