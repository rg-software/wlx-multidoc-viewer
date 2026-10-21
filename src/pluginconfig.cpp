#include "pluginconfig.h"

#include <string>
#include <utility>

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace PluginConfig {

namespace {
inline constexpr const char* kIniName = "multidocviewer.ini";
inline constexpr const char* kSampleName = "multidocviewer.ini.sample";
}

const mINI::INIStructure& get() {
    // Lazy layered parse, cached for the process lifetime:
    //   1. shipped template (multidocviewer.ini.sample) as the base layer —
    //      absent file or key falls through to the hardcoded read-site default;
    //   2. the user's multidocviewer.ini overlaid key by key on top.
    // The result is cached for the process lifetime. Neither file is ever
    // written at runtime.
    static mINI::INIStructure ini;
    if (ini.size() == 0) {
        const std::string dir = modulePath();
        mINI::INIFile sampleFile(dir + "/" + kSampleName);
        sampleFile.read(ini);
        mINI::INIStructure user;
        mINI::INIFile userFile(dir + "/" + kIniName);
        if (userFile.read(user)) {
            for (auto& section : user) {
                for (auto& kv : section.second)
                    ini[section.first][kv.first] = kv.second;
            }
        }
    }
    return ini;
}

std::string modulePath() {
#ifdef _WIN32
    // Resolve the plugin DLL itself, not the host executable. The address of a
    // function in this translation unit is guaranteed to live in the DLL.
    HMODULE hMod = nullptr;
    if (GetModuleHandleExW(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCWSTR>(&modulePath), &hMod)) {
        wchar_t buf[MAX_PATH];
        const DWORD len = GetModuleFileNameW(hMod, buf, MAX_PATH);
        if (len > 0 && len < MAX_PATH) {
            std::wstring wide(buf, len);
            const size_t slash = wide.find_last_of(L"\\/");
            if (slash != std::wstring::npos)
                wide.erase(slash);
            return std::string(wide.begin(), wide.end());
        }
    }
#else
    Dl_info info;
    if (dladdr(reinterpret_cast<void*>(&modulePath), &info) && info.dli_fname) {
        std::string path(info.dli_fname);
        const size_t slash = path.find_last_of('/');
        if (slash != std::string::npos)
            path.erase(slash);
        else
            path.clear();
        return path.empty() ? std::string(".") : std::move(path);
    }
#endif
    return "."; // resolution failed - fall back to the current directory
}

} // namespace PluginConfig