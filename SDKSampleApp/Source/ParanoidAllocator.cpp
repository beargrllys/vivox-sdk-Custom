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

#include <assert.h>
#include <stdio.h>
#include <cstring>
#include <cstdarg>
#include <sstream>

#include "ParanoidAllocator.h"
#if PARANOID_ALLOCATOR_BACKTRACE
#       include <windows.h>
#           pragma warning(push)
#           pragma warning(disable: 4091)
#           include <dbghelp.h>
#           pragma warning(pop)
#       define TRACE_MAX_STACK_FRAMES 1024
#       define TRACE_MAX_FUNCTION_NAME_LENGTH 1024
#endif

#define __STDC_FORMAT_MACROS 1
#include <inttypes.h>
#if defined(_WIN64) || defined(__LP64__)
#   define PRIADDR "0x%016" PRIxPTR
#else
#   define PRIADDR "0x%08" PRIxPTR
#endif


#define IS_POWER_OF_2(x) ((x) != 0 && ((x) & ((x) - 1)) == 0)


int printStdErr(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    int ret = vfprintf(stderr, fmt, args);
    va_end(args);
    return ret;
}

struct SymHelpers
{
#if PARANOID_ALLOCATOR_BACKTRACE && defined(_WIN32) && !defined(_XBOX_ONE) && !defined(_GAMING_XBOX) && !defined(_GAMING_DESKTOP)

private:
// VX_IMAGEAPI has to be defined cause original IMAGEAPI includes DECLSPEC_IMPORT
#define VX_IMAGEAPI __stdcall
    typedef BOOL (VX_IMAGEAPI *PSYMFROMADDR_FUNC)(
            HANDLE hProcess,
            DWORD64 Address,
            PDWORD64 Displacement,
            PSYMBOL_INFO Symbol);
    PSYMFROMADDR_FUNC m_pSymFromAddr;

    typedef BOOL (VX_IMAGEAPI *PSYMGETLINEFROMADDR64_FUNC)(
            HANDLE hProcess,
            DWORD64 qwAddr,
            PDWORD pdwDisplacement,
            PIMAGEHLP_LINE64 Line64);
    PSYMGETLINEFROMADDR64_FUNC m_pSymGetLineFromAddr64;

    typedef BOOL (VX_IMAGEAPI *PSYMINITIALIZE_FUNC)(
            HANDLE hProcess,
            PCSTR UserSearchPath,
            BOOL fInvadeProcess
            );
    PSYMINITIALIZE_FUNC m_pSymInitialize;

    typedef BOOL (VX_IMAGEAPI *PSYMCLEANUP_FUNC)(HANDLE hProcess);
    PSYMCLEANUP_FUNC m_pSymCleanup;

    typedef WORD (NTAPI *PRTLCAPTURESTACKBACKTRACE_FUNC)(
            _In_ DWORD FramesToSkip,
            _In_ DWORD FramesToCapture,
            _Out_writes_to_ (FramesToCapture, return ) PVOID *BackTrace,
            _Out_opt_ PDWORD BackTraceHash);

    PRTLCAPTURESTACKBACKTRACE_FUNC m_pRtlCaptureStackBackTrace;

#undef VX_IMAGEAPI

    HMODULE m_hDbgHelp;
    HMODULE m_hKernel32;

private:
    void printLastError(const std::string &description) const
    {
        LPVOID buffer = nullptr;
        DWORD lastError = GetLastError();

        FormatMessageA(
                FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                NULL,
                lastError,
                MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                (LPSTR)&buffer,
                0,
                NULL);
        printStdErr("Error: %s - (%lu) %s\n", description.c_str(), lastError, buffer);
        LocalFree(buffer);
    }

    BOOL Init()
    {
        if (!m_hDbgHelp) {
            m_hDbgHelp = LoadLibrary(TEXT("dbghelp.dll"));
            if (!m_hDbgHelp) {
                printLastError("unable to load DBGHELP.DLL");
                UnInit();
                return FALSE;
            }

#define GET_PROC_ADDRESS(lib, type, proc)                          \
    m_p##proc = reinterpret_cast<type>(GetProcAddress(lib,#proc)); \
    if (!m_p##proc) {                                              \
        printLastError("unable to get proc address");              \
        UnInit();                                                  \
        assert(FALSE);                                             \
        return FALSE;                                              \
    }

            GET_PROC_ADDRESS(m_hDbgHelp, PSYMINITIALIZE_FUNC, SymInitialize)
            GET_PROC_ADDRESS(m_hDbgHelp, PSYMFROMADDR_FUNC, SymFromAddr)
            GET_PROC_ADDRESS(m_hDbgHelp, PSYMGETLINEFROMADDR64_FUNC, SymGetLineFromAddr64)
            GET_PROC_ADDRESS(m_hDbgHelp, PSYMCLEANUP_FUNC, SymCleanup)

            HANDLE handle = GetCurrentProcess();
            if (!handle) {
                printLastError("unable to call GetCurrentProcess()");
                UnInit();
                assert(FALSE);
                return FALSE;
            }

            if (!(*m_pSymInitialize)(handle, NULL, TRUE)) {
                printLastError("unable to call SymInitialize()");
                UnInit();
                assert(FALSE);
                return FALSE;
            }
        }

        if (!m_hKernel32) {
            m_hKernel32 = LoadLibrary(TEXT("kernel32.dll"));
            if (!m_hKernel32) {
                printLastError("unable to load KERNEL32.DLL");
                UnInit();
                assert(FALSE);
                return FALSE;
            }

            GET_PROC_ADDRESS(m_hKernel32, PRTLCAPTURESTACKBACKTRACE_FUNC, RtlCaptureStackBackTrace)

#undef GET_PROC_ADDRESS
        }

        return TRUE;
    }

