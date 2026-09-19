#!/usr/bin/env python3
"""Собирает theory/*/THEORY.md и REVIEW.md в одну HTML-страницу для чтения.

Исходник теории — по-прежнему Markdown: его удобно грепать, дифать и править.
HTML — то, что читаешь. Страница самодостаточная: ни сети, ни зависимостей,
ни внешних файлов. Только стандартная библиотека Python.

    python scripts/build_theory.py

Результат: theory/index.html
"""

from __future__ import annotations

import html
import re
import sys
from dataclasses import dataclass, field
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
THEORY_DIR = ROOT / "theory"
OUTPUT = THEORY_DIR / "index.html"

# --------------------------------------------------------------------------
# Подсветка C++
# --------------------------------------------------------------------------

CPP_KEYWORDS = {
    "alignas", "alignof", "auto", "bool", "break", "case", "catch", "char", "class",
    "concept", "const", "consteval", "constexpr", "constinit", "const_cast", "continue",
    "co_await", "co_return", "co_yield", "decltype", "default", "delete", "do", "double",
    "dynamic_cast", "else", "enum", "explicit", "export", "extern", "false", "final",
    "float", "for", "friend", "goto", "if", "import", "inline", "int", "long", "module",
    "mutable", "namespace", "new", "noexcept", "nullptr", "operator", "override",
    "private", "protected", "public", "register", "reinterpret_cast", "requires",
    "return", "short", "signed", "sizeof", "static", "static_assert", "static_cast",
    "struct", "switch", "template", "this", "throw", "true", "try", "typedef", "typeid",
    "typename", "union", "unsigned", "using", "virtual", "void", "volatile", "while",
}

# Токенизация одним проходом: порядок альтернатив задаёт приоритет.
CPP_TOKEN_RE = re.compile(
    r"""
    (?P<comment>//[^\n]*|/\*.*?\*/)
  | (?P<rawstring>R"\([^)]*\)")
  | (?P<string>"(?:[^"\\\n]|\\.)*")
  | (?P<char>'(?:[^'\\\n]|\\.)')
  | (?P<preproc>^[ \t]*\#[a-z_]+)
  | (?P<number>\b\d+(?:\.\d+)?(?:[fuzlFUZL]+)?\b)
  | (?P<ident>[A-Za-z_][A-Za-z0-9_]*)
    """,
    re.VERBOSE | re.DOTALL | re.MULTILINE,
)


def highlight_cpp(code: str) -> str:
    out: list[str] = []
    pos = 0
    for match in CPP_TOKEN_RE.finditer(code):
        out.append(html.escape(code[pos:match.start()]))
        kind = match.lastgroup
        text = html.escape(match.group())

        if kind == "ident":
            word = match.group()
            after = code[match.end():match.end() + 2]
            if word in CPP_KEYWORDS:
                cls = "kw"
            elif word == "std":
                cls = "ns"
            elif after.startswith("("):
                cls = "fn"
            elif word[0].isupper():
                cls = "type"
            else:
                cls = ""
            out.append(f'<span class="{cls}">{text}</span>' if cls else text)
        elif kind == "rawstring":
            out.append(f'<span class="str">{text}</span>')
        else:
            out.append(f'<span class="{kind}">{text}</span>')
        pos = match.end()

    out.append(html.escape(code[pos:]))
    return "".join(out)


def highlight_shell(code: str) -> str:
    lines = []
    for line in code.split("\n"):
        stripped = line.lstrip()
        if stripped.startswith("#") or stripped.startswith("::"):
            lines.append(f'<span class="comment">{html.escape(line)}</span>')
        else:
            lines.append(html.escape(line))
    return "\n".join(lines)


def render_code(code: str, lang: str) -> str:
    if lang in ("cpp", "c++", "c"):
        body = highlight_cpp(code)
    elif lang in ("powershell", "bash", "sh", "shell", "cmd"):
        body = highlight_shell(code)
    else:
        body = html.escape(code)
    label = f'<span class="lang">{html.escape(lang)}</span>' if lang else ""
    return f'<pre class="code">{label}<code>{body}</code></pre>'


# --------------------------------------------------------------------------
# Инлайновый Markdown
# --------------------------------------------------------------------------

def render_inline(text: str) -> str:
    placeholders: list[str] = []

    def stash(markup: str) -> str:
        placeholders.append(markup)
        return f"\x00{len(placeholders) - 1}\x00"

    # `код` — раньше всего, внутри него разметка не работает
    text = re.sub(
        r"`([^`]+)`",
        lambda m: stash(f"<code>{html.escape(m.group(1))}</code>"),
        text,
    )
    text = html.escape(text)
    text = re.sub(
        r"\[([^\]]+)\]\(([^)]+)\)",
        lambda m: stash(f'<a href="{m.group(2)}">{m.group(1)}</a>'),
        text,
    )
    text = re.sub(r"\*\*([^*]+)\*\*", r"<strong>\1</strong>", text)
    text = re.sub(r"(?<![\w*])\*([^*\n]+)\*(?![\w*])", r"<em>\1</em>", text)
    text = re.sub(r"(?<!\w)«([^»]+)»", r"«\1»", text)

    for index, markup in enumerate(placeholders):
        text = text.replace(f"\x00{index}\x00", markup)
    return text


# --------------------------------------------------------------------------
# Блочный Markdown
# --------------------------------------------------------------------------

@dataclass
class Section:
    """Заголовок второго уровня — пункт в боковом меню."""
    anchor: str
    title: str


@dataclass
class Chapter:
    slug: str
    number: str
    title: str
    html: str = ""
    sections: list[Section] = field(default_factory=list)


def plain_title(text: str) -> str:
    """Заголовок для бокового меню: без разметки, только текст."""
    text = re.sub(r"`([^`]+)`", r"\1", text)
    text = re.sub(r"\*\*([^*]+)\*\*", r"\1", text)
    return re.sub(r"\s+", " ", text).strip()


def slugify(text: str, used: set[str]) -> str:
    base = re.sub(r"[^\wа-яё -]", "", text.lower(), flags=re.IGNORECASE)
    base = re.sub(r"[\s-]+", "-", base).strip("-") or "section"
    candidate = base
    counter = 2
    while candidate in used:
        candidate = f"{base}-{counter}"
        counter += 1
    used.add(candidate)
    return candidate


def render_table(rows: list[str]) -> str:
    def cells(line: str) -> list[str]:
        return [c.strip() for c in line.strip().strip("|").split("|")]

    header = cells(rows[0])
    body = [cells(r) for r in rows[2:]]

    out = ["<table><thead><tr>"]
    out += [f"<th>{render_inline(c)}</th>" for c in header]
    out.append("</tr></thead><tbody>")
    for row in body:
        out.append("<tr>" + "".join(f"<td>{render_inline(c)}</td>" for c in row) + "</tr>")
    out.append("</tbody></table>")
    return "".join(out)


def render_markdown(md: str, chapter: Chapter, used_anchors: set[str]) -> str:
    lines = md.split("\n")
    out: list[str] = []
    index = 0

    while index < len(lines):
        line = lines[index]
        stripped = line.strip()

        if not stripped:
            index += 1
            continue

        # Код
        if stripped.startswith("```"):
            lang = stripped[3:].strip()
            index += 1
            block: list[str] = []
            while index < len(lines) and not lines[index].strip().startswith("```"):
                block.append(lines[index])
                index += 1
            index += 1
            out.append(render_code("\n".join(block), lang))
            continue

        # Заголовки
        heading = re.match(r"^(#{1,4})\s+(.*)$", stripped)
        if heading:
            level = len(heading.group(1))
            title = heading.group(2).strip()
            if level == 1:
                index += 1
                continue  # заголовок главы рисуется отдельно
            anchor = slugify(f"{chapter.slug}-{title}", used_anchors)
            if level == 2:
                chapter.sections.append(Section(anchor, title))
            out.append(
                f'<h{level} id="{anchor}">'
                f'<a class="anchor" href="#{anchor}">#</a>{render_inline(title)}</h{level}>'
            )
            index += 1
            continue

        # Горизонтальная черта
        if re.fullmatch(r"-{3,}|\*{3,}", stripped):
            out.append("<hr>")
            index += 1
            continue

        # Таблица
        if stripped.startswith("|") and index + 1 < len(lines) and re.match(
            r"^\|[\s:|-]+\|?$", lines[index + 1].strip()
        ):
            rows = []
            while index < len(lines) and lines[index].strip().startswith("|"):
                rows.append(lines[index])
                index += 1
            out.append(render_table(rows))
            continue

        # Цитата
        if stripped.startswith(">"):
            quote: list[str] = []
            while index < len(lines) and lines[index].strip().startswith(">"):
                quote.append(lines[index].strip()[1:].strip())
                index += 1
            out.append(f"<blockquote>{render_inline(' '.join(quote))}</blockquote>")
            continue

        # Списки
        bullet = re.match(r"^(\s*)([-*]|\d+\.)\s+(.*)$", line)
        if bullet:
            ordered = bool(re.match(r"^\d+\.$", bullet.group(2)))
            tag = "ol" if ordered else "ul"
            items: list[str] = []
            while index < len(lines):
                item = re.match(r"^(\s*)([-*]|\d+\.)\s+(.*)$", lines[index])
                if not item:
                    # продолжение пункта на следующей строке
                    if items and lines[index].startswith(("  ", "\t")) and lines[index].strip():
                        items[-1] += " " + lines[index].strip()
                        index += 1
                        continue
                    break
                items.append(item.group(3))
                index += 1
            body = "".join(f"<li>{render_inline(i)}</li>" for i in items)
            out.append(f"<{tag}>{body}</{tag}>")
            continue

        # Абзац
        paragraph: list[str] = []
        while index < len(lines) and lines[index].strip() and not re.match(
            r"^(#{1,4}\s|```|\||>|\s*([-*]|\d+\.)\s|-{3,}$)", lines[index].strip()
        ):
            paragraph.append(lines[index].strip())
            index += 1
        if paragraph:
            out.append(f"<p>{render_inline(' '.join(paragraph))}</p>")

    return "\n".join(out)


