#ifndef ANALYZER_DATA_FILE_MATCH_SOURCE_H
#define ANALYZER_DATA_FILE_MATCH_SOURCE_H

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>
#include <filesystem>
#include "match_source.h"  // интерфейс MatchSource из core/
namespace fs = std::filesystem;
// ============================================================================
// data/file_match_source — источник матчей из каталога с файлами.
//
// Четвёртая реализация MatchSource, и первая, которая читает настоящие данные.
// Весь остальной анализатор от её появления не меняется ни на строку —
// ровно то, ради чего в теме 3 заводился интерфейс.
//
// Критерии приёмки:
//   1. В analysis/ и app/ не появилось ни одного упоминания
//      FileMatchSource — они работают через const MatchSource&.
//   2. Несуществующий каталог — IsAvailable() == false и пустой вектор,
//      а не исключение и не падение.
//   3. Битый файл среди нормальных не роняет загрузку: он пропускается,
//      остальные читаются.
//   4. index.json в выгрузке — не матч; он не должен попадать в записи.
//   5. Заглушек и TODO в файле не осталось.
//
// Одна идея: адаптер отвечает за «где лежит и как прочитать», и больше
// ни за что. Разбор формата — в match_json, метрики — в analysis/.
// ============================================================================

namespace course {

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

}  // namespace course

#endif  // ANALYZER_DATA_FILE_MATCH_SOURCE_H