    void UnInit()
    {
        if (m_pSymCleanup) {
            (*m_pSymCleanup)(GetCurrentProcess());
        }
        m_pSymInitialize = NULL;
        m_pSymFromAddr = NULL;
        m_pSymGetLineFromAddr64 = NULL;
        m_pSymCleanup = NULL;
        if (m_hDbgHelp) {
            FreeLibrary(m_hDbgHelp);
            m_hDbgHelp = NULL;
        }

        m_pRtlCaptureStackBackTrace = NULL;
        if (m_hKernel32) {
            FreeLibrary(m_hKernel32);
            m_hKernel32 = NULL;
        }
    }

public:
    SymHelpers() :
        m_pSymFromAddr(NULL),
        m_pSymGetLineFromAddr64(NULL),
        m_pSymInitialize(NULL),
        m_pSymCleanup(NULL),
        m_hDbgHelp(NULL),
        m_hKernel32(NULL)
    {
        BOOL bResult = Init();
        (void)bResult;
    }

    ~SymHelpers()
    {
        UnInit();
    }

    BOOL SymFromAddr(
            HANDLE hProcess,
            DWORD64 Address,
            PDWORD64 Displacement,
            PSYMBOL_INFO Symbol)
    {
        if (!m_hDbgHelp) {
            return FALSE;
        }
        return (*m_pSymFromAddr)(hProcess, Address, Displacement, Symbol);
    }

    BOOL SymGetLineFromAddr64(
            HANDLE hProcess,
            DWORD64 qwAddr,
            PDWORD pdwDisplacement,
            PIMAGEHLP_LINE64 Line64)
    {
        if (!m_hDbgHelp) {
            return FALSE;
        }
        return (*m_pSymGetLineFromAddr64)(hProcess, qwAddr, pdwDisplacement, Line64);
    }

    WORD CaptureStackBackTrace(
            DWORD FramesToSkip,
            DWORD FramesToCapture,
            PVOID *BackTrace,
            PDWORD BackTraceHash)
    {
        if (!m_hKernel32) {
            return 0;
        }
        return (*m_pRtlCaptureStackBackTrace)(FramesToSkip, FramesToCapture, BackTrace, BackTraceHash);
    }
#endif
};

// Uncomment to always use this allocator
#define USE_PARANOID_ALLOCATOR 1
// #define PARANOID_ALLOCATOR_DO_CHECK_ALL 1

// Uncomment if you want to always have assert() on leak
#define DMN_ASSERT_ON_LEAK 1

ParanoidAllocator ParanoidAllocator::s_Instance;

ParanoidAllocator::Stats::Stats()
{
    m_lAllocs = 0;
    m_lFrees = 0;
    m_lAlignedAllocs = 0;
    m_lAlignedFrees = 0;
    m_lReallocs = 0;
    m_lCallocs = 0;
    m_lCurrentlyAllocated = 0;
    m_lPeakAllocated = 0;
}

ParanoidAllocator::Stats::Stats(const ParanoidAllocator::Stats &stats)
{
    *this = stats;
}

ParanoidAllocator::Stats &ParanoidAllocator::Stats::operator=(const ParanoidAllocator::Stats &stats)
{
    m_mapAllocsBySize = stats.m_mapAllocsBySize;
    m_lAllocs = stats.m_lAllocs;
    m_lFrees = stats.m_lFrees;
    m_lAlignedAllocs = stats.m_lAlignedAllocs;
    m_lAlignedFrees = stats.m_lAlignedFrees;
    m_lReallocs = stats.m_lReallocs;
    m_lCallocs = stats.m_lCallocs;
    m_lCurrentlyAllocated = stats.m_lCurrentlyAllocated;
    m_lPeakAllocated = stats.m_lPeakAllocated;
    return *this;
}

