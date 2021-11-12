/* (c) Copyright 2007-2018 Mercer Road Corp. All rights reserved.
 * Mercer Road Corp. hereby grants you ("User"), under its copyright rights, the right to distribute,
 * reproduce and/or prepare derivative works of this software source (or binary) code ("Software")
 * solely to the extent expressly authorized in writing by Mercer Road Corp.  If you do not have a written
 * license from Mercer Road Corp., you have no right to use the Software except for internal testing and review.
 *
 * No other rights are granted and no other use is authorized. The availability of the Software does not provide
 * any license by implication, estoppel, or otherwise under any patent rights or other rights owned or
 * controlled by Mercer Road or others covering any use of the Software herein.
 *
 * USER AGREES THAT MERCER ROAD ASSUMES NO LIABILITY FOR ANY DAMAGES, WHETHER DIRECT OR OTHERWISE,
 * WHICH THE USER MAY SUFFER DUE TO USER'S USE OF THE SOFTWARE.  MERCER ROAD PROVIDES THE SOFTWARE "AS IS,"
 * MAKES NO EXPRESS OR IMPLIED REPRESENTATIONS OR WARRANTIES OF ANY TYPE, AND EXPRESSLY DISCLAIMS THE
 * WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT.
 * USER ACKNOWLEDGES THAT IT ASSUMES TOTAL RESPONSIBILITY AND RISK FOR USER'S USE OF THE SOFTWARE.
 *
 * Except for any written authorization, license or agreement from or with Mercer Road Corp.
 * the foregoing terms and conditions shall constitute the entire agreement between User and Mercer Road Corp.
 * with respect to the subject matter hereof and shall not be modified or superceded
 * without the express written authorization of Mercer Road Corp.
 *
 * Any copies or derivative works must include this and all other proprietary notices contained on or in the Software as received by the User.
 */
#include "vxplatform/vxcplatform.h"
  #include <windows.h>

#pragma comment(lib, "shlwapi")
#include <objbase.h>

#include <string>
#include <mutex>

