#include "file_match_source.h"

#include <fstream>
#include <sstream>
#include <system_error>
#include <utility>

#include "match_json.h"

namespace course {

FileMatchSource::FileMatchSource(std::filesystem::path dir, std::string puuid)
    : dir_(std::move(dir)), puuid_(std::move(puuid)) {
}

std::string FileMatchSource::Name() const {
    // TODO: "files:<имя каталога>", например "files:matches".
    //
    // API: dir_.filename().string() — последний компонент пути.
    // Осторожно: у пути со слешем на конце filename() пустой. Что с этим
    // делать (и надо ли) — реши сам, тест на это не давит.
    return {};
}

bool FileMatchSource::IsAvailable() const {
    // TODO: доступен ли источник.
    //
    // ПОТОК ДАННЫХ
    //   вход:  dir_
    //   выход: bool
    //
    // API, который нужен:
    //   std::error_code ec;
    //   std::filesystem::is_directory(dir_, ec)   версия с ec НЕ бросает
    //   std::filesystem::directory_iterator(dir_, ec)
    //   entry.path().extension() == ".json"
    //   entry.path().filename() == "index.json"
    //
    // ПОРЯДОК ДЕЙСТВИЙ
    //   1. Нет каталога или это не каталог — false. Без исключений:
    //      бери перегрузки с std::error_code, иначе несуществующий путь
    //      бросит filesystem_error и уронит приложение.
    //   2. Пройти каталог и ответить true на первом же подходящем файле:
    //      расширение .json и имя не index.json.
    //   3. Пустой каталог — false.
    //
    // Почему index.json не считается: это манифест выгрузки (кто, когда,
    // какие матчи), а не матч. Попав в разбор, он тихо даст ноль записей —
    // и ты будешь искать, почему «прочитано 192 из 193».
    return false;
}

std::vector<MatchEntry> FileMatchSource::LoadMatches() const {
    // TODO: прочитать все матчи каталога и достать из них строки игрока.
    //
    // ПОТОК ДАННЫХ
    //   вход:        dir_ и puuid_
    //   по каждому:  путь -> текст файла -> ParseMatchEntry -> MatchEntry?
    //   выход:       std::vector<MatchEntry> — только то, что разобралось
    //
    // API, который нужен:
    //   std::filesystem::directory_iterator(dir_, ec)
    //   std::ifstream file(entry.path());          RAII: закроется сам
    //   std::ostringstream buffer; buffer << file.rdbuf();
    //       — идиома «прочитать файл целиком в строку»
    //   ParseMatchEntry(text, puuid_)              -> std::optional<MatchEntry>
    //   result.push_back(std::move(*parsed))       не забудь move: внутри
    //                                              строка и структура
    //
    // ПОРЯДОК ДЕЙСТВИЙ
    //   1. Пустой результат, если IsAvailable() == false.
    //   2. Обойти каталог, пропуская не-.json и index.json — тем же условием,
    //      что и в IsAvailable. Одинаковое условие в двух местах — повод
    //      вынести его в маленькую функцию в анонимном namespace.
    //   3. Каждый файл прочитать целиком в строку.
    //      Не открылся — пропустить, не падать: файл могли удалить между
    //      обходом каталога и открытием.
    //   4. Отдать текст в ParseMatchEntry. Вернулся nullopt — пропустить.
    //   5. Собрать всё, что вернулось, в вектор и вернуть его.
    //
    // Про MatchFile из темы 2: здесь он сознательно не используется.
    // Он учил RAII на сыром FILE*, и свою задачу выполнил; std::ifstream —
    // уже RAII, закрывается сам и работает со строками, а не с fread.
    // Брать свой класс там, где стандартный лучше, — это не уважение
    // к своему коду, а лишняя работа при каждом чтении.
    //
    // Про производительность: 193 файла по 73 КБ — это 14 МБ на каждый запуск.
    // В Debug со санитайзером это будет заметно, и это нормально. Когда
    // станет мешать — померяем и решим (кеш разобранного, ленивая загрузка);
    // гадать до замера смысла нет.
    return {};
}

std::size_t FileMatchSource::JsonFileCount() const {
    // TODO: сколько файлов-кандидатов в каталоге.
    //
    // То же условие, что в IsAvailable и LoadMatches: *.json, кроме index.json.
    // Если ты уже вынес его в отдельную функцию — здесь просто счётчик
    // по обходу каталога.
    //
    // Несуществующий каталог — 0, без исключения.
    return 0;
}

}  // namespace course