ParanoidAllocator::Stats ParanoidAllocator::Stats::operator-(const ParanoidAllocator::Stats &stats)
{
    Stats result;

    result.m_lAllocs = m_lAllocs - stats.m_lAllocs;
    result.m_lFrees = m_lFrees - stats.m_lFrees;
    result.m_lAlignedAllocs = m_lAlignedAllocs - stats.m_lAlignedAllocs;
    result.m_lAlignedFrees = m_lAlignedFrees - stats.m_lAlignedFrees;
    result.m_lReallocs = m_lReallocs - stats.m_lReallocs;
    result.m_lCallocs = m_lCallocs - stats.m_lCallocs;
    result.m_lCurrentlyAllocated = m_lCurrentlyAllocated - stats.m_lCurrentlyAllocated;
    result.m_lPeakAllocated = m_lPeakAllocated - stats.m_lPeakAllocated;

    // m_mapAllocsBySize
    // m_mapAllocsBySize - stats.m_mapAllocsBySize
    for (std::map<size_t, long>::const_iterator it = m_mapAllocsBySize.begin(); it != m_mapAllocsBySize.end(); it++) {
        size_t size = it->first;
        std::map<size_t, long>::const_iterator itPrev = stats.m_mapAllocsBySize.find(size);
        if (itPrev == stats.m_mapAllocsBySize.end()) {
            result.m_mapAllocsBySize[size] = it->second;
        } else if (it->second != itPrev->second) {
            result.m_mapAllocsBySize[size] = it->second - itPrev->second;
        }
    }
    // stats.m_mapAllocsBySize - m_mapAllocsBySize
    for (std::map<size_t, long>::const_iterator it = stats.m_mapAllocsBySize.begin(); it != stats.m_mapAllocsBySize.end(); it++) {
        if (m_mapAllocsBySize.find(it->first) == m_mapAllocsBySize.end()) {
            result.m_mapAllocsBySize[it->first] = -it->second;
        }
    }
    return result;
}

std::string ParanoidAllocator::Stats::toString()
{
    std::stringstream ss;
    ss << "m_lAllocs = " << m_lAllocs << "\n";
    ss << "m_lFrees = " << m_lFrees << "\n";
    ss << "m_lAlignedAllocs = " << m_lAlignedAllocs << "\n";
    ss << "m_lAlignedFrees = " << m_lAlignedFrees << "\n";
    ss << "m_lReallocs = " << m_lReallocs << "\n";
    ss << "m_lCallocs = " << m_lCallocs << "\n";
    ss << "m_lCurrentlyAllocated = " << m_lCurrentlyAllocated << "\n";
    ss << "m_lPeakAllocated = " << m_lPeakAllocated << "\n";
    return ss.str();
}


ParanoidAllocator::ParanoidAllocator() :
    m_pStdOut(&printf),
    m_pStdErr(&printStdErr),
    m_pSymHelpers(new SymHelpers())
{
}

ParanoidAllocator::~ParanoidAllocator()
{
    std::list<void *> blocks;
    std::list<void *> blocks_aligned;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (BlockListMap::const_iterator it = m_mapFreeBlockListsBySize.begin(); it != m_mapFreeBlockListsBySize.end(); it++) {
            BlockList *pBlockList = it->second;
            while (!pBlockList->empty()) {
                free(pBlockList->front());
                pBlockList->pop_front();
            }
            delete pBlockList;
        }
        m_mapFreeBlockListsBySize.clear();

        if (!m_mapAllocatedBlocks.empty()) {
            int hidden = 0;
            m_pStdOut("%u block(s) are still allocated:\n", m_mapAllocatedBlocks.size());
            for (BlockSizeMap::const_iterator it = m_mapAllocatedBlocks.begin(); it != m_mapAllocatedBlocks.end(); it++) {
                unsigned char *p = PABlockFromUser(it->first);
                if (((PAHeader *)p)->flags & hdrFlag_Hidden) {
                    hidden++;
                } else {
                    m_pStdOut("  " PRIADDR " (%u bytes)\n", (uintptr_t)it->first, it->second);
                    PrintStack(p + c_nNoMansLand, c_nStackTraceSize);
                }
                blocks.push_back(it->first);
            }
            m_pStdOut("  %d block(s) shown and there is %d hidden block(s)\n", (int)m_mapAllocatedBlocks.size() - hidden, hidden);
        }
        if (!m_mapAllocatedAlignedBlocks.empty()) {
            int hidden = 0;
            m_pStdOut("%u aligned block(s) are still allocated:\n", m_mapAllocatedAlignedBlocks.size());
            for (BlockSizeMap::const_iterator it = m_mapAllocatedAlignedBlocks.begin(); it != m_mapAllocatedAlignedBlocks.end(); it++) {
                unsigned char *p = PABlockFromUser(it->first);
                if (((PAHeader *)p)->flags & hdrFlag_Hidden) {
                    hidden++;
                } else {
                    PrintStack(p + c_nNoMansLand, c_nStackTraceSize);
                    m_pStdOut("  " PRIADDR " (%u bytes)\n", (uintptr_t)it->first, it->second);
                }
                blocks_aligned.push_back(it->first);
            }
            m_pStdOut("  %d block(s) shown and there is %d hidden block(s)\n", (int)m_mapAllocatedAlignedBlocks.size() - hidden, hidden);
        }
        if (m_mapAllocatedBlocks.empty() && m_mapAllocatedAlignedBlocks.empty()) {
            m_pStdOut("ParanoidAllocator::%s - clean exit (peak mem use %ld bytes, %ld allocs, %ld reallocs, %ld callocs, %ld memaligns)\n", __FUNCTION__, m_stats.m_lPeakAllocated, m_stats.m_lAllocs, m_stats.m_lReallocs, m_stats.m_lCallocs, m_stats.m_lAlignedAllocs);
        } else {
            m_pStdOut("ParanoidAllocator::%s - error!\n", __FUNCTION__);
        }
#ifdef DMN_ASSERT_ON_LEAK
        assert(m_mapAllocatedBlocks.empty());
        assert(m_mapAllocatedAlignedBlocks.empty());
#endif
    }
    while (!blocks.empty()) {
        paranoidFree(blocks.front());
        blocks.pop_front();
    }
    while (!blocks_aligned.empty()) {
        paranoidFreeAligned(blocks_aligned.front());
        blocks_aligned.pop_front();
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    assert(m_mapAllocatedBlocks.empty());
    assert(m_mapAllocatedAlignedBlocks.empty());

    delete m_pSymHelpers;
}

