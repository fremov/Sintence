#ifndef SINTENCE_DATA_APP_LOG_H
#define SINTENCE_DATA_APP_LOG_H

#include <filesystem>
#include <format>
#include <string_view>
#include <utility>

// data/app_log — журнал приложения.
//
// У sintence.exe нет консоли: всё, что раньше печаталось в неё, пишется
// сюда. По умолчанию файл %LOCALAPPDATA%\Sintence\logs\sintence.log,
// каждая строка — с датой, временем до миллисекунд и номером потока:
//
//   2026-09-26 18:42:01.123 [14820] сервер данных: http://127.0.0.1:8777
//   2026-09-26 18:42:05.870 [9312] ОШИБКА: riot: 403 — ключ недействителен
//
// Строка сбрасывается на диск сразу: журнал, потерянный при аварийном
// завершении, бесполезен ровно тогда, когда нужен.
//
// До OpenLogFile (и в тестах, где его не зовут) строки идут в stderr.
// OpenLogFile заодно направляет в файл stdout и stderr процесса: отчёт
// ASan или случайный printf из библиотеки тоже окажутся в журнале.
//
// Что в журнал не попадает никогда: ключ Riot и пароль из lockfile
// клиента League (POLICY.md).

namespace sintence {

// %LOCALAPPDATA%\Sintence\logs\sintence.log; порт не 8777 — sintence-<порт>.log,
// чтобы отладочная копия рядом с рабочей не писала в тот же файл.
std::filesystem::path DefaultLogPath(int port);

// Открыть журнал (каталоги создаются). Файл больше 5 МБ сначала
// переименовывается в *.old.log — предыдущий старый затирается.
// false — не открылся, строки продолжают идти в stderr.
bool OpenLogFile(const std::filesystem::path& path);

// Строка журнала как есть — для текста, собранного во время выполнения.
void LogText(std::string_view text);
void LogErrorText(std::string_view text);

template <class... Args>
void Log(std::format_string<Args...> format, Args&&... args) {
    LogText(std::format(format, std::forward<Args>(args)...));
}

// То же с пометкой «ОШИБКА:» — такие строки ищутся в журнале первыми.
template <class... Args>
void LogError(std::format_string<Args...> format, Args&&... args) {
    LogErrorText(std::format(format, std::forward<Args>(args)...));
}

}  // namespace sintence

#endif  // SINTENCE_DATA_APP_LOG_H
