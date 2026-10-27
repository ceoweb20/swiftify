"""Check catalog validity, placeholders, and coverage of literal qsTr calls."""
import json
import pathlib
import re
import sys

root = pathlib.Path(sys.argv[1])
translations_header = (root / "src/app/Translations.h").read_text(encoding="utf-8")


def is_language_enabled(flag_name):
    match = re.search(rf"\b{flag_name}\s*=\s*(true|false)\b", translations_header)
    assert match is not None, f"Missing {flag_name} in src/app/Translations.h"
    return match.group(1) == "true"


def unique_object(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError(f"Duplicate translation: {key}")
        result[key] = value
    return result


if not is_language_enabled("kRussianTranslationEnabled"):
    print("Skipped Russian translations (disabled in src/app/Translations.h).")
    raise SystemExit(0)

catalog = json.loads((root / "translations/ru.json").read_text(encoding="utf-8"), object_pairs_hook=unique_object)
for source, translated in catalog.items():
    forms = translated if isinstance(translated, list) else [translated]
    if isinstance(translated, list):
        assert len(forms) == 3, source
    for form in forms:
        assert isinstance(form, str) and form.strip(), source
        assert sorted(re.findall(r"%(?:n|[1-9]\d*)", source)) == sorted(
            re.findall(r"%(?:n|[1-9]\d*)", form)
        ), f"Placeholders differ: {source}"

missing = []
for path in (root / "qml").rglob("*.qml"):
    for match in re.finditer(r'qsTr\(("(?:[^"\\]|\\.)*")', path.read_text(encoding="utf-8")):
        source = json.loads(match.group(1))
        if source not in catalog:
            missing.append(f"{path.relative_to(root)}: {source}")
if missing:
    raise SystemExit("Missing Russian translations:\n" + "\n".join(missing))
print(f"Validated {len(catalog)} Russian translations and QML coverage.")