ParanoidAllocator::OutputFunction ParanoidAllocator::SetOutputFunction(OutputFunction pfOutput)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    OutputFunction pfTmp = m_pStdOut;
    m_pStdOut = pfOutput;
    return pfTmp;
}

ParanoidAllocator::BlockList *ParanoidAllocator::GetBlockList(size_t bytes, bool bCreate)
{
    BlockList *pBlockList = NULL;
    BlockListMap::const_iterator it = m_mapFreeBlockListsBySize.find(bytes);
    if (it == m_mapFreeBlockListsBySize.end()) {
        if (bCreate) {
            pBlockList = new BlockList;
            m_mapFreeBlockListsBySize[bytes] = pBlockList;
        }
    } else {
        pBlockList = it->second;
    }
    assert(pBlockList || !bCreate);
    return pBlockList;
}

#   define update_allocated_by_allocators(x)

#ifdef MEMLEAK_DUMP
#include <execinfo.h>
#include <array>
#include <thread>
#include <mutex>
#include <cassert>
#endif

void *ParanoidAllocator::paranoidAllocImpl(ParanoidAllocator::BlockSizeMap &map, size_t bytes, bool bUpdateStats)
{
    assert(bytes != 0);
    unsigned char *p = NULL;
    // Find list of free blocks of this size
    BlockList *pBlockList = GetBlockList(bytes, false);
    if (pBlockList && !pBlockList->empty()) {
        p = (unsigned char *)pBlockList->front();
        pBlockList->pop_front();
        assert(p != NULL);
        // Check that free block is really intact
        for (size_t n = 0; n < PABlockSizeFromUser(bytes); n++) {
            assert(p[n] == c_byteFiller);
        }
    }
    if (!p) {
        // Allocate a new block
        p = (unsigned char *)malloc(PABlockSizeFromUser(bytes));
        if (p == NULL) {
            return p;
        }
    }
    assert(p);
    // update stats
    if (bUpdateStats) {
        if (&map == &m_mapAllocatedBlocks) {
            ++m_stats.m_lAllocs;
        } else {
            ++m_stats.m_lAlignedAllocs;
        }
    }
    ++m_stats.m_mapAllocsBySize[bytes];
    m_stats.m_lCurrentlyAllocated += static_cast<long>(bytes);
    if (m_stats.m_lCurrentlyAllocated > m_stats.m_lPeakAllocated) {
        m_stats.m_lPeakAllocated = m_stats.m_lCurrentlyAllocated;
    }
    update_allocated_by_allocators(bytes);
    // Initialize the new block
    PAHeader *header = (PAHeader *)p;
    header->magic = c_uiMagic;
    header->size = bytes;
    header->flags = 0;
    memset(p + sizeof(PAHeader), c_byteFiller, c_nNoMansLand - sizeof(PAHeader));
    memset(p + c_nNoMansLand + c_nStackTraceSize, c_byteFiller, c_nNoMansLand);
    memset(p + c_nNoMansLand + c_nStackTraceSize + c_nNoMansLand + bytes, c_byteFiller, c_nNoMansLand);
    void *pResult = UserBlockFromPA(p);
    DumpStack(p + c_nNoMansLand, c_nStackTraceSize);
    // Add to the allocated blocks map
    assert(m_mapAllocatedBlocks.find(pResult) == m_mapAllocatedBlocks.end());
    assert(m_mapAllocatedAlignedBlocks.find(pResult) == m_mapAllocatedAlignedBlocks.end());
    map[pResult] = bytes;
    return pResult;
}

