# Анализатор статистики матчей League of Legends

Самостоятельный курс C++ параллельно с «Разработка на C++ (расширенный)» Яндекс Практикума.
Девятнадцать тем, каждая добавляет кусок к одному приложению: разбор матчей, агрегация
по чемпионам и ролям, KDA, winrate, CS/min, потом реальные запросы к Riot API и интерфейс.

Учебный контракт — в `.claude/skills/cpp-mentor/SKILL.md`. План и прогресс — в `PROGRESS.md`.

## Требования

- C++23: MSVC 14.51+ (Visual Studio 2026), GCC 15+ или Clang 20+
- CMake ≥ 3.20
- Python 3.8+ (только для выгрузки матчей)

Проверено на этой машине: MSVC 14.51 (Visual Studio 2026), CMake 4.3.1, Python 3.13.7.

Планка высокая осознанно: курс пользуется `std::print`/`std::format`, `contains()`,
`std::expected` и `<flat_map>`. `<flat_map>` появился в MSVC ровно в 14.51 и в GCC 15,
`std::print` — в GCC 14 (на Windows требует `-lstdc++exp` при линковке).
MSVC не понимает флаг `/std:c++23` и молча игнорирует его — стандарт задаётся
через `/std:c++latest`, и CMake подставляет его сам.

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

Две мишени и больше ничего: **`analyzer`** — приложение из `src/app/main.cpp`,
**`analyzer_tests`** — весь набор тестов одним бинарником.

Отдельная группа тестов — флагами doctest, а не пересборкой:

```powershell
build\tests\Debug\analyzer_tests.exe --source-file="*match_json*"
build\tests\Debug\analyzer_tests.exe --test-case="*winrate*"
build\tests\Debug\analyzer_tests.exe --list-test-cases
```

Новый `src/**/*.cpp` и `tests/test_*.cpp` подхватываются сами, нужно только
один раз перегенерировать (`cmake -S . -B build`).

Собрать без тестов:

```powershell
cmake -S . -B build -DANALYZER_TESTS=OFF
```

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
src/core/                     доменные типы: Champion, ChampionReport, ChampionIndex
src/data/                     адаптеры: файлы, JSON, резервный источник
src/analysis/                 метрики и агрегация над типами core/
src/app/main.cpp              точка входа, только обвязка
tests/                        один бинарник analyzer_tests из всех test_*.cpp
theory/index.html             ЧИТАТЬ ЗДЕСЬ: все главы одной страницей, собирается сама
theory/*/THEORY.md            исходники глав (правятся здесь, а не в html)
theory/00_build_model/        плюс experiments/ и demo/: компилятор против линкера
project/data/                 схема данных, фикстуры, README про ключ Riot
project/data/matches/         реальная выгрузка (в git не попадает)
project/ARCHITECTURE.md       решения по слоям и границам
scripts/fetch_matches.py      выгрузка матчей Riot API в локальные файлы
third_party/doctest.h         doctest 2.4.12, один заголовок
third_party/json.hpp          nlohmann/json 3.12.0, один заголовок
.claude/skills/cpp-mentor/    учебный контракт
PROGRESS.md                   состояние проекта, слабые места, журнал сессий
REVIEW.md                     повторение по интервалам: очередь и вопросы на вспоминание
practice_code/                разовая практика из Практикума, в сборку не входит
```

Зависимости идут только вниз: `app/ → analysis/ → core/`, `app/ → data/ → core/`.
Правило из `ARCHITECTURE.md`: ни один тип библиотеки (nlohmann, позже Qt)
не попадает в `core/` и `analysis/` — они видны только внутри `data/*.cpp`.

## Запуск

```powershell
cmake --build build --config Debug --target analyzer
build\src\Debug\analyzer.exe
```

Без аргументов берётся `project/data/matches` и `puuid` из лежащего там
`index.json`. Иначе: `analyzer.exe <каталог> [puuid]`.

## Теория

Открыть `theory/index.html` в браузере: все главы одной страницей, боковое меню,
поиск по разделам (клавиша `/`), тёмная и светлая тема, подсветка C++.

Страница собирается из `theory/*/THEORY.md` и `REVIEW.md` скриптом
`scripts/build_theory.py` на каждой сборке — править её руками бессмысленно,
изменения затрутся. Правится Markdown, страница обновляется сама:

```powershell
cmake --build build --config Debug --target theory
python scripts/build_theory.py          # то же самое без CMake
```

Скрипт на голой стандартной библиотеке: ни pip, ни сети, ни зависимостей.
В git лежит Markdown, `theory/index.html` в него не попадает.

## Данные

193 реальных матча уже выгружены в `project/data/matches/`. Как получить ключ,
сколько матчей качать и какие поля из ответа Riot нужны — в `project/data/README.md`.

Тесты работают и на фикстурах из `project/data/fixtures/`, и на реальной выгрузке;
ни ключа, ни сети не требуют.

## Первый шаг

Запусти `analyzer.exe` и посмотри, где он останавливается. Там и работа.
