#!/usr/bin/env python3
import json
import re
import unicodedata
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SOURCE_JSON = ROOT / "scripts" / "emoji.json"
OUT_C = ROOT / "src" / "emoji_data.c"
OUT_H = ROOT / "src" / "emoji_data.h"

GENERATED_HEADER = "/* # GENERATED - do not edit; run scripts/gen_emoji.py */\n"


def c_escape(text: str) -> str:
    return text.replace("\\", "\\\\").replace('"', '\\"')


def ascii_fold(text: str) -> str:
    text = unicodedata.normalize("NFKD", text.casefold())
    text = "".join(ch for ch in text if not unicodedata.combining(ch))
    return text.encode("ascii", "ignore").decode("ascii")


def ascii_words(text: str) -> list[str]:
    text = ascii_fold(text)
    return re.findall(r"[a-z0-9]+", text)


def alias_words(alias: str) -> list[str]:
    stripped = alias.strip().strip(":")
    return ascii_words(stripped)


def build_keywords(entry: dict) -> str:
    seen: set[str] = set()
    ordered: list[str] = []

    def add(word: str) -> None:
        if not word or word in seen:
            return
        seen.add(word)
        ordered.append(word)

    for alias in entry.get("aliases", []):
        for word in alias_words(alias):
            add(word)
    for tag in entry.get("tags", []):
        for word in ascii_words(tag):
            add(word)
    for word in ascii_words(entry.get("description", "")):
        add(word)
    return " ".join(ordered)


def build_aliases(entry: dict) -> str:
    seen: set[str] = set()
    ordered: list[str] = []

    def add(token: str) -> None:
        if not token or token in seen:
            return
        seen.add(token)
        ordered.append(token)

    for alias in entry.get("aliases", []):
        stripped = alias.strip().strip(":").lower()
        add(stripped)
    return " ".join(ordered)


def build_name_norm(name: str) -> str:
    return " ".join(ascii_words(name))


def load_entries() -> list[dict]:
    with SOURCE_JSON.open("r", encoding="utf-8") as f:
        raw = json.load(f)

    entries: list[dict] = []
    for item in raw:
        glyph = item.get("emoji", "")
        name = item.get("description", "")
        if not glyph or not name:
            continue
        entries.append(
            {
                "glyph": glyph,
                "name": name,
                "name_norm": build_name_norm(name),
                "aliases": build_aliases(item),
                "keywords": build_keywords(item),
            }
        )
    return entries


def render_h(entry_count: int) -> str:
    return (
        GENERATED_HEADER
        + "\n"
        + "#ifndef EMOJI_DATA_H\n"
        + "#define EMOJI_DATA_H\n"
        + "\n"
        + "typedef struct {\n"
        + "    const char *glyph;\n"
        + "    const char *name;\n"
        + "    const char *name_norm;\n"
        + "    const char *aliases;\n"
        + "    const char *keywords;\n"
        + "} EmojiEntry;\n"
        + "\n"
        + f"#define EMOJI_COUNT {entry_count}\n"
        + "\n"
        + "extern const EmojiEntry EMOJI_TABLE[];\n"
        + "extern const int EMOJI_TABLE_LEN;\n"
        + "\n"
        + "#endif\n"
    )


def render_c(entries: list[dict]) -> str:
    lines = [
        GENERATED_HEADER.rstrip("\n"),
        "",
        '#include "emoji_data.h"',
        "",
        "const EmojiEntry EMOJI_TABLE[] = {",
    ]
    for entry in entries:
        lines.append(
            f'    {{"{c_escape(entry["glyph"])}", "{c_escape(entry["name"])}", "{c_escape(entry["name_norm"])}", "{c_escape(entry["aliases"])}", "{c_escape(entry["keywords"])}"}},'
        )
    lines.extend(
        [
            "};",
            "",
            f"const int EMOJI_TABLE_LEN = {len(entries)};",
            "",
        ]
    )
    return "\n".join(lines)


def main() -> None:
    entries = load_entries()
    OUT_H.write_text(render_h(len(entries)), encoding="utf-8")
    OUT_C.write_text(render_c(entries), encoding="utf-8")
    print(f"generated {OUT_H} and {OUT_C} ({len(entries)} rows)")


if __name__ == "__main__":
    main()