void ParanoidAllocator::HideUnfreedBlocks()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    { // BlockSizeMap m_mapAllocatedBlocks;
        BlockSizeMap::iterator i = m_mapAllocatedBlocks.begin();
        for (; i != m_mapAllocatedBlocks.end(); i++) {
            unsigned char *p = PABlockFromUser((unsigned char *)i->first);
            PAHeader *header = (PAHeader *)p;
            if (!(header->flags & hdrFlag_Hidden)) {
                header->flags |= hdrFlag_Hidden;
                m_stats.m_lHidden++;
            }
        }
    }
    { // BlockSizeMap m_mapAllocatedAlignedBlocks;
        BlockSizeMap::iterator i = m_mapAllocatedAlignedBlocks.begin();
        for (; i != m_mapAllocatedAlignedBlocks.end(); i++) {
            unsigned char *p = PABlockFromUser((unsigned char *)i->first);
            PAHeader *header = (PAHeader *)p;
            if (!(header->flags & hdrFlag_Hidden)) {
                header->flags |= hdrFlag_Hidden;
                m_stats.m_lHidden++;
            }
        }
    }
}



void ParanoidAllocator::DumpStack(void *place, size_t size)
{
#if PARANOID_ALLOCATOR_BACKTRACE
    if ((place == NULL) || (size < sizeof(int))) {
        return;
    }
    int *frames = (int *)place;
    void **dump = (void **)(frames + 1);
    size -= sizeof(int);

    *frames = (int)m_pSymHelpers->CaptureStackBackTrace(0, (int)(size / sizeof(void *)), dump, NULL);
#endif
}

void *ParanoidAllocator::paranoidAlloc(size_t bytes)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return paranoidAllocImpl(m_mapAllocatedBlocks, bytes, true);
}

void ParanoidAllocator::CheckAllMemBlockIntegrity()
{
    { // BlockSizeMap m_mapAllocatedBlocks;
        BlockSizeMap::iterator i = m_mapAllocatedBlocks.begin();
        for (; i != m_mapAllocatedBlocks.end(); i++) {
            unsigned char *p = PABlockFromUser((unsigned char *)i->first);
            CheckMemBlockIntegrity(p);
        }
    }

    { // BlockSizeMap m_mapAllocatedAlignedBlocks;
        BlockSizeMap::iterator i = m_mapAllocatedAlignedBlocks.begin();
        for (; i != m_mapAllocatedAlignedBlocks.end(); i++) {
            unsigned char *p = PABlockFromUser((unsigned char *)i->first);
            CheckMemBlockIntegrity(p);
        }
    }

    { // BlockListMap m_mapFreeBlockListsBySize;
        BlockListMap::iterator i = m_mapFreeBlockListsBySize.begin();
        for (; i != m_mapFreeBlockListsBySize.end(); i++) {
            BlockList *lst = i->second;
            BlockList::iterator j = lst->begin();
            for (; j != lst->end(); j++) {
                unsigned char *p = (unsigned char *)*j;
                CheckMemBlockIntegrity(p, i->first);
            }
        }
    }
}

void ParanoidAllocator::CheckMemBlockIntegrity(void *pMem, size_t blockSize)
{
    unsigned char *p = (unsigned char *)pMem;
    PAHeader *header = (PAHeader *)p;
    assert(header->magic == c_uiMagic);
    size_t bytes = (0 == blockSize) ?
        header->size :
        blockSize;
    // Check the block's "no man's land"
    for (size_t n = sizeof(PAHeader); n < c_nNoMansLand; n++) {
        assert(p[n] == c_byteFiller);
    }
    for (size_t n = 0; n < c_nNoMansLand; n++) {
        assert(p[c_nNoMansLand + c_nStackTraceSize + n] == c_byteFiller);
    }
    for (size_t n = 0; n < c_nNoMansLand; n++) {
        assert(p[c_nNoMansLand + c_nStackTraceSize + c_nNoMansLand + bytes + n] == c_byteFiller);
        (void)bytes; // unused variable in release
    }
}

