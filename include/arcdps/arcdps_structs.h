#pragma once

#include <stdint.h>
#include <windows.h>

typedef struct cbtevent {
    uint64_t time;
    uint64_t src_agent;
    uint64_t dst_agent;
    int32_t  value;
    int32_t  buff_dmg;
    uint32_t overstack_value;
    uint32_t skill_id;
    uint16_t src_instid;
    uint16_t dst_instid;
    uint16_t src_master_instid;
    uint16_t dst_master_instid;
    uint8_t  iff;
    uint8_t  buff;
    uint8_t  result;
    uint8_t  is_activation;
    uint8_t  is_buffremove;
    uint8_t  is_ninety;
    uint8_t  is_fifty;
    uint8_t  is_moving;
    uint8_t  is_statechange;
    uint8_t  is_flanking;
    uint8_t  is_shields;
    uint8_t  is_offcycle;
    uint8_t  pad61;
    uint8_t  pad62;
    uint8_t  pad63;
    uint8_t  pad64;
} cbtevent;

typedef struct ag {
    const char* name;
    uintptr_t   id;
    uint32_t    prof;
    uint32_t    elite;
    uint32_t    self;
    uint16_t    team;
} ag;

typedef struct arcdps_exports {
    uintptr_t   size;
    uint32_t    sig;
    uint32_t    imguivers;
    const char* out_name;
    const char* out_build;
    void* wnd_nofilter;
    void* combat;
    void* imgui;
    void* options_end;
    void* combat_local;
    void* wnd_filter;
    void* options_windows;
} arcdps_exports;

typedef void     (*combat_cb_t)(cbtevent*, ag*, ag*, const char*, uint64_t, uint64_t);
typedef uint32_t (*wndproc_cb_t)(HWND, UINT, WPARAM, LPARAM);
typedef void     (*imgui_cb_t)(uint32_t not_charsel_or_loading, uint32_t hide_if_combat_or_ooc);
typedef void     (*options_end_cb_t)();        /* a.k.a. options_tab */
typedef uint32_t (*options_windows_cb_t)(const char*);

/* Per the arcdps API:
 *   void* get_init_addr(arcversion, imguictx, id3dptr, arcdll, mallocfn, freefn, d3dversion)
 *     -> returns a pointer to the addon's parameterless mod_init.
 *   arcdps_exports* mod_init(void)
 *     -> returns the addon's exports table.
 *   void* get_release_addr() -> returns a pointer to parameterless mod_release. */
typedef arcdps_exports* (*mod_init_t)();
typedef void            (*mod_release_t)();

typedef mod_init_t    (*get_init_addr_fn_t)(
    char*    arcversion,
    void*    imguictx,
    void*    id3dptr,
    HMODULE  arcdll,
    void*    mallocfn,
    void*    freefn,
    uint32_t d3dversion);
typedef mod_release_t (*get_release_addr_fn_t)();