# --------------------------------------------------------------------------
# Сборка страницы
# --------------------------------------------------------------------------

CSS = """
:root {
  --bg: #12141a; --bg-soft: #181b23; --bg-code: #0e1016; --line: #262b36;
  --fg: #d8dde8; --fg-dim: #8b94a7; --accent: #7aa2f7; --accent-soft: #2a3450;
  --kw: #bb9af7; --str: #9ece6a; --num: #ff9e64; --comment: #5a6379;
  --type: #7dcfff; --fn: #7aa2f7; --ns: #2ac3de; --preproc: #e0af68;
}
:root[data-theme="light"] {
  --bg: #fbfbfd; --bg-soft: #f2f3f7; --bg-code: #f6f7fa; --line: #dfe2ea;
  --fg: #1f2430; --fg-dim: #5c6478; --accent: #2f5fd0; --accent-soft: #dfe7fb;
  --kw: #8b3fc9; --str: #2f7d32; --num: #b35000; --comment: #77808f;
  --type: #0b6f9c; --fn: #2f5fd0; --ns: #0e7490; --preproc: #9a6400;
}
* { box-sizing: border-box; }
html { scroll-behavior: smooth; scroll-padding-top: 1.5rem; }
body {
  margin: 0; background: var(--bg); color: var(--fg);
  font: 16px/1.68 -apple-system, "Segoe UI", Roboto, "Helvetica Neue", Arial, sans-serif;
  -webkit-font-smoothing: antialiased;
}
.layout { display: grid; grid-template-columns: 310px minmax(0, 1fr); }

aside {
  position: sticky; top: 0; height: 100vh; overflow-y: auto;
  background: var(--bg-soft); border-right: 1px solid var(--line); padding: 1.25rem 1rem 3rem;
}
aside h1 { font-size: .95rem; margin: 0 0 .25rem; letter-spacing: .01em; }
aside .sub { color: var(--fg-dim); font-size: .78rem; margin-bottom: 1rem; }
#search {
  width: 100%; padding: .5rem .65rem; border-radius: 7px; margin-bottom: 1rem;
  border: 1px solid var(--line); background: var(--bg); color: var(--fg); font-size: .85rem;
}
#search:focus { outline: none; border-color: var(--accent); }
.nav-chapter { margin-bottom: .35rem; }
.nav-chapter > a {
  display: block; padding: .4rem .5rem; border-radius: 6px; text-decoration: none;
  color: var(--fg); font-size: .87rem; font-weight: 600;
}
.nav-chapter > a:hover { background: var(--accent-soft); }
.nav-chapter > a .num { color: var(--fg-dim); font-weight: 400; margin-right: .4rem; }
.nav-sections { list-style: none; margin: .1rem 0 .5rem; padding-left: .7rem;
  border-left: 1px solid var(--line); }
.nav-sections a {
  display: block; padding: .22rem .5rem; text-decoration: none;
  color: var(--fg-dim); font-size: .81rem; border-radius: 5px;
}
.nav-sections a:hover { color: var(--fg); background: var(--accent-soft); }
.nav-sections a.active { color: var(--accent); background: var(--accent-soft); }

main { padding: 2.5rem 3rem 6rem; max-width: 60rem; }
.chapter { margin-bottom: 5rem; }
.chapter > h1 {
  font-size: 1.85rem; line-height: 1.25; margin: 0 0 1.5rem;
  padding-bottom: .7rem; border-bottom: 2px solid var(--line);
}
.chapter > h1 .num { color: var(--accent); }
h2 { font-size: 1.32rem; margin: 2.6rem 0 .9rem; }
h3 { font-size: 1.08rem; margin: 1.9rem 0 .6rem; }
h4 { font-size: .97rem; margin: 1.4rem 0 .5rem; color: var(--fg-dim); }
h2 .anchor, h3 .anchor, h4 .anchor {
  float: left; margin-left: -1.1rem; width: 1.1rem; color: var(--line);
  text-decoration: none; font-weight: 400;
}
h2:hover .anchor, h3:hover .anchor, h4:hover .anchor { color: var(--accent); }
p { margin: .85rem 0; }
a { color: var(--accent); }
strong { color: #fff; font-weight: 650; }
:root[data-theme="light"] strong { color: #000; }
ul, ol { margin: .85rem 0; padding-left: 1.4rem; }
li { margin: .35rem 0; }
hr { border: none; border-top: 1px solid var(--line); margin: 2.5rem 0; }
blockquote {
  margin: 1.2rem 0; padding: .7rem 1rem; border-left: 3px solid var(--accent);
  background: var(--bg-soft); color: var(--fg-dim); border-radius: 0 6px 6px 0;
}
code {
  font-family: "Cascadia Code", "JetBrains Mono", Consolas, monospace;
  font-size: .86em; background: var(--bg-soft); padding: .12em .35em;
  border-radius: 4px; border: 1px solid var(--line);
}
pre.code {
  position: relative; background: var(--bg-code); border: 1px solid var(--line);
  border-radius: 9px; padding: 1rem 1.1rem; overflow-x: auto; margin: 1.1rem 0;
  font-size: .855rem; line-height: 1.6;
}
pre.code code { background: none; border: none; padding: 0; font-size: 1em; }
pre.code .lang {
  position: absolute; top: .45rem; right: .7rem; font-size: .68rem;
  color: var(--fg-dim); text-transform: uppercase; letter-spacing: .06em;
}
.kw { color: var(--kw); } .str { color: var(--str); } .num { color: var(--num); }
.comment { color: var(--comment); font-style: italic; } .type { color: var(--type); }
.fn { color: var(--fn); } .ns { color: var(--ns); } .preproc { color: var(--preproc); }
.char { color: var(--str); }

table { border-collapse: collapse; width: 100%; margin: 1.2rem 0; font-size: .89rem; }
th, td { border: 1px solid var(--line); padding: .5rem .7rem; text-align: left;
  vertical-align: top; }
th { background: var(--bg-soft); font-weight: 600; }
tbody tr:nth-child(even) { background: rgba(255,255,255,.017); }
:root[data-theme="light"] tbody tr:nth-child(even) { background: rgba(0,0,0,.02); }

#theme {
  position: fixed; top: 1rem; right: 1.25rem; z-index: 10;
  background: var(--bg-soft); color: var(--fg); border: 1px solid var(--line);
  border-radius: 7px; padding: .4rem .7rem; cursor: pointer; font-size: .8rem;
}
#theme:hover { border-color: var(--accent); }
.hidden { display: none !important; }

@media (max-width: 1000px) {
  .layout { grid-template-columns: 1fr; }
  aside { position: static; height: auto; border-right: none;
    border-bottom: 1px solid var(--line); }
  main { padding: 1.5rem 1.1rem 4rem; }
}
"""