void ParanoidAllocator::paranoidFreeImpl(ParanoidAllocator::BlockSizeMap &map, void *pUserBlock, bool bUpdateStats)
{
#ifdef PARANOID_ALLOCATOR_DO_CHECK_ALL
    CheckAllMemBlockIntegrity();
#endif
    BlockSizeMap::const_iterator it = map.find(pUserBlock);
    if (it == map.end()) {
        PrintCurrentStack("free called for memory which wasn't allocated:");
        assert(it != map.end()); // trying to free normal block as aligned, or vise versa? Or passing the pointer which was never allocated?
        return;
    }
    // assert(m_mapAllocatedAlignedBlocks.find(pUserBlock) == m_mapAllocatedAlignedBlocks.end());
    unsigned char *p = PABlockFromUser(pUserBlock);
    PAHeader *header = (PAHeader *)p;
    if (header->flags & hdrFlag_Hidden) {
        --m_stats.m_lHidden;
    }
    size_t bytes = header->size;
    // Check the block size
    assert(header->magic == c_uiMagic);
    assert(bytes == it->second);
    CheckMemBlockIntegrity(p);
    // update stats
    if (bUpdateStats) {
        if (&map == &m_mapAllocatedBlocks) {
            ++m_stats.m_lFrees;
        } else {
            ++m_stats.m_lAlignedFrees;
        }
    }
    m_stats.m_lCurrentlyAllocated -= static_cast<long>(bytes);
    update_allocated_by_allocators(-(int)bytes);
    // Remove the block from allocated blocks map
    map.erase(it);
    // Add the block to the free list
    BlockList *pList = GetBlockList(bytes, true);
    pList->push_back(p);
    // Entire block is "no man's land" now
    memset(p, c_byteFiller, PABlockSizeFromUser(bytes));
}

void ParanoidAllocator::paranoidFree(void *pUserBlock)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    paranoidFreeImpl(m_mapAllocatedBlocks, pUserBlock, true);
}

void *ParanoidAllocator::paranoidRealloc(void *pUserBlock, size_t bytes)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    bool bUpdateAllocStats = (pUserBlock == NULL);
    void *pNewUserBlock = paranoidAllocImpl(m_mapAllocatedBlocks, bytes, bUpdateAllocStats);
    if (!pNewUserBlock) {
        return NULL;
    }
    if (pUserBlock) {
        BlockSizeMap::const_iterator it = m_mapAllocatedBlocks.find(pUserBlock);
        assert(it != m_mapAllocatedBlocks.end());
        (void)it; // unused variable in release
        // assert(m_mapAllocatedAlignedBlocks.find(pUserBlock) == m_mapAllocatedAlignedBlocks.end());
        unsigned char *p = PABlockFromUser(pUserBlock);
        PAHeader *header = (PAHeader *)p;
        size_t oldSize = header->size;
        // Check the block size
        assert(header->magic == c_uiMagic);
        assert(oldSize == it->second);
        CheckMemBlockIntegrity(p);
        memcpy(pNewUserBlock, pUserBlock, (bytes > oldSize) ? oldSize : bytes);
        paranoidFreeImpl(m_mapAllocatedBlocks, pUserBlock, false);
    }
    // update stats
    ++m_stats.m_lReallocs;
    return pNewUserBlock;
}

void ParanoidAllocator::paranoidFreeAligned(void *p)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    paranoidFreeImpl(m_mapAllocatedAlignedBlocks, p, true);
}

void *ParanoidAllocator::paranoidCalloc(size_t count, size_t size)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    void *p = paranoidAllocImpl(m_mapAllocatedBlocks, size * count, false);
    memset(p, 0, size * count);
    // update stats
    ++m_stats.m_lCallocs;
    return p;
}

void *ParanoidAllocator::paranoidMemalign(size_t alignment, size_t bytes)
{
    (void)alignment;
    std::lock_guard<std::mutex> lock(m_mutex);
    // Ignore alignment for now
    return paranoidAllocImpl(m_mapAllocatedAlignedBlocks, bytes, true);
}

