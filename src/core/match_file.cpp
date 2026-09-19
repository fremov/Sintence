#include "match_file.h"

namespace course {
MatchFile::MatchFile(std::string path) : path_(path) {
    // Функция называется std::fopen, режим для чтения бинарно — "rb".
    // Она возвращает nullptr, если открыть не удалось, и это нормальный
    // рабочий случай, а не повод для исключения.
    file_ = std::fopen(path.c_str(), "rb");
}

MatchFile::~MatchFile() {
    if (file_ != nullptr) {
        std::fclose(file_);
    }
}

bool MatchFile::IsOpen() const {
    return file_ != nullptr;
}

const std::string& MatchFile::Path() const {
    return path_;
}

std::FILE* MatchFile::Handle() const {
    return file_;
}
} // namespace course