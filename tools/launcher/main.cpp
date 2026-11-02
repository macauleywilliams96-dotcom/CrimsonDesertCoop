// CoopLauncher: keeps the co-op mod up to date, then starts the game.
//
// Lives in bin64 next to CrimsonDesert.exe. On each start it asks GitHub for
// the latest release of kRepo; if its tag differs from cdmp_version.txt it
// downloads the release's cdmp-update.zip and extracts it here (cdmp.ini is
// never in the zip, so each player's settings stay). The release notes carry
// "game_pe=XXXXXXXX", the game build the mod was made for: on any other build
// the mod is switched off (cdmp.asi -> cdmp.asi.off) instead of crashing the
// game. No internet: the game starts with the mod already installed.

#include <windows.h>
#include <shellapi.h>
#include <winhttp.h>

#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>

namespace fs = std::filesystem;

namespace {

constexpr wchar_t kRepo[] = L"macauleywilliams96-dotcom/CrimsonDesertCoop";
constexpr wchar_t kAsset[] = L"cdmp-update.zip";
constexpr wchar_t kSteamRun[] = L"steam://rungameid/3321460";
constexpr wchar_t kTitle[] = L"Crimson Desert Co-op";

// GET over https; follows redirects (release assets live on another host).
std::optional<std::string> http_get(const std::wstring& url) {
    URL_COMPONENTS uc{sizeof uc};
    wchar_t host[256], path[2048];
    uc.lpszHostName = host;
    uc.dwHostNameLength = 256;
    uc.lpszUrlPath = path;
    uc.dwUrlPathLength = 2048;
    if (!WinHttpCrackUrl(url.c_str(), 0, 0, &uc)) return std::nullopt;
    std::optional<std::string> out;
    HINTERNET s = WinHttpOpen(L"CoopLauncher/1.0", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, nullptr, nullptr, 0);
    HINTERNET c = s ? WinHttpConnect(s, host, uc.nPort, 0) : nullptr;
    HINTERNET r = c ? WinHttpOpenRequest(c, L"GET", path, nullptr, nullptr, nullptr, WINHTTP_FLAG_SECURE) : nullptr;
    if (r) {
        WinHttpSetTimeouts(r, 5000, 5000, 10000, 30000);
        const wchar_t* hdr = L"Accept: application/vnd.github+json, application/octet-stream\r\n";
        DWORD status = 0, sz = sizeof status;
        if (WinHttpSendRequest(r, hdr, static_cast<DWORD>(-1), nullptr, 0, 0, 0) &&
            WinHttpReceiveResponse(r, nullptr) &&
            WinHttpQueryHeaders(r, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, nullptr, &status, &sz,
                                nullptr) &&
            status == 200) {
            std::string body;
            for (DWORD avail = 0; WinHttpQueryDataAvailable(r, &avail) && avail;) {
                std::string chunk(avail, '\0');
                DWORD got = 0;
                if (!WinHttpReadData(r, chunk.data(), avail, &got)) break;
                body.append(chunk.data(), got);
            }
            out = std::move(body);
        }
    }
    if (r) WinHttpCloseHandle(r);
    if (c) WinHttpCloseHandle(c);
    if (s) WinHttpCloseHandle(s);
    return out;
}

// The string value of the first "key": "..." at or after `from` (no escapes needed here).
std::string json_str(const std::string& j, const std::string& key, size_t from = 0) {
    const auto k = j.find("\"" + key + "\"", from);
    if (k == std::string::npos) return {};
    const auto q1 = j.find('"', j.find(':', k) + 1);
    const auto q2 = j.find('"', q1 + 1);
    return q1 == std::string::npos || q2 == std::string::npos ? std::string{} : j.substr(q1 + 1, q2 - q1 - 1);
}

std::wstring widen(const std::string& s) {
    std::wstring w(s.size(), L'\0');
    w.resize(MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), w.data(), static_cast<int>(w.size())));
    return w;
}

std::string read_text(const fs::path& p) {
    std::ifstream f(p);
    std::string s;
    std::getline(f, s);
    return s;
}

uint32_t pe_timestamp(const fs::path& exe) {
    std::ifstream f(exe, std::ios::binary);
    uint32_t pe = 0, ts = 0;
    f.seekg(0x3C);
    f.read(reinterpret_cast<char*>(&pe), 4);
    f.seekg(pe + 8);
    f.read(reinterpret_cast<char*>(&ts), 4);
    return f ? ts : 0;
}

