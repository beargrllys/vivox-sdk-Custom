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
#pragma once

#include <list>
#include <map>
#include <mutex>
#include <vector>
#include <stdint.h>
#include "VxcTypes.h"


#define PARANOID_ALLOCATOR_BACKTRACE 1



struct SymHelpers;
class ParanoidAllocator
{
private:
    enum {
        hdrFlag_Hidden = 0x00000001
    };

    typedef struct PAHeader {
        uint32_t magic;
        uint32_t flags;
        size_t size;
    } PAHeader;

public:
    ~ParanoidAllocator();
    static ParanoidAllocator &GetInstance() { return s_Instance; }

    void *paranoidAlloc(size_t size);
    void  paranoidFree(void *p);
    void *paranoidRealloc(void *p, size_t size);
    void  paranoidFreeAligned(void *p);
    void *paranoidCalloc(size_t count, size_t size);
    void *paranoidMemalign(size_t alignment, size_t size);

    static void *s_malloc(size_t size);
    static void  s_free(void *p);
    static void *s_realloc(void *p, size_t size);
    static void  s_freeAligned(void *p);
    static void *s_calloc(size_t count, size_t size);
    static void *s_memalign(size_t alignment, size_t size);

    typedef int (*OutputFunction)(const char *format, ...);
    OutputFunction SetOutputFunction(OutputFunction pfOutput);

    void AdHocSelfTest();
    void Dump();
    bool AreThereAllocatedBlocks();
    void HideUnfreedBlocks();

    static bool CheckIfMustBeUsed();
    static void ConfigureHooks(vx_sdk_config_t &config);
    static void ConfigureHooksIfRequired(vx_sdk_config_t &config);

    class Stats
    {
    public:
        Stats();
        Stats(const Stats &stats);
        Stats &operator=(const Stats &stats);
        Stats operator-(const Stats &stats);
        std::string toString();

        std::map<size_t, long> m_mapAllocsBySize;
        long m_lAllocs;
        long m_lFrees;
        long m_lAlignedAllocs;
        long m_lAlignedFrees;
        long m_lReallocs;
        long m_lCallocs;
        long m_lCurrentlyAllocated;
        long m_lPeakAllocated;
        long m_lHidden;
    };

    void GetStats(Stats &stats);

private:
    ParanoidAllocator();
    static ParanoidAllocator s_Instance;

    typedef std::map<void *, size_t> BlockSizeMap;
    typedef std::list<void *> BlockList;
    typedef std::map<size_t, BlockList *> BlockListMap;

    std::mutex m_mutex;
    BlockSizeMap m_mapAllocatedBlocks;
    BlockSizeMap m_mapAllocatedAlignedBlocks;
    BlockSizeMap m_mapHiddenAllocatedBlocks;
    BlockSizeMap m_mapHiddenAllocatedAlignedBlocks;
    BlockListMap m_mapFreeBlockListsBySize;

    Stats m_stats;

    static const size_t c_nNoMansLand = 32;
    static const unsigned char c_byteFiller = 0xAA;
    static const uint32_t c_uiMagic = 0xBFBFBFBF;
#if PARANOID_ALLOCATOR_BACKTRACE
    static const size_t c_nStackTraceSize = 1024;
#else
    static const size_t c_nStackTraceSize = 0;
#endif

    // Unprotected functions. Have m_mutex locked before calling them
    BlockList *GetBlockList(size_t size, bool bCreate);
    void  CheckAllMemBlockIntegrity();
    void  CheckMemBlockIntegrity(void *p, size_t blockSize = 0);
    void *paranoidAllocImpl(BlockSizeMap &map, size_t bytes, bool bUpdateStats);
    void  paranoidFreeImpl(BlockSizeMap &map, void *p, bool bUpdateStats);
    void  DumpStack(void *place, size_t size);
    void  PrintStack(void *place, size_t size);
    void  PrintCurrentStack(const char *pszMessage);

    size_t PABlockSizeFromUser(size_t bytes)
    {
        return bytes + 3 * c_nNoMansLand + c_nStackTraceSize;
    }
    void *UserBlockFromPA(unsigned char *p)
    {
        return p + c_nNoMansLand + c_nStackTraceSize + c_nNoMansLand;
    }
    unsigned char *PABlockFromUser(void *p)
    {
        return ((unsigned char *)p) - c_nNoMansLand - c_nStackTraceSize - c_nNoMansLand;
    }

    OutputFunction m_pStdOut, m_pStdErr;

    SymHelpers *m_pSymHelpers;
};