void ParanoidAllocator::AdHocSelfTest()
{
    char *pTmp;
    // Some ad-hoc tests to make sure Paranoid Allocator works

    // 0. State Diff
    Stats stats1;
    GetStats(stats1);
    pTmp = (char *)paranoidAlloc(10);
    assert(pTmp != NULL);
    pTmp = (char *)paranoidRealloc(pTmp, 20);
    assert(pTmp != NULL);
    paranoidFree(pTmp);
    pTmp = NULL;
    pTmp = (char *)paranoidCalloc(2, 15);
    assert(pTmp != NULL);
    for (int i = 0; i < 30; i++) {
        assert(pTmp[i] == 0);
    }
    paranoidFree(pTmp);
    pTmp = NULL;
    pTmp = (char *)paranoidMemalign(128, 40);
    assert(pTmp != NULL);
    paranoidFreeAligned(pTmp);
    pTmp = NULL;
    Stats stats2;
    GetStats(stats2);
    Stats statsDiff = stats2 - stats1;

    assert(statsDiff.m_lAllocs == 1);
    assert(statsDiff.m_lFrees == 2);
    assert(statsDiff.m_lReallocs == 1);
    assert(statsDiff.m_lCallocs == 1);
    assert(statsDiff.m_lAlignedAllocs == 1);
    assert(statsDiff.m_lAlignedFrees == 1);
    assert(statsDiff.m_lCurrentlyAllocated == 0);
    // can fail if something was free before AdHocSelfTest() is called
    // assert(statsDiff.m_lPeakAllocated == 40);
    assert(statsDiff.m_mapAllocsBySize.size() == 4);
    for (size_t n = 10; n <= 40; n += 10) {
        assert(statsDiff.m_mapAllocsBySize.find(n) != statsDiff.m_mapAllocsBySize.end());
        assert(1 == statsDiff.m_mapAllocsBySize[n]);
    }

    // Leave only one test of interest uncommented
    /*
    // 1. Small underflow
    pTmp = (char*)paranoidAlloc(10);
    assert(pTmp != NULL);
    memset(pTmp-5, 'U', 15);
    paranoidFree(pTmp); // should assert
    pTmp = NULL;

    // 2. Small overflow
    pTmp = (char*)paranoidAlloc(10);
    assert(pTmp != NULL);
    memset(pTmp, 'O', 15);
    paranoidFree(pTmp); // should assert
    pTmp = NULL;

    // 3. Writing to deallocated memory
    pTmp = (char*)paranoidAlloc(10);
    assert(pTmp != NULL);
    paranoidFree(pTmp);
    memset(pTmp, 'D', 10);
    pTmp = (char*)paranoidAlloc(10); // should assert
    pTmp = NULL;

    // 4. Alloc as aligned, free as normal
    pTmp = (char*)paranoidMemalign(128, 10);
    assert(pTmp != NULL);
    paranoidFree(pTmp); // should assert
    pTmp = NULL;

    // 5. Alloc as normal, free as aligned
    pTmp = (char*)paranoidAlloc(10);
    assert(pTmp != NULL);
    paranoidFreeAligned(pTmp); // should assert
    pTmp = NULL;

    // 6. Leak
    pTmp = (char *)paranoidAlloc(10); // should be the only leak on exit
    assert(pTmp != NULL);
    m_pfOutput("%s: expecting leaked block at " PRIADDR " on exit\n", __FUNCTION__, (uintptr_t)pTmp);
    */
}

void ParanoidAllocator::GetStats(ParanoidAllocator::Stats &stats)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    stats = m_stats;
}

void ParanoidAllocator::Dump()
{
    if (!m_mapAllocatedBlocks.empty()) {
        m_pStdOut("%u block(s) allocated:\n", m_mapAllocatedBlocks.size());
        int hidden = 0;
        for (BlockSizeMap::const_iterator it = m_mapAllocatedBlocks.begin(); it != m_mapAllocatedBlocks.end(); it++) {
            unsigned char *p = PABlockFromUser(it->first);
            PAHeader *header = (PAHeader *)p;
            if (!(header->flags & hdrFlag_Hidden)) {
                m_pStdOut("  " PRIADDR " (%u bytes)\n", (uintptr_t)it->first, it->second);
                PrintStack(p + c_nNoMansLand, c_nStackTraceSize);
            } else {
                hidden++;
            }
        }
        m_pStdOut("  %d block(s) shown and there is %d hidden block(s)\n", (int)m_mapAllocatedBlocks.size() - hidden, hidden);
    }
    if (!m_mapAllocatedAlignedBlocks.empty()) {
        m_pStdOut("%u aligned block(s) allocated:\n", m_mapAllocatedAlignedBlocks.size());
        int hidden = 0;
        for (BlockSizeMap::const_iterator it = m_mapAllocatedAlignedBlocks.begin(); it != m_mapAllocatedAlignedBlocks.end(); it++) {
            unsigned char *p = PABlockFromUser(it->first);
            PAHeader *header = (PAHeader *)p;
            if (!(header->flags & hdrFlag_Hidden)) {
                m_pStdOut("  " PRIADDR " (%u bytes)\n", (uintptr_t)it->first, it->second);
                PrintStack(p + c_nNoMansLand, c_nStackTraceSize);
            } else {
                hidden++;
            }
        }
        m_pStdOut("  %d block(s) shown and there is %d hidden block(s)\n", (int)m_mapAllocatedAlignedBlocks.size() - hidden, hidden);
    }
}



