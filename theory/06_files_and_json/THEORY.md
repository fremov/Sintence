# Тема 6. Файлы, JSON и значения, которых может не быть

Глава, а не карточка: в Практикуме этого не было, книги по C++ про nlohmann не пишут,
значит это твой основной источник. Все сообщения об ошибках ниже получены прогоном
на твоей машине — не пересказ документации.

1. Зачем эта тема нужна
2. `std::optional` — «значения может не быть»
3. Прочитать файл целиком
4. `nlohmann::json`: разбор чужого формата
5. Четыре способа достать поле, и три из них опасны
6. `std::filesystem`: обход каталога
7. Разбор: из чего складывается `ParseMatchEntry`
8. Разбор твоего кода: почему `FileMatchSource` ничего не ломает
9. Ошибки этой темы
10. Читать в книгах
11. Вопросы на вспоминание
12. ВЫПИСАТЬ В БЛОКНОТ

---

## 1. Зачем эта тема нужна

Твой анализатор сейчас останавливается на третьей строке вывода:

```
Каталог: D:/.../project/data/matches
Игрок:   csZCcpsb...07MQ
Источник недоступен.
```

На диске 193 настоящих матча по 73 КБ. Между ними и `ChampionReport`, который ты
написал в теме 1, не хватает ровно двух вещей: **прочитать файл** и **понять,
что внутри**. Это и есть тема.

Дополнительная сложность, которой не было в предыдущих темах: данные **чужие**.
Ты не контролируешь, что пришло от Riot. Поле может отсутствовать, быть `null`,
быть строкой там, где ждёшь число. Файл может оказаться обрезанным. Поэтому
половина главы — не «как достать значение», а «как не упасть, когда его нет».

---

## 2. `std::optional<T>` — «значения может не быть»

### Задача, которую он решает

`ParseMatchEntry` должна вернуть запись игрока. Но игрока в матче может не быть,
а файл может оказаться битым. Что возвращать в этих случаях?

Плохие ответы, которые ты увидишь в чужом коде:

```cpp
MatchEntry ParseMatchEntry(...);            // а что вернуть при ошибке? пустую? какую?
bool ParseMatchEntry(..., MatchEntry& out); // результат через параметр, вызов нечитаем
MatchEntry* ParseMatchEntry(...);           // кто теперь владеет указателем?
```

`std::optional<T>` — это «либо значение типа `T`, либо ничего», в одном объекте,
без указателей и без выделения памяти:

```cpp
#include <optional>

std::optional<MatchEntry> ParseMatchEntry(std::string_view json, std::string_view puuid);
```

### Как им пользоваться

```cpp
std::optional<int> deaths;              // пусто
deaths = 11;                            // теперь значение

if (deaths.has_value()) { ... }         // явная проверка
if (deaths) { ... }                     // то же самое короче

const int value = *deaths;              // разыменование: БЕЗ проверки — UB
const int safe  = deaths.value();       // бросит std::bad_optional_access, если пусто
const int any   = deaths.value_or(0);   // 0, если пусто — самый частый вариант

return std::nullopt;                    // вернуть «ничего»
return MatchEntry{name, line};          // вернуть значение: обёртка сама построится
```

На пустом `optional` `value()` бросает исключение с текстом `Bad optional access`,
а `*` — это неопределённое поведение без единого предупреждения. Проверено.

### Монадические операции (C++23)

```cpp
std::optional<int> deaths = 11;

auto doubled = deaths.transform([](int d) { return d * 2; });      // optional<int>{22}
auto checked = deaths.and_then([](int d) -> std::optional<double> {
    return d == 0 ? std::nullopt : std::optional{10.0 / d};
});
```

`transform` применяет функцию, если значение есть, и ничего не делает, если пусто.
Цепочка из трёх `if (x.has_value())` схлопывается в три вызова. Пригодится
в `analysis/`, где метрики считаются одна из другой; в `ParseMatchEntry` можно
обойтись обычными проверками.

### Где здесь ловушка

`std::optional<T>` **хранит `T` внутри себя**, а не ссылку на него. Это значит,
что `optional<MatchEntry>` — полноценная копия записи, и её надо перемещать
так же, как всё остальное:

```cpp
result.push_back(std::move(*parsed));   // не забудь: тема 2, третий раз
```

---

## 3. Прочитать файл целиком

Стандартная идиома, которую стоит запомнить целиком:

```cpp
#include <fstream>
#include <sstream>

std::ifstream file(path);              // RAII: закроется сам, деструктор всё сделает
if (!file) {                           // не открылся — файл удалили, нет прав, занят
    return std::nullopt;
}

std::ostringstream buffer;
buffer << file.rdbuf();                // весь поток разом, без цикла по строкам
const std::string text = buffer.str();
```

`file.rdbuf()` — это указатель на буфер потока; `<<` от него перекачивает всё
содержимое за один приём. Без этого пришлось бы читать `std::getline` в цикле
и склеивать строки руками.

**Почему не `MatchFile` из темы 2.** Ты написал класс-обёртку над `FILE*`, и он
рабочий. Но `std::ifstream` уже RAII, уже закрывается сам и уже работает
со строками, а не с `fread` и ручным подсчётом байт. Брать свой класс там, где
стандартный делает то же самое лучше, — это не бережливость, а лишняя работа
на каждом чтении. `MatchFile` свою задачу выполнил: он научил, что деструктор
освобождает ресурс. Дальше он не нужен, и это нормальная судьба учебного класса.

**Проверка `if (!file)` обязательна.** Между тем, как ты увидел файл в каталоге,
и тем, как ты его открыл, файл может исчезнуть. На 193 файлах это никогда
не случится; на выгрузке, которую параллельно обновляет скрипт, — случится.

---

## 4. `nlohmann::json`: разбор чужого формата

Библиотека вендорена одним заголовком в `third_party/json.hpp`, версия 3.12.0.
Подключается так:

```cpp
#include "json.hpp"

using nlohmann::json;   // в .cpp можно; в .h — нельзя, см. ниже
```

### Правило границы, которое проверяется на ревью

`nlohmann::json` **не имеет права появиться** ни в одном заголовке `core/`,
`analysis/` или в публичной части `data/`. Только внутри `data/*.cpp`.

Причина записана в `project/ARCHITECTURE.md` и она практическая: один `json`
в доменном заголовке — и весь анализатор перестаёт собираться без этой библиотеки,
а замена её на другую становится переписыванием половины проекта. Посмотри
на `src/data/match_json.h`: там `std::optional<MatchEntry>` и `std::string_view`,
и ни одного типа библиотеки. Так и должно остаться.

### Разбор без исключений

```cpp
const json doc = json::parse(text, nullptr, false);
//                                  ^^^^^^^  ^^^^^
//                                  колбэк   allow_exceptions = false
if (doc.is_discarded()) {
    return std::nullopt;            // текст не разобрался
}
```

Третий аргумент `false` — самое важное в этой строке. Без него битый файл бросит
исключение:

```
[json.exception.parse_error.101] parse error at line 1, column 2:
syntax error while parsing object key - invalid literal; last read: '{б'
```

С ним — вернётся объект, у которого `is_discarded() == true`, и ты сам решаешь,
что делать. Для перебора 193 файлов, среди которых может попасться обрезанный,
второе удобнее: битый файл не должен ронять разбор остальных.

### Структура ответа Match-V5

```
{
  "metadata": { "matchId": "RU_528252891", "participants": [ ...десять puuid... ] },
  "info": {
    "gameDuration": 2315,
    "queueId": 420,
    "participants": [
      { "puuid": "...", "championName": "Darius", "kills": 3, "deaths": 11,
        "assists": 9, "win": false, "totalMinionsKilled": 173,
        "neutralMinionsKilled": 0, "teamPosition": "TOP", ...ещё 135 полей... },
      ...ещё девять участников...
    ]
  }
}
```

Из этого тебе нужны семь полей. Два наблюдения, которые экономят время:

- **`gameDuration` лежит в `info`, а не в участнике.** Это единственное поле,
  которое берётся не из твоей строки.
- **CS — это сумма двух полей.** `totalMinionsKilled` (миньоны) плюс
  `neutralMinionsKilled` (лес). У леснка второе слагаемое — основная часть фарма:
  в фикстуре у Nidalee 11 миньонов и 74 нейтрала. Забудешь — CS/min окажется
  в семь раз меньше настоящего, и ни один тест типов этого не заметит.

### Обход массива участников

```cpp
const auto& participants = doc["info"]["participants"];
for (const auto& p : participants) {
    if (p.value("puuid", std::string{}) == puuid) {
        // нашёл себя
    }
}
```

Обход `json`, который **не** массив, ошибки не даст — объект обходится по своим
значениям, и ты молча получишь не то. Проверяй `is_array()` перед циклом.

---

## 5. Четыре способа достать поле, и три из них опасны

Это раздел, ради которого стоило писать главу целиком. Все четыре выглядят
одинаково безобидно.