namespace vxplatform {
static std::string string_vformat(const char *fmt, va_list ap)
{
    std::string result;
    char tmp = '\0';
    int size = 0;
    va_list ap2;
    va_copy(ap2, ap);
    size = vsnprintf(&tmp, 0, fmt, ap2);
    va_end(ap2);

    if (size < 0) {
        return result;
    }

    char *dest = new char[size + 1];

    if (NULL == dest) {
        return result;
    }

    size = vsnprintf(dest, size + 1, fmt, ap);

    if (size >= 0) {
        dest[size] = '\0';
        result.assign(dest, size);
    }

    delete[] dest;
    dest = NULL;
    return result;
}

/* static */
std::string string_format(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    std::string s = string_vformat(fmt, ap);
    va_end(ap);
    return s;
}

class InternalThreadArgs
{
    thread_start_function_t m_func;
    void *m_pArg;

public:
    InternalThreadArgs(thread_start_function_t pf, void *pArg)
    {
        m_func = pf;
        m_pArg = pArg;
    }
    os_error_t Run()
    {
        return m_func(m_pArg);
    }
};

DWORD WINAPI OnThreadStart(LPVOID pArg)
{
    HRESULT hr = CoInitialize(NULL);
    if (FAILED(hr)) {
        return hr;
    }

    InternalThreadArgs *args = (InternalThreadArgs *)pArg;
    InternalThreadArgs _args(*args);
    delete args;
    os_error_t err = _args.Run();


    return err;
}

os_error_t create_thread(thread_start_function_t pf, void *pArg, os_thread_handle *phThread, os_thread_id *pTid, size_t stacksize, int priority)
{
    // MUSTDO: Implement stacksize and priority
    (void)stacksize;
    (void)priority;
    DWORD tid;
    HANDLE h = CreateThread(NULL, 0, OnThreadStart, new InternalThreadArgs(pf, pArg), 0, &tid);
    DWORD dwError = 0;
    if (h == NULL) {
        dwError = GetLastError();
    } else {
        *phThread = (os_thread_handle)h;
        *pTid = (os_thread_id)tid;
    }
    return (os_error_t)dwError;
}

os_error_t create_thread(thread_start_function_t pf, void *pArg, os_thread_handle *phThread)
{
    os_thread_id tid;
    return create_thread(pf, pArg, phThread, &tid, 0, 0);
}

os_error_t create_thread(thread_start_function_t pf, void *pArg, os_thread_handle *phThread, size_t stacksize, int priority)
{
    os_thread_id tid;
    return create_thread(pf, pArg, phThread, &tid, stacksize, priority);
}

os_error_t delete_thread(os_thread_handle handle)
{
    if (!CloseHandle(handle)) {
        return GetLastError();
    }
    return 0;
}
os_error_t create_event(os_event_handle *pHandle)
{
    HANDLE h = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (h == NULL) {
        return GetLastError();
    }
    *pHandle = (os_event_handle)h;
    return 0;
}
os_error_t set_event(os_event_handle handle)
{
    if (!SetEvent((HANDLE)handle)) {
        return GetLastError();
    }
    return 0;
}
os_error_t reset_event(os_event_handle handle)
{
    if (!ResetEvent((HANDLE)handle)) {
        return GetLastError();
    }
    return 0;
}
os_error_t join_thread(os_thread_handle handle, int timeout)
{
    os_error_t err = wait_event(handle, timeout);
    if (err == 0) {
        CloseHandle(handle);
    }
    // if OS_E_TIMEOUT we should not close handle, it is already invalid
    return err;
}

void thread_sleep(unsigned long long ms)
{
    Sleep(static_cast<DWORD>(ms));
}

os_error_t wait_event(os_event_handle handle, int timeout)
{
    __try {
        if (timeout < 0) {
            timeout = INFINITE;
        }
        DWORD dwStat = WaitForSingleObject((HANDLE)handle, (DWORD)timeout);
        if (dwStat == WAIT_OBJECT_0) {
            return 0;
        }
        if (dwStat == WAIT_TIMEOUT) {
            return OS_E_TIMEOUT;
        }
        return dwStat;
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }
}

const DWORD MS_VC_EXCEPTION = 0x406D1388;

#pragma pack(push,8)
typedef struct tagTHREADNAME_INFO
{
    DWORD dwType;     // Must be 0x1000.
    LPCSTR szName;     // Pointer to name (in user addr space).
    DWORD dwThreadID;     // Thread ID (-1=caller thread).
    DWORD dwFlags;     // Reserved for future use, must be zero.
} THREADNAME_INFO;
#pragma pack(pop)

void set_thread_name_internal(os_thread_id dwThreadID, os_thread_handle,  const std::string &threadName)
{
    THREADNAME_INFO info;
    info.dwType = 0x1000;
    info.szName = threadName.c_str();
    info.dwThreadID = dwThreadID;
    info.dwFlags = 0;

    __try {
        RaiseException(MS_VC_EXCEPTION, 0, sizeof(info) / sizeof(ULONG_PTR), (ULONG_PTR *)&info);
    } __except(EXCEPTION_EXECUTE_HANDLER) {
    }
}

void set_thread_name(os_thread_id dwThreadID, const std::string &threadName)
{
    std::string name("Vivox: ");
    name.append(threadName);
    set_thread_name_internal(dwThreadID, NULL, name);
}

void set_thread_name(const std::string &threadName)
{
    set_thread_name(get_current_thread_id(), threadName);
}

os_error_t delete_event(os_event_handle handle)
{
    if (!CloseHandle((HANDLE)handle)) {
        return GetLastError();
    }
    return 0;
}

os_error_t close_thread_handle(os_thread_handle handle)
{
    BOOL b = CloseHandle((HANDLE)handle);
    if (!b) {
        return GetLastError();
    }
    return 0;
}

os_thread_id get_current_thread_id()
{
    return GetCurrentThreadId();
}

double get_millisecond_tick_counter()
{
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);     // This is in 100 nanosecond increments, which is 1/10 of a microsecond.
    ULARGE_INTEGER ul;
    ul.u.LowPart = ft.dwLowDateTime;
    ul.u.HighPart = ft.dwHighDateTime;
    return (double)(ul.QuadPart / 10000);
}

Lock::Lock()
{
    m_pImpl = new std::mutex;
}
Lock::~Lock(void)
{
    delete (std::mutex *)m_pImpl;
}
void Lock::Take()
{
    ((std::mutex *)m_pImpl)->lock();
}
void Lock::Release()
{
    ((std::mutex *)m_pImpl)->unlock();
}

Locker::Locker(Lock *pLock)
{
    m_pLock = pLock;
    m_pLock->Take();
}
Locker::~Locker(void)
{
    m_pLock->Release();
    m_pLock = NULL;
}
}