void ParanoidAllocator::PrintStack(void *place, size_t size)
{
#if PARANOID_ALLOCATOR_BACKTRACE
    if ((place == NULL) || (size < sizeof(int))) {
        return;
    }
    int *frames = (int *)place;
    void **dump = (void **)(frames + 1);
    m_pStdErr("======================== Leak ============================\n");
    m_pStdErr("%d frames:\n", *frames);

    HANDLE process = GetCurrentProcess();

    char buffer[sizeof(SYMBOL_INFO) + (TRACE_MAX_FUNCTION_NAME_LENGTH - 1) * sizeof(TCHAR)];
    SYMBOL_INFO *symbol = reinterpret_cast<SYMBOL_INFO *>(buffer);
    symbol->MaxNameLen = TRACE_MAX_FUNCTION_NAME_LENGTH;
    symbol->SizeOfStruct = sizeof(SYMBOL_INFO);

    for (int i = 0; i < *frames; i++) {
        DWORD64 address = (DWORD64)(*dump);
        DWORD64 symbolDisplacement = 0;
        if (!m_pSymHelpers->SymFromAddr(process, address, &symbolDisplacement, symbol)) {
            m_pStdErr(
                    "\tSymFromAddr returned error code %lu for address " PRIADDR ".\n",
                    (unsigned long)GetLastError(),
                    (uintptr_t)*dump);
        } else {
            DWORD lineDisplacement = 0;
            IMAGEHLP_LINE64 line = { 0 };
            line.SizeOfStruct = sizeof(IMAGEHLP_LINE64);

            if (m_pSymHelpers->SymGetLineFromAddr64(process, address, &lineDisplacement, &line)) {
                m_pStdErr(
                        "\tat %s+0x%lx in %s: line: %lu+0x%lx: address: " PRIADDR "\n",
                        symbol->Name,
                        (unsigned long)symbolDisplacement,
                        line.FileName,
                        (unsigned long)line.LineNumber,
                        (unsigned long)lineDisplacement,
                        (uintptr_t)symbol->Address);
            } else {
                m_pStdErr(
                        "\tSymGetLineFromAddr64 returned error code %lu.\n"
                        "\tat %s+0x%lx, address " PRIADDR ".\n",
                        (unsigned long)GetLastError(),
                        symbol->Name,
                        (unsigned long)symbolDisplacement,
                        (uintptr_t)symbol->Address);
            }
        }
        dump++;
    }

    m_pStdErr("==========================================================\n");
#else
    (void)place;
    (void)size;
#endif
}

void ParanoidAllocator::PrintCurrentStack(const char *pszMessage)
{
#if PARANOID_ALLOCATOR_BACKTRACE
    if (pszMessage) {
        m_pStdErr("%s\n", pszMessage);
    }
    char aStackBacktrace[c_nStackTraceSize];
    DumpStack(aStackBacktrace, sizeof(aStackBacktrace));
    PrintStack(aStackBacktrace, sizeof(aStackBacktrace));
#else
    (void)pszMessage;
#endif
}

void *ParanoidAllocator::s_malloc(size_t size)
{
    return ParanoidAllocator::GetInstance().paranoidAlloc(size);
}

void ParanoidAllocator::s_free(void *p)
{
    ParanoidAllocator::GetInstance().paranoidFree(p);
}

void *ParanoidAllocator::s_realloc(void *p, size_t size)
{
    return ParanoidAllocator::GetInstance().paranoidRealloc(p, size);
}

void ParanoidAllocator::s_freeAligned(void *p)
{
    ParanoidAllocator::GetInstance().paranoidFreeAligned(p);
}

void *ParanoidAllocator::s_calloc(size_t count, size_t size)
{
    return ParanoidAllocator::GetInstance().paranoidCalloc(count, size);
}

void *ParanoidAllocator::s_memalign(size_t alignment, size_t size)
{
    assert(IS_POWER_OF_2(alignment));
    return ParanoidAllocator::GetInstance().paranoidMemalign(alignment, size);
}

bool ParanoidAllocator::CheckIfMustBeUsed()
{
#ifdef USE_PARANOID_ALLOCATOR
    return true;
#else
    return NULL != getenv("VIVOX_USE_PARANOID_ALLOCATOR");
#endif
}

void ParanoidAllocator::ConfigureHooks(vx_sdk_config_t &config)
{
    config.pf_malloc_func = &s_malloc;
    config.pf_calloc_func = &s_calloc;
    config.pf_realloc_func = &s_realloc;
    config.pf_free_func = &s_free;
    config.pf_malloc_aligned_func = &s_memalign;
    config.pf_free_aligned_func = &s_freeAligned;
}

void ParanoidAllocator::ConfigureHooksIfRequired(vx_sdk_config_t &config)
{
    if (CheckIfMustBeUsed()) {
        ConfigureHooks(config);
    }
}

bool ParanoidAllocator::AreThereAllocatedBlocks()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    size_t count = m_mapAllocatedBlocks.size();
    count += m_mapAllocatedAlignedBlocks.size();
    count -= m_stats.m_lHidden;
    /*
    m_pfOutput( "m_mapAllocatedBlocks = %d, m_mapAllocatedAlignedBlocks = %d, m_lHidden = %d\n"
        , (int)m_mapAllocatedBlocks.size()
        , (int)m_mapAllocatedAlignedBlocks.size()
        , (int)m_stats.m_lHidden
    );
    */
    return count ? true : false;
}