| Способ | На отсутствующем ключе | На `null` в значении |
|---|---|---|
| `doc["ключ"]` на **const** объекте | **assert и падение процесса** | вернёт `null`-узел |
| `doc.at("ключ")` | исключение `out_of_range.403` | вернёт `null`-узел |
| `doc.value("ключ", умолчание)` | вернёт умолчание | **исключение `type_error.302`** |
| `doc.contains("ключ")` + проверка типа | `false`, ты решаешь | `is_null()`, ты решаешь |

Проверенные сообщения:

```
const doc["нет-такого"]  -> Assertion failed: it != m_data.m_value.object->end(),
                            file third_party/json.hpp, line 22188
doc.at("нет-такого")     -> [json.exception.out_of_range.403] key 'нет-такого' not found
p["deaths"].get<int>()   -> [json.exception.type_error.302] type must be number, but is null
p.value("deaths", -1)    -> [json.exception.type_error.302] type must be number, but is null
arr.at(5) на массиве из 1 -> [json.exception.out_of_range.401] array index 5 is out of range
```

**Две вещи здесь контринтуитивны, и обе тебя укусят.**

Первая: `operator[]` у `json` ведёт себя как `operator[]` у `std::map` из темы 4 —
на неконстантном объекте он **создаёт** отсутствующий ключ, а на константном
не может, и вместо честной ошибки получается `assert` в Debug и UB в Release.
Тот же вывод, что и в теме 4: `operator[]` — инструмент записи, не чтения.

Вторая, и она прямо про твою задачу: **`value()` спасает от отсутствующего ключа,
но не от `null` в значении.** В `fixture_malformed.json` у нужного участника
`deaths` равно `null`, а не отсутствует. `p.value("deaths", -1)` на нём бросит
`type_error.302`, тест «битая фикстура не роняет разбор» станет красным,
а в выводе будет непойманное исключение.

Отсюда рабочий приём для чужих данных:

```cpp
if (!p.contains("deaths") || !p["deaths"].is_number_integer()) {
    return std::nullopt;                 // нет поля или оно не число
}
const int deaths = p["deaths"].get<int>();
```

Многословно — да. Но каждая строка отвечает на реальный вопрос про данные,
которые ты не контролируешь. Когда таких полей семь, это просится в маленькую
вспомогательную функцию в анонимном namespace — и это будет правильным решением,
а не преждевременным обобщением.

### Проверки типов, которые понадобятся

```cpp
p.contains("kills")            // есть ли ключ
p["kills"].is_number_integer() // целое число
p["win"].is_boolean()          // bool
p["championName"].is_string()  // строка
p["deaths"].is_null()          // явный null
participants.is_array()        // массив
```

---

## 6. `std::filesystem`: обход каталога

```cpp
#include <filesystem>
namespace fs = std::filesystem;          // общепринятое сокращение
```

### Путь — это тип, а не строка

```cpp
fs::path dir = "project/data/matches";
fs::path file = dir / "RU_528252891.json";   // оператор / собирает путь
                                             // и сам ставит разделитель

file.filename()    // "RU_528252891.json"
file.stem()        // "RU_528252891"
file.extension()   // ".json"
file.string()      // обратно в std::string, для печати
```

Склейка путей через `+` и `"/"` руками — источник ошибок на Windows;
`operator/` делает это правильно на всех системах.

### Обход и почему нужен `error_code`

У почти каждой функции `filesystem` две версии: бросающая и с `std::error_code`.

```cpp
// Бросающая: несуществующий каталог уронит программу
for (const auto& entry : fs::directory_iterator(dir)) { ... }
// directory_iterator::directory_iterator: The system cannot find the path
// specified.: "Z:/нет-такого-каталога"

// С error_code: ошибка возвращается, итератор становится равен end()
std::error_code ec;
for (const auto& entry : fs::directory_iterator(dir, ec)) { ... }
// цикл просто не выполнится ни разу
```

Оба сообщения проверены. Для `FileMatchSource` нужна вторая форма: по контракту
`IsAvailable()` на несуществующем каталоге возвращает `false`, а не бросает.

```cpp
std::error_code ec;
if (!fs::is_directory(dir, ec)) {
    return false;                        // нет каталога или это файл
}

for (const auto& entry : fs::directory_iterator(dir, ec)) {
    if (entry.path().extension() == ".json" &&
        entry.path().filename() != "index.json") {
        return true;
    }
}
return false;
```

**Почему `index.json` исключается.** Это манифест выгрузки (кто, когда, какие матчи),
а не матч. Попав в разбор, он честно вернёт `nullopt` — и ты будешь полчаса искать,
почему «прочитано 192 из 193».

---