JS = """
const search = document.getElementById('search');
const chapters = [...document.querySelectorAll('.nav-chapter')];

search.addEventListener('input', () => {
  const q = search.value.trim().toLowerCase();
  chapters.forEach(nav => {
    const links = [...nav.querySelectorAll('.nav-sections a')];
    let anyVisible = false;
    links.forEach(a => {
      const hit = !q || a.textContent.toLowerCase().includes(q);
      a.classList.toggle('hidden', !hit);
      anyVisible = anyVisible || hit;
    });
    const titleHit = !q || nav.querySelector('a').textContent.toLowerCase().includes(q);
    nav.classList.toggle('hidden', !(titleHit || anyVisible));
    if (titleHit) links.forEach(a => a.classList.remove('hidden'));
  });
});

document.addEventListener('keydown', e => {
  if (e.key === '/' && document.activeElement !== search) { e.preventDefault(); search.focus(); }
  if (e.key === 'Escape') { search.value = ''; search.dispatchEvent(new Event('input')); search.blur(); }
});

// Подсветка текущего раздела в меню. Считаем по позиции, а не через
// IntersectionObserver: при переходе по якорю заголовок не пересекает
// узкую полосу наблюдения, и подсветка молча не срабатывает.
const links = new Map();
document.querySelectorAll('.nav-sections a').forEach(a => links.set(a.getAttribute('href').slice(1), a));
const headings = [...document.querySelectorAll('h2[id]')];
const aside = document.querySelector('aside');
let current = null;
let ticking = false;

function syncActive() {
  ticking = false;
  let found = headings[0];
  for (const h of headings) {
    if (h.getBoundingClientRect().top <= 120) found = h; else break;
  }
  if (!found || found === current) return;
  current = found;

  links.forEach(a => a.classList.remove('active'));
  const active = links.get(found.id);
  if (!active) return;
  active.classList.add('active');

  // меню едет за чтением, но только если пункт ушёл из видимой части
  const box = active.getBoundingClientRect();
  const frame = aside.getBoundingClientRect();
  if (box.top < frame.top + 40 || box.bottom > frame.bottom - 40) {
    active.scrollIntoView({ block: 'center', behavior: 'smooth' });
  }
}

addEventListener('scroll', () => {
  if (!ticking) { ticking = true; requestAnimationFrame(syncActive); }
}, { passive: true });
addEventListener('hashchange', () => requestAnimationFrame(syncActive));
syncActive();

const root = document.documentElement;
const saved = localStorage.getItem('theory-theme');
if (saved) root.dataset.theme = saved;
document.getElementById('theme').addEventListener('click', () => {
  const next = root.dataset.theme === 'light' ? 'dark' : 'light';
  root.dataset.theme = next;
  localStorage.setItem('theory-theme', next);
});
"""


