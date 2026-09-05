#include "match_payload.h"

namespace course {

MatchPayload::MatchPayload() {
    // TODO: пустой payload — здесь может не оказаться ни одной строки кода.
    // Подумай, почему: посмотри на инициализаторы полей в match_payload.h.
}

MatchPayload::MatchPayload(const std::string& /*text*/) {
    // TODO: выделить буфер под байты текста и скопировать их туда.
    // Выделяет new char[n], копирует std::memcpy или std::copy.
    // Отдельно реши, что делать с пустым текстом.
}

// TODO: здесь же напиши тела тех специальных функций-членов,
// которые объявишь в match_payload.h.

const char* MatchPayload::Data() const {
    // TODO
    return nullptr;
}

std::size_t MatchPayload::Size() const {
    // TODO
    return 0;
}

bool MatchPayload::Empty() const {
    // TODO
    return true;
}

std::string MatchPayload::ToString() const {
    // TODO
    return std::string();
}

}  // namespace course
