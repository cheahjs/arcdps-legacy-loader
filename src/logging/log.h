#pragma once

/* Logs to arcdps's log window/file via its e3 export. Safe to call before
 * mod_init returns as long as ArcdpsProxy has captured the handoff. */
namespace Log {
    void Msg(const char* fmt, ...);
}
