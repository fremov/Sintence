#ifndef SINTENCE_CORE_MATCH_PAYLOAD_H
#define SINTENCE_CORE_MATCH_PAYLOAD_H

#include <cstddef>
#include <string>
#include <algorithm>
// core/match_payload — буфер с телом матча, владеющий сырой памятью.
//
// Правило пяти целиком: глубокое копирование, корректное самоприсваивание,
// перемещение, оставляющее источник пустым и валидным (Data() == nullptr,
// Size() == 0). Обе move-функции noexcept — иначе std::vector при расширении
// молча копирует вместо перемещения.

namespace sintence {

// Сырой JSON одного матча: байты, как они лежали в файле, до разбора.
// Разбор содержимого — не его дело: здесь только владение памятью.
class MatchPayload {
public:
    // Пустой payload: ничего не выделено.
    MatchPayload();

    // Копирует байты текста в собственный буфер.
    // Пустой текст — валидный вход: получается пустой payload,
    // и выделять под него ничего не нужно.
    explicit MatchPayload(const std::string& text);
    MatchPayload(const MatchPayload& other);
    MatchPayload& operator=(const MatchPayload& other);
    MatchPayload(MatchPayload&& other) noexcept;
    MatchPayload& operator=(MatchPayload&& other) noexcept;
    ~MatchPayload();
    
    // Указатель на байты. nullptr, если payload пуст.
    // Владение не передаётся: освобождает буфер только сам объект.
    const char* Data() const;

    // Сколько байт лежит в буфере.
    std::size_t Size() const;

    bool Empty() const;

    // Копия содержимого в виде строки. Для пустого payload — пустая строка.
    std::string ToString() const;

private:
    char* data_ = nullptr;
    std::size_t size_ = 0;
};

}  // namespace sintence

#endif  // SINTENCE_CORE_MATCH_PAYLOAD_H
