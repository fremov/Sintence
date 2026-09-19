#include "doctest.h"

#include "match_file.h"

#include <string>
#include <type_traits>

using course::MatchFile;

namespace {

// Путь подставляет CMake — тесты работают с настоящими фикстурами анализатора
// из project/data/fixtures/, а не с выдуманными файлами.
const std::string kExistingPath = std::string(COURSE_FIXTURES_DIR) + "/fixture_win_mid.json";
const std::string kMissingPath = std::string(COURSE_FIXTURES_DIR) + "/no_such_match_9999.json";

}  // namespace

TEST_CASE("существующий файл открывается") {
    const MatchFile file(kExistingPath);
    CHECK(file.IsOpen());
    CHECK(file.Handle() != nullptr);
    CHECK(file.Path() == kExistingPath);
}

TEST_CASE("несуществующий файл — не открыт, но объект жив") {
    // Некорректный вход. Файл мог быть удалён между сканированием каталога
    // и открытием: анализатор должен это пережить, а не упасть.
    const MatchFile file(kMissingPath);
    CHECK_FALSE(file.IsOpen());
    CHECK(file.Handle() == nullptr);

    // Путь сохраняется в любом случае — иначе сообщение об ошибке не собрать.
    CHECK(file.Path() == kMissingPath);

    // Отдельно: на выходе из этого TEST_CASE сработает деструктор.
    // Если он зовёт fclose безусловно, здесь будет fclose(nullptr) —
    // неопределённое поведение, и упадёт либо тест, либо санитайзер.
}

TEST_CASE("пустой путь") {
    // Граничный случай: fopen("") не открывает ничего ни на одной ОС.
    const MatchFile file("");
    CHECK_FALSE(file.IsOpen());
    CHECK(file.Handle() == nullptr);
}

TEST_CASE("деструктор действительно закрывает файл") {
    // Главный тест темы. Каждый MatchFile живёт один виток цикла и должен
    // закрыться на выходе из области видимости.
    //
    // Если этот тест падает, а первый прошёл — значит файлы открываются,
    // но не закрываются: у процесса кончились дескрипторы (их около 500 на
    // Windows и около 1000 на Linux), и очередной fopen вернул nullptr.
    // Это ровно то, что RAII обязан предотвращать.
    int opened = 0;
    for (int i = 0; i < 2000; ++i) {
        const MatchFile file(kExistingPath);
        if (file.IsOpen()) {
            ++opened;
        }
    }
    CHECK(opened == 2000);
}

TEST_CASE("копирование запрещено") {
    // Владение уникально: две копии закрыли бы один и тот же FILE* дважды.
    // Запрет должен быть явным (= delete), тогда попытка скопировать —
    // ошибка компиляции с внятным текстом, а не порча памяти в рантайме.
    CHECK_FALSE(std::is_copy_constructible<MatchFile>::value);
    CHECK_FALSE(std::is_copy_assignable<MatchFile>::value);
}

TEST_CASE("два объекта на один путь владеют разными дескрипторами") {
    const MatchFile first(kExistingPath);
    const MatchFile second(kExistingPath);

    REQUIRE(first.IsOpen());
    REQUIRE(second.IsOpen());
    CHECK(first.Handle() != second.Handle());
}

TEST_CASE("все геттеры доступны у const-объекта") {
    // Возврат к теме 1: если этот тест не компилируется — где-то потерян const.
    MatchFile mutable_file(kExistingPath);
    const MatchFile& file = mutable_file;

    CHECK(file.IsOpen());
    CHECK(file.Path() == kExistingPath);
    CHECK(file.Handle() != nullptr);
}
