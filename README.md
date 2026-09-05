# Анализатор статистики матчей League of Legends

Самостоятельный курс C++ параллельно с «Разработка на C++ (расширенный)» Яндекс Практикума.
Девятнадцать тем, каждая добавляет кусок к одному приложению: разбор матчей, агрегация
по чемпионам и ролям, KDA, winrate, CS/min, потом реальные запросы к Riot API и интерфейс.

Учебный контракт — в `.claude/skills/cpp-mentor/SKILL.md`. План и прогресс — в `PROGRESS.md`.

## Требования

- C++17: MSVC 19.2x+, GCC 9+ или Clang 10+
- CMake ≥ 3.16
- Python 3.8+ (только для выгрузки матчей)

Проверено на этой машине: MSVC 14.51 (Visual Studio 2026), CMake 4.3.1, Python 3.13.7.

## Сборка и тесты

```powershell
cmake -S . -B build
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

Linux/macOS — то же самое, но конфигурация задаётся при генерации:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
```

Один тест:

```powershell
ctest --test-dir build -C Debug -R topic01_t01_champion --output-on-failure
```

Новая задача появляется в сборке сама: корневой `CMakeLists.txt` находит все
`topics/*/tasks/*/CMakeLists.txt` через `file(GLOB_RECURSE ...)`. После добавления папки
нужно один раз перегенерировать (`cmake -S . -B build`).

### Санитайзеры

Debug собирается с address/undefined sanitizer с первого дня: неопределённое поведение
не падает — оно работает, пока не перестанет. На MSVC это `/fsanitize=address /Zi`,
на GCC/Clang — `-fsanitize=address,undefined -g -O0`.

Если санитайзер мешает (например, конфликт с отладчиком):

```powershell
cmake -S . -B build -DCOURSE_SANITIZERS=OFF
```

Это временная мера, а не настройка по умолчанию.

## Что где лежит

```
CLAUDE.md                     правила для ассистента, указывает на навык
PROGRESS.md                   карта тем, слабые места, журнал сессий, темп
CMakeLists.txt                корень, сам находит задачи
.claude/skills/cpp-mentor/    учебный контракт
scripts/fetch_matches.py      выгрузка матчей Riot API в локальные файлы
third_party/doctest.h         doctest 2.4.12, один заголовок, сеть при сборке не нужна
topics/00_build_model/        единицы трансляции, компилятор против линкера
topics/01_classes/            классы, инкапсуляция, const-корректность
project/data/                 схема данных, фикстуры, README про ключ Riot
project/data/matches/         реальная выгрузка (в git не попадает)
```

Внутри темы:

- `THEORY.md` — справочная карточка на одну страницу, а не учебник.
- `tasks/<задача>/` — заголовок с критериями приёмки, пустая реализация, тесты.
- `demo/`, `experiments/` — только в теме 0: эталонный разбор и четыре сломанных примера.

## Данные

Курс работает с локальными файлами; C++ в сеть не ходит до темы 15. Как получить ключ,
сколько матчей качать и какие поля из ответа Riot нужны — в `project/data/README.md`.

Тесты работают на фикстурах из `project/data/fixtures/` и не требуют ни ключа, ни сети.

## Первый шаг

`topics/00_build_model/THEORY.md`, потом `topics/00_build_model/experiments/README.md`.
Полчаса на то, чем компилятор отличается от линкера, экономят недели.
