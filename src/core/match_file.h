#ifndef SINTENCE_CORE_MATCH_FILE_H
#define SINTENCE_CORE_MATCH_FILE_H

#include <cstdio>
#include <string>

// core/match_file — владение открытым файлом (RAII).
//
// Конструктор захватывает ресурс, деструктор освобождает; больше нигде
// в коде нет вызовов fclose. Деструктор корректен и тогда, когда файл
// открыть не удалось: fclose(nullptr) — неопределённое поведение.
//
// Копирование запрещено явно: владение файлом уникально.
// Перемещения нет, поэтому объект нельзя вернуть из функции по значению
// и нельзя положить в std::vector.

namespace sintence {

class MatchFile {
public:
    // Открывает файл на чтение в бинарном режиме.
    //
    // Несуществующий путь — это не ошибка и не исключение: файл мог быть удалён
    // между сканированием каталога и открытием, и анализатор должен пережить это,
    // а не упасть посреди выгрузки. Объект в этом случае создаётся, но IsOpen()
    // возвращает false.
    explicit MatchFile(std::string path);
    ~MatchFile();
    MatchFile(const MatchFile&) = delete;
    MatchFile& operator=(const MatchFile&) = delete;

    // true, если файл удалось открыть.
    bool IsOpen() const;

    // Путь, с которым объект создавался, — включая случай, когда файл не открылся.
    // Без него сообщение об ошибке пришлось бы собирать на стороне вызывающего.
    const std::string& Path() const;

    // Сырой дескриптор для чтения. nullptr, если файл не открыт.
    // Владение не передаётся: закрывать его снаружи нельзя, этим занимается
    // деструктор. Через него читаются данные файла.
    std::FILE* Handle() const;

private:
    std::string path_;
    std::FILE* file_ = nullptr;
};

}  // namespace sintence

#endif  // SINTENCE_CORE_MATCH_FILE_H
