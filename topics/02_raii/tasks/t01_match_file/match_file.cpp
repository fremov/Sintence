#include "match_file.h"

namespace course {

MatchFile::MatchFile(std::string /*path*/) {
    // TODO: сохранить путь и попробовать открыть файл.
    // Функция называется std::fopen, режим для чтения бинарно — "rb".
    // Она возвращает nullptr, если открыть не удалось, и это нормальный
    // рабочий случай, а не повод для исключения.
}

// TODO: здесь же напиши тела тех специальных функций-членов,
// которые объявишь в match_file.h.

bool MatchFile::IsOpen() const {
    // TODO
    return false;
}

const std::string& MatchFile::Path() const {
    // TODO
    return path_;
}

std::FILE* MatchFile::Handle() const {
    // TODO
    return nullptr;
}

}  // namespace course
