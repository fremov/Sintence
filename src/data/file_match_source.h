#ifndef SINTENCE_DATA_FILE_MATCH_SOURCE_H
#define SINTENCE_DATA_FILE_MATCH_SOURCE_H

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>
#include <filesystem>
#include "match_source.h"  // интерфейс MatchSource из core/
namespace fs = std::filesystem;
// data/file_match_source — источник матчей из каталога с файлами.
//
// Адаптер отвечает за «где лежит и как прочитать»: разбор формата — в
// match_json, метрики — в analysis/.
//
// Несуществующий каталог даёт IsAvailable() == false и пустой вектор,
// а не исключение. Битый файл среди нормальных пропускается, остальные
// читаются. index.json из выгрузки матчем не считается.

namespace sintence {

class FileMatchSource : public MatchSource {
public:
    // dir   — каталог с файлами матчей (project/data/matches).
    // puuid — игрок, чьи строки достаём из каждого матча.
    //
    // Конструктор ничего не читает: каталог может быть недоступен, а сканировать
    // 193 файла при создании объекта незачем. Чтение происходит в LoadMatches.
    FileMatchSource(std::filesystem::path dir, std::string puuid);

    // "files:<имя каталога>" — попадает в сообщения и логи.
    std::string Name() const override;

    // Каталог существует, это каталог, и в нём есть хотя бы один *.json
    // кроме index.json.
    bool IsAvailable() const override;

    // Все записи игрока из всех файлов каталога.
    //
    // Порядок — как отдаёт обход каталога (на Windows это по имени).
    // Битые файлы и матчи без нужного игрока молча пропускаются:
    // сколько именно пропущено, вызывающий узнаёт, сравнив размер
    // результата с JsonFileCount().
    std::vector<MatchEntry> LoadMatches() const override;

    // Сколько файлов-кандидатов лежит в каталоге (*.json, кроме index.json).
    // Нужен приложению, чтобы честно печатать «прочитано N из M».
    std::size_t JsonFileCount() const;

private:
    std::filesystem::path dir_;
    std::string puuid_;
};

}  // namespace sintence

#endif  // SINTENCE_DATA_FILE_MATCH_SOURCE_H