## 7. Разбор: из чего складывается `ParseMatchEntry`

Собираем всё вышесказанное в поток данных. Кода здесь нет намеренно — его пишешь ты.

```
текст файла (std::string_view)
      │
      ├─ json::parse(text, nullptr, false)
      │     └─ is_discarded()? ──► nullopt
      │
      ├─ есть ли "info", и объект ли это? ──► нет: nullopt
      ├─ есть ли "info"/"participants", массив ли? ──► нет: nullopt
      │
      ├─ цикл по участникам
      │     └─ p.value("puuid", "") == puuid? ──► нет: следующий
      │
      ├─ нашёл участника
      │     ├─ championName — строка? ──► нет: nullopt
      │     ├─ kills/deaths/assists — целые? ──► нет: nullopt
      │     ├─ win — bool? ──► нет: nullopt
      │     ├─ totalMinionsKilled + neutralMinionsKilled ──► minions
      │     └─ info.gameDuration — целое? ──► нет: nullopt
      │
      └─ MatchEntry{ championName, MatchLine{...} }

участник не найден за весь цикл ──► nullopt
```

Три решения, которые стоит принять до того, как писать:

1. **Где проверять поля** — по мере доступа или все сразу в начале. Второе читается
   лучше, но требует сначала найти участника.
2. **Что делать, если поле одно битое, а остальные целые.** Контракт в заголовке
   говорит `nullopt` — весь матч целиком. Альтернатива (подставить ноль) заманчива
   и неверна: KDA, посчитанный по частично битой записи, — это число, которое
   выглядит настоящим и таковым не является.
3. **Нужна ли вспомогательная функция** вроде «достать целое поле или `nullopt`».
   Семь полей с одинаковой проверкой — достаточный повод.

---

## 8. Разбор твоего кода: почему `FileMatchSource` ничего не ломает

Посмотри на `src/analysis/index_by_map.cpp`, который ты написал вчера:

```cpp
ChampionIndex BuildChampionIndexFast(const MatchSource& source) {
    ChampionIndex index;
    if (!source.IsAvailable()) {
        return index;
    }
    auto matches = source.LoadMatches();
    ...
}
```

Теперь появляется `FileMatchSource` — класс, который ходит в файловую систему,
дёргает JSON-библиотеку и обрабатывает битые данные. Сколько строк надо изменить
в `BuildChampionIndexFast`, чтобы он заработал на реальной выгрузке?

**Ноль.** Он принимает `const MatchSource&`, а не конкретный класс. То же самое
с `main.cpp`, с `BuildChampionIndex`, со всеми тестами. Ты добавляешь четвёртую
реализацию интерфейса, и весь код выше по стеку про неё не знает.

Это тот самый возврат вложений из темы 3, и стоит его прочувствовать сейчас,
потому что в теме 3 он был обещанием, а сейчас — фактом. В варианте без интерфейса
`BuildChampionIndexFast` содержал бы `if (source_type == kFixtures) ... else if
(source_type == kFiles) ...`, и каждый новый источник (Live Client Data в теме 19,
LCU потом) добавлял бы ветку в функцию, которая вообще не про источники.

Проверить это на себе можно прямо сейчас, не написав ни строки: в
`tests/test_file_match_source.cpp` есть случай «источник работает через интерфейс
MatchSource», где твой файловый источник передаётся в `BuildChampionIndexFast`
как `const MatchSource&`.

---

## 9. Ошибки этой темы

| Что сделал | Что увидишь |
|---|---|
| `json::parse(text)` без третьего аргумента на битом файле | `[json.exception.parse_error.101] parse error at line 1, column 2: syntax error...` — непойманное исключение роняет программу |
| `doc["ключ"]` на const `json` с отсутствующим ключом | `Assertion failed: it != m_data.m_value.object->end(), file json.hpp, line 22188` — падение процесса в Debug, UB в Release |
| `p.value("deaths", -1)`, а в данных `"deaths": null` | `[json.exception.type_error.302] type must be number, but is null` — `value()` защищает от отсутствия ключа, но не от `null` |
| `get<int>()` без проверки `is_number_integer()` | та же `type_error.302` |
| `fs::directory_iterator(dir)` без `error_code` на несуществующем пути | `directory_iterator::directory_iterator: The system cannot find the path specified.` |
| разыменовал пустой `std::optional` через `*` | ничего при компиляции; UB. Через `.value()` — честное `Bad optional access` |
| забыл `neutralMinionsKilled` в сумме CS | ни ошибки, ни предупреждения: CS/min у леснка втрое-семикратно занижен |
| `index.json` попал в разбор как матч | «прочитано 192 из 193» и полчаса поисков |