def collect_chapters() -> list[Chapter]:
    chapters: list[Chapter] = []
    used_anchors: set[str] = set()

    for md_path in sorted(THEORY_DIR.glob("*/THEORY.md")):
        slug = md_path.parent.name
        number = slug.split("_")[0]
        text = md_path.read_text(encoding="utf-8")
        title_match = re.search(r"^#\s+(.*)$", text, re.MULTILINE)
        title = title_match.group(1).strip() if title_match else slug

        chapter = Chapter(slug=slug, number=number, title=title)
        chapter.html = render_markdown(text, chapter, used_anchors)
        chapters.append(chapter)

    review = ROOT / "REVIEW.md"
    if review.exists():
        text = review.read_text(encoding="utf-8")
        chapter = Chapter(slug="review", number="—", title="Повторение и вспоминание")
        chapter.html = render_markdown(text, chapter, used_anchors)
        chapters.append(chapter)

    return chapters


def build_page(chapters: list[Chapter]) -> str:
    nav: list[str] = []
    for chapter in chapters:
        sections = "".join(
            f'<li><a href="#{s.anchor}">{html.escape(plain_title(s.title))}</a></li>'
            for s in chapter.sections
        )
        number = "" if chapter.number == "—" else f'<span class="num">{chapter.number}</span>'
        nav.append(
            f'<div class="nav-chapter">'
            f'<a href="#chapter-{chapter.slug}">{number}{html.escape(chapter.title)}</a>'
            f'<ul class="nav-sections">{sections}</ul></div>'
        )

    body: list[str] = []
    for chapter in chapters:
        number = "" if chapter.number == "—" else f'<span class="num">{chapter.number}. </span>'
        body.append(
            f'<section class="chapter" id="chapter-{chapter.slug}">'
            f"<h1>{number}{html.escape(chapter.title)}</h1>{chapter.html}</section>"
        )

    return f"""<!DOCTYPE html>
<html lang="ru" data-theme="dark">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Теория — анализатор матчей League of Legends</title>
<style>{CSS}</style>
</head>
<body>
<button id="theme" type="button">тема</button>
<div class="layout">
<aside>
  <h1>Теория</h1>
  <div class="sub">анализатор матчей LoL · {len(chapters)} глав</div>
  <input id="search" type="search" placeholder="поиск по разделам  ( / )" autocomplete="off">
  <nav>{''.join(nav)}</nav>
</aside>
<main>{''.join(body)}</main>
</div>
<script>{JS}</script>
</body>
</html>
"""


def main() -> int:
    if not THEORY_DIR.is_dir():
        print(f"нет каталога {THEORY_DIR}", file=sys.stderr)
        return 1

    chapters = collect_chapters()
    if not chapters:
        print("не найдено ни одной главы theory/*/THEORY.md", file=sys.stderr)
        return 1

    OUTPUT.write_text(build_page(chapters), encoding="utf-8")
    total_sections = sum(len(c.sections) for c in chapters)
    size_kb = OUTPUT.stat().st_size / 1024
    print(f"{OUTPUT.relative_to(ROOT)}: {len(chapters)} глав, "
          f"{total_sections} разделов, {size_kb:.0f} КБ")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
