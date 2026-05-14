#pragma once
#include <vector>
#include <string>
#include <cstdint>
#include "Snowdrop.h"

// Read-only inspector that walks the live UI graph manager and lists
// every active .muigraph by path. Source-of-truth pointer chain:
//
//   g_pBase + 0x480ECE0   = qword_480ECE0
//                           dereference -> manager `this` (live heap ptr)
//   manager + 0x20        = pointer to the entry array
//   manager + 0x28        = int32 count (LO) + int32 capacity (HI)
//
//   each entry (24 bytes stride):
//     +0x00  const char*  path_str   (heap pool; direct C-string, NUL terminated)
//     +0x08  char[4]      tag        ("MAY_" magic — asset-table marker)
//     +0x0C  uint32_t     unk1       (flag or short hash; usually 0x01000000)
//     +0x10  void*        secondary  (sometimes points at another path/asset, e.g. .juice)
//
// We don't dereference the secondary pointer in the list view because its
// content varies (sometimes a path, sometimes binary). The "Show details"
// pane dumps the raw 24-byte entry plus a snippet of the secondary buffer.
class UIInspector
{
public:
    UIInspector();
    ~UIInspector();

    void DrawUI();

    // Refresh the cached entries from the live manager. Cheap — just reads
    // pointers and copies bounded strings. Called automatically on first
    // open and via the Refresh button.
    void Refresh();

private:
    struct Entry
    {
        std::uintptr_t entryAddr;   // address of this 24-byte slot
        std::uintptr_t pathPtr;     // entry[0..7]
        std::uintptr_t secondary;   // entry[0x10..0x17]
        char           tag[5];      // entry[0x08..0x0B] + NUL
        std::uint32_t  unk1;        // entry[0x0C..0x0F]
        std::string    path;        // resolved path string
        std::string    secondaryStr;// resolved secondary string (best-effort)
    };

    std::vector<Entry> m_entries;
    int   m_selectedIndex = -1;
    char  m_filter[128] = {};
    bool  m_loadedOnce  = false;

    // Manager snapshot (also useful as a header readout)
    std::uintptr_t m_managerAddr  = 0;
    std::uintptr_t m_arrayPtr     = 0;
    std::int32_t   m_arrayCount   = -1;
    std::int32_t   m_arrayCap     = -1;
    std::uintptr_t m_managerVtbl  = 0;

public:
    UIInspector(UIInspector const&) = delete;
    void operator=(UIInspector const&) = delete;
};
