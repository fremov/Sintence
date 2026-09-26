#include "app_log.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <io.h>
#include <share.h>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <string>
#include <system_error>

namespace sintence {

namespace {

constexpr std::uintmax_t kRotateBytes = 5 * 1024 * 1024;

std::mutex g_mutex;
std::FILE* g_file = nullptr;  // nullptr — журнал не открыт, строки в stderr

void Write(std::string_view prefix, std::string_view text) {
    const auto now = std::chrono::current_zone()->to_local(std::chrono::system_clock::now());
    const std::string line =
        std::format("{:%Y-%m-%d %H:%M:%S} [{}] {}{}\n",
                    std::chrono::floor<std::chrono::milliseconds>(now), GetCurrentThreadId(),
                    prefix, text);
    const std::lock_guard<std::mutex> lock(g_mutex);
    std::FILE* target = g_file != nullptr ? g_file : stderr;
    std::fputs(line.c_str(), target);
    std::fflush(target);
}

std::filesystem::path LocalAppData() {
    wchar_t* value = nullptr;
    std::size_t size = 0;
    if (_wdupenv_s(&value, &size, L"LOCALAPPDATA") != 0 || value == nullptr) {
        return std::filesystem::temp_directory_path();
    }
    std::filesystem::path result(value);
    std::free(value);
    return result;
}

}  // namespace

std::filesystem::path DefaultLogPath(int port) {
    const std::wstring name =
        port == 8777 ? L"sintence.log" : L"sintence-" + std::to_wstring(port) + L".log";
    return LocalAppData() / L"Sintence" / L"logs" / name;
}

bool OpenLogFile(const std::filesystem::path& path) {
    std::error_code error;
    std::filesystem::create_directories(path.parent_path(), error);

    // Ротация: один старый файл, не больше. Журнал нужен, чтобы понять
    // последние запуски, а не хранить историю за полгода.
    if (std::filesystem::file_size(path, error) > kRotateBytes && !error) {
        std::filesystem::path old = path;
        old.replace_extension(L".old.log");
        std::filesystem::remove(old, error);
        std::filesystem::rename(path, old, error);
    }

    // _SH_DENYNO: журнал можно открыть Блокнотом, пока приложение работает.
    // freopen так не умеет — он открывает файл с запретом записи другим.
    std::FILE* file = _wfsopen(path.c_str(), L"a", _SH_DENYNO);
    if (file == nullptr) {
        return false;
    }
    const std::lock_guard<std::mutex> lock(g_mutex);
    g_file = file;

    // stdout и stderr — туда же: у оконного приложения консоли нет, и вывод
    // мимо журнала пропал бы молча. Дескрипторы 1 и 2 становятся копиями
    // журнала; кто пишет в хэндлы Windows напрямую (рантайм ASan), берёт
    // их из GetStdHandle.
    std::fflush(stdout);
    std::fflush(stderr);
    const int fd = _fileno(file);
    _dup2(fd, 1);
    _dup2(fd, 2);
    const auto handle = reinterpret_cast<HANDLE>(_get_osfhandle(fd));
    SetStdHandle(STD_OUTPUT_HANDLE, handle);
    SetStdHandle(STD_ERROR_HANDLE, handle);
    return true;
}

void LogText(std::string_view text) {
    Write("", text);
}

void LogErrorText(std::string_view text) {
    Write("ОШИБКА: ", text);
}

}  // namespace sintence