bool run_wait(const std::wstring& cmd) {
    STARTUPINFOW si{sizeof si};
    PROCESS_INFORMATION pi{};
    std::wstring line = cmd;
    if (!CreateProcessW(nullptr, line.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
        return false;
    }
    WaitForSingleObject(pi.hProcess, 60000);
    DWORD code = 1;
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return code == 0;
}

// Mod on or off for this game build.
void set_mod_enabled(const fs::path& dir, bool on) {
    const auto asi = dir / L"cdmp.asi", off = dir / L"cdmp.asi.off";
    std::error_code ec;
    if (on && fs::exists(off)) {
        fs::remove(asi, ec);
        fs::rename(off, asi, ec);
    } else if (!on && fs::exists(asi)) {
        fs::remove(off, ec);
        fs::rename(asi, off, ec);
    }
}

}  // namespace

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    wchar_t self[MAX_PATH];
    GetModuleFileNameW(nullptr, self, MAX_PATH);
    const fs::path dir = fs::path(self).parent_path();
    if (!fs::exists(dir / L"CrimsonDesert.exe")) {
        MessageBoxW(nullptr, L"Put CoopLauncher.exe in the game's bin64 folder (next to CrimsonDesert.exe).",
                    kTitle, MB_ICONERROR);
        return 1;
    }
    const auto version_file = dir / L"cdmp_version.txt";
    const std::string local = read_text(version_file);
    std::string game_pe = read_text(dir / L"cdmp_game_pe.txt");

    if (const auto rel = http_get(std::wstring(L"https://api.github.com/repos/") + kRepo + L"/releases/latest")) {
        const std::string tag = json_str(*rel, "tag_name");
        if (const auto at = rel->find("game_pe="); at != std::string::npos) game_pe = rel->substr(at + 8, 8);
        if (!tag.empty() && tag != local) {
            std::string url;
            for (size_t at = rel->find("\"browser_download_url\""); at != std::string::npos;
                 at = rel->find("\"browser_download_url\"", at + 1)) {
                const auto u = json_str(*rel, "browser_download_url", at);
                if (u.size() >= 15 && u.compare(u.size() - 15, 15, "cdmp-update.zip") == 0) url = u;
            }
            const auto zip = fs::temp_directory_path() / kAsset;
            const auto data = url.empty() ? std::nullopt : http_get(widen(url));
            bool ok = false;
            if (data && data->size() > 1000) {
                std::ofstream(zip, std::ios::binary).write(data->data(), static_cast<std::streamsize>(data->size()));
                set_mod_enabled(dir, true);  // extract over the enabled name
                ok = run_wait(L"tar.exe -xf \"" + zip.wstring() + L"\" -C \"" + dir.wstring() + L"\"");
                std::error_code ec;
                fs::remove(zip, ec);
            }
            if (ok) {
                std::ofstream(version_file) << tag;
                MessageBoxW(nullptr, (L"Co-op mod updated to " + widen(tag) + L".").c_str(), kTitle,
                            MB_ICONINFORMATION);
            } else {
                MessageBoxW(nullptr, (L"Could not update the co-op mod to " + widen(tag) +
                                      L". Starting with the installed version.").c_str(),
                            kTitle, MB_ICONWARNING);
            }
        }
        if (game_pe.size() == 8) std::ofstream(dir / L"cdmp_game_pe.txt") << game_pe;
    }

    // Right game build? Otherwise the mod would only crash it.
    if (game_pe.size() == 8) {
        const uint32_t want = static_cast<uint32_t>(std::stoul(game_pe, nullptr, 16));
        const bool match = pe_timestamp(dir / L"CrimsonDesert.exe") == want;
        set_mod_enabled(dir, match);
        if (!match) {
            MessageBoxW(nullptr,
                        L"The game has updated and the co-op mod does not support this version yet.\n\n"
                        L"The game will start without the mod. Try again once a mod update is out.",
                        kTitle, MB_ICONWARNING);
        }
    }
    ShellExecuteW(nullptr, L"open", kSteamRun, nullptr, nullptr, SW_SHOWNORMAL);
    return 0;
}