Первые пять — исключения и падения, их видно сразу. Последние три — тихие:
программа работает, числа неверные.

---

## 10. Читать в книгах

- **«Чистый код», глава 7 «Обработка ошибок».** Мартин пишет «не возвращайте null»
  и «не передавайте null» — и он прав, но в Java у него нет альтернативы, кроме
  исключений. В C++ альтернатива есть: `std::optional` — это «значения нет»,
  выраженное в типе, которое вызывающий обязан распаковать. Компилятор не даст
  забыть проверку так, как даёт забыть проверку на `nullptr`. Читай главу
  и подставляй `std::optional` везде, где он говорит про null.
- **«Чистый код», глава 3 «Функции».** Раздел про один уровень абстракции:
  `ParseMatchEntry` не должна одновременно ходить в файловую систему, разбирать
  JSON и считать метрики. Она делает одно, `FileMatchSource` — другое,
  `ChampionReport` — третье. Когда будешь писать, проверь себя этим.

> Сверь номера глав со своим изданием при первом обращении.

---

## 11. Вопросы на вспоминание

Отвечать без подглядывания, вслух или в блокнот. Уйдут в `REVIEW.md`.

6.1 Чем `std::optional<T>` лучше, чем вернуть `T*` или `bool` с выходным параметром?

6.2 Что произойдёт при `*empty_optional` и при `empty_optional.value()`? Разница
принципиальная — в чём она?

6.3 `json::parse(text)` и `json::parse(text, nullptr, false)` — что меняет третий
аргумент и почему для перебора 193 файлов нужен второй вариант?

6.4 В данных `"deaths": null`. Что вернёт `p.value("deaths", -1)`?

6.5 Почему `nlohmann::json` не имеет права появиться в `match_json.h`, хотя
внутри `match_json.cpp` он используется свободно?

6.6 Чем `fs::directory_iterator(dir)` отличается от `fs::directory_iterator(dir, ec)`
и какой нужен в `IsAvailable()`?

6.7 Сколько строк в `BuildChampionIndexFast` надо изменить, чтобы он заработал
с `FileMatchSource` вместо `FixtureMatchSource`? Почему?

6.8 Из каких двух полей складывается CS, и как выглядит ошибка, если взять только одно?

---

## 12. ВЫПИСАТЬ В БЛОКНОТ

```
OPTIONAL (#include <optional>)
  std::optional<T> f();          вернуть значение или std::nullopt
  if (x)  /  x.has_value()       проверка
  *x                             БЕЗ проверки — UB
  x.value()                      бросает bad_optional_access
  x.value_or(по_умолчанию)       самый частый вариант
  x.transform(f) / x.and_then(f) C++23, цепочки без вложенных if

ЧТЕНИЕ ФАЙЛА ЦЕЛИКОМ
  std::ifstream file(path);      RAII, закроется сам
  if (!file) return ...;         проверять обязательно
  std::ostringstream buf;
  buf << file.rdbuf();           весь файл одной строкой
  buf.str()

NLOHMANN JSON (#include "json.hpp", только в data/*.cpp!)
  json::parse(text, nullptr, false)   НЕ бросает на битом
  doc.is_discarded()                  не разобралось
  doc.contains("k")                   есть ли ключ
  doc["k"]      на const + нет ключа → ASSERT, падение
  doc.at("k")   нет ключа → out_of_range.403
  doc.value("k", по_умолчанию)        нет ключа → умолчание
                                      ЗНАЧЕНИЕ null → ИСКЛЮЧЕНИЕ 302 (!)
  p["k"].is_number_integer() / is_string() / is_boolean() / is_null()
  p["k"].get<int>()                   только после проверки типа

  Порядок для чужих данных:  contains → is_тип → get

FILESYSTEM (#include <filesystem>, namespace fs = std::filesystem)
  fs::path dir = "...";  dir / "file.json"    склейка через /
  p.filename() / p.stem() / p.extension() / p.string()
  std::error_code ec;
  fs::is_directory(dir, ec)                   НЕ бросает
  fs::directory_iterator(dir, ec)             НЕ бросает, сразу == end()
  без ec на несуществующем пути → filesystem_error

ПОЛЯ MATCH-V5
  info.gameDuration                     ← длительность, НЕ в участнике
  info.participants[].puuid             ← найти себя среди десяти
  info.participants[].championName
  info.participants[].kills/deaths/assists/win
  CS = totalMinionsKilled + neutralMinionsKilled   ← ОБА слагаемых
  index.json — манифест, не матч: пропускать
```
