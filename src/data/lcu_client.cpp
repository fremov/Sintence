#include "lcu_client.h"

// httplib первым: он включает winsock2.h, а windows.h без WIN32_LEAN_AND_MEAN
// тянет старый winsock.h, и вместе они не компилируются.
#include <httplib.h>

// windows.h определяет min и max макросами — NOMINMAX до включения.
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <tlhelp32.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>

namespace sintence {

namespace {

std::string EnvDirectory() {
    // getenv, а не _dupenv_s: _CRT_SECURE_NO_WARNINGS определён для sintence_lib.
    const char* value = std::getenv("SINTENCE_LOL_DIR");
    return value ? std::string(value) : std::string();
}

// Каталог запущенного LeagueClientUx.exe. Клиент работает без повышения
// прав, поэтому PROCESS_QUERY_LIMITED_INFORMATION хватает и нам без UAC.
std::string RunningClientDirectory() {
    const HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        return {};
    }
    std::string directory;
    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    for (BOOL ok = Process32FirstW(snapshot, &entry); ok; ok = Process32NextW(snapshot, &entry)) {
        if (_wcsicmp(entry.szExeFile, L"LeagueClientUx.exe") != 0) {
            continue;
        }
        const HANDLE process =
            OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, entry.th32ProcessID);
        if (process == nullptr) {
            continue;
        }
        wchar_t path[MAX_PATH * 2] = {};
        DWORD size = static_cast<DWORD>(std::size(path));
        if (QueryFullProcessImageNameW(process, 0, path, &size)) {
            directory = std::filesystem::path(path).parent_path().string();
        }
        CloseHandle(process);
        if (!directory.empty()) {
            break;
        }
    }
    CloseHandle(snapshot);
    return directory;
}

std::optional<std::string> ReadFile(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return std::nullopt;
    }
    std::ostringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

}  // namespace

std::string FindLeagueDirectory() {
    std::vector<std::string> candidates;
    if (auto from_env = EnvDirectory(); !from_env.empty()) {
        candidates.push_back(std::move(from_env));
    }
    if (auto running = RunningClientDirectory(); !running.empty()) {
        candidates.push_back(std::move(running));
    }
    candidates.emplace_back("C:\\Riot Games\\League of Legends");
    candidates.emplace_back("D:\\Riot Games\\League of Legends");

    for (const std::string& candidate : candidates) {
        std::error_code error;
        if (std::filesystem::exists(std::filesystem::path(candidate) / "lockfile", error)) {
            return candidate;
        }
    }
    return {};
}

std::optional<LcuCredentials> LcuClient::Credentials() {
    {
        const std::lock_guard<std::mutex> lock(mutex_);
        if (credentials_) {
            return credentials_;
        }
    }
    const std::string directory = FindLeagueDirectory();
    if (directory.empty()) {
        return std::nullopt;
    }
    const auto text = ReadFile(std::filesystem::path(directory) / "lockfile");
    if (!text) {
        return std::nullopt;
    }
    auto credentials = ParseLockfile(*text);
    const std::lock_guard<std::mutex> lock(mutex_);
    credentials_ = credentials;
    return credentials;
}

std::optional<std::string> LcuClient::Get(std::string_view path) {
    const auto credentials = Credentials();
    if (!credentials) {
        return std::nullopt;
    }

    // Сертификат клиента самоподписанный (корень Riot), как у Live Client
    // на :2999. Соединение локальное, до 127.0.0.1.
    httplib::SSLClient client("127.0.0.1", credentials->port);
    client.enable_server_certificate_verification(false);
    client.set_basic_auth("riot", credentials->password);
    client.set_connection_timeout(2);
    client.set_read_timeout(3);

    const auto response = client.Get(std::string(path));
    if (!response) {
        // Клиент закрыт или перезапущен с новым портом — перечитаем lockfile.
        const std::lock_guard<std::mutex> lock(mutex_);
        credentials_.reset();
        return std::nullopt;
    }
    if (response->status == 401) {
        // Старый пароль: клиент перезапустился, lockfile уже другой.
        const std::lock_guard<std::mutex> lock(mutex_);
        credentials_.reset();
        return std::nullopt;
    }
    if (response->status != 200) {
        return std::nullopt;
    }
    return response->body;
}

LcuClient::Response LcuClient::Send(std::string_view method, std::string_view path,
                                    std::string_view json_body) {
    Response result;
    const auto credentials = Credentials();
    if (!credentials) {
        return result;
    }

    httplib::SSLClient client("127.0.0.1", credentials->port);
    client.enable_server_certificate_verification(false);
    client.set_basic_auth("riot", credentials->password);
    client.set_connection_timeout(2);
    client.set_read_timeout(5);

    const std::string target(path);
    const std::string body(json_body);
    httplib::Result response;
    if (method == "POST") {
        response = client.Post(target, body, "application/json");
    } else if (method == "PUT") {
        response = client.Put(target, body, "application/json");
    } else if (method == "PATCH") {
        response = client.Patch(target, body, "application/json");
    } else if (method == "DELETE") {
        response = client.Delete(target);
    } else {
        return result;
    }

    if (!response) {
        const std::lock_guard<std::mutex> lock(mutex_);
        credentials_.reset();
        return result;
    }
    if (response->status == 401) {
        const std::lock_guard<std::mutex> lock(mutex_);
        credentials_.reset();
    }
    result.status = response->status;
    result.body = response->body;
    return result;
}

bool LcuClient::Connected() const {
    const std::lock_guard<std::mutex> lock(mutex_);
    return credentials_.has_value();
}

}  // namespace sintence
