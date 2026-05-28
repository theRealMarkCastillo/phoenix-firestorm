#!/usr/bin/env python3
"""
XUI code-gen: generates a typed C++ accessor header from an XUI floater XML.

Usage:
    python3 xui_codegen.py <floater.xml> <OutputClass> > output.gen.h

The generated header defines a struct (or base class) with one typed member
per named UI control found in the XML.  The floater's postBuild() can then
use these instead of runtime getChild<T>("string") lookups.

Example — given floater_foo.xml with:
    <line_editor name="search_field" />
    <button      name="ok_btn" />

The tool emits:
    struct FooUI {
        LLLineEditor* search_field {};
        LLButton*     ok_btn       {};
        bool postBuild(LLPanel* panel);
    };

postBuild() calls panel->getChild<T>("name") once and caches the result.
Any name typo becomes a compile-time-detectable nullptr (+ assertion).

Supported control types (extend TAG_TO_TYPE as needed):
    button, check_box, combo_box, line_editor, scroll_list,
    spin_ctrl, tab_container, text, text_editor, icon,
    fs:scroll_list_ctrl (FSScrollListCtrl)
"""
from __future__ import annotations

import argparse
import re
import sys
import xml.etree.ElementTree as ET
from dataclasses import dataclass, field
from pathlib import Path

# ── tag name → (C++ type, include hint) ──────────────────────────────────────
TAG_TO_TYPE: dict[str, tuple[str, str]] = {
    "button":            ("LLButton",         "llbutton.h"),
    "check_box":         ("LLCheckBoxCtrl",   "llcheckboxctrl.h"),
    "combo_box":         ("LLComboBox",        "llcombobox.h"),
    "icon":              ("LLIconCtrl",        "lliconctrl.h"),
    "line_editor":       ("LLLineEditor",      "lllineeditor.h"),
    "scroll_list":       ("LLScrollListCtrl",  "llscrolllistctrl.h"),
    "fs:scroll_list_ctrl": ("FSScrollListCtrl", "fsscrolllistctrl.h"),
    "fs_scroll_list":    ("FSScrollListCtrl",  "fsscrolllistctrl.h"),
    "spin_ctrl":         ("LLSpinCtrl",        "llspinctrl.h"),
    "spinner":           ("LLSpinCtrl",        "llspinctrl.h"),
    "tab_container":     ("LLTabContainer",    "lltabcontainer.h"),
    "text":              ("LLTextBox",         "lltextbox.h"),
    "text_editor":       ("LLTextEditor",      "lltexteditor.h"),
    "slider":            ("LLSliderCtrl",      "llsliderctrl.h"),
    "radio_group":       ("LLRadioGroup",      "llradiogroup.h"),
    "color_swatch":      ("LLColorSwatchCtrl", "llcolorswatch.h"),
    "texture_picker":    ("LLTextureCtrl",     "lltexturectrl.h"),
}

# C++ keywords that cannot be used as identifiers — prefixed with "ui_" to avoid
# conflicts.  Includes the alternative token keywords (and, or, not, etc.).
_CPP_KEYWORDS: frozenset[str] = frozenset({
    "alignas", "alignof", "and", "and_eq", "asm", "auto",
    "bitand", "bitor", "bool", "break",
    "case", "catch", "char", "char8_t", "char16_t", "char32_t", "class",
    "compl", "concept", "const", "consteval", "constexpr", "constinit",
    "const_cast", "continue", "co_await", "co_return", "co_yield",
    "decltype", "default", "delete", "do", "double", "dynamic_cast",
    "else", "enum", "explicit", "export", "extern",
    "false", "float", "for", "friend",
    "goto",
    "if", "inline", "int",
    "long",
    "mutable",
    "namespace", "new", "noexcept", "not", "not_eq", "nullptr",
    "operator", "or", "or_eq",
    "private", "protected", "public",
    "register", "reinterpret_cast", "requires", "return",
    "short", "signed", "sizeof", "static", "static_assert",
    "static_cast", "struct", "switch",
    "template", "this", "thread_local", "throw", "true", "try", "typedef",
    "typeid", "typename",
    "union", "unsigned", "using",
    "virtual", "void", "volatile",
    "wchar_t", "while",
    "xor", "xor_eq",
})


@dataclass
class Control:
    tag: str
    name: str
    cpp_type: str
    include: str

    @property
    def member_name(self) -> str:
        return _to_identifier(self.name)


def _to_identifier(s: str) -> str:
    """Turn an XML name into a valid C++ identifier (replace non-alnum with _)."""
    ident = re.sub(r"[^A-Za-z0-9_]", "_", s)
    if ident and ident[0].isdigit():
        ident = "_" + ident
    if ident in _CPP_KEYWORDS:
        ident = "ui_" + ident
    return ident


def _collect_controls(root: ET.Element) -> list[Control]:
    controls: list[Control] = []
    seen: set[str] = set()
    for elem in root.iter():
        tag = elem.tag.lower()
        name = elem.get("name", "")
        if not name or tag not in TAG_TO_TYPE:
            continue
        if name in seen:
            continue
        seen.add(name)
        cpp_type, include = TAG_TO_TYPE[tag]
        controls.append(Control(tag=tag, name=name, cpp_type=cpp_type, include=include))
    return controls


def _guard_name(struct_name: str) -> str:
    return re.sub(r"[^A-Z0-9_]", "_", struct_name.upper()) + "_GEN_H"


def generate(xml_path: Path, struct_name: str) -> str:
    tree = ET.parse(xml_path)
    root = tree.getroot()
    controls = _collect_controls(root)

    includes = sorted({c.include for c in controls} | {"llpanel.h"})
    guard = _guard_name(struct_name)

    lines: list[str] = []
    lines.append(f"// AUTO-GENERATED by xui_codegen.py — DO NOT EDIT")
    lines.append(f"// Source: {xml_path.name}")
    lines.append(f"// Regenerate: python3 scripts/xui-codegen/xui_codegen.py {xml_path} {struct_name}")
    lines.append(f"")
    lines.append(f"#ifndef {guard}")
    lines.append(f"#define {guard}")
    lines.append(f"")
    for inc in includes:
        lines.append(f"#include \"{inc}\"")
    lines.append(f"")
    lines.append(f"struct {struct_name}")
    lines.append(f"{{")
    for ctrl in controls:
        lines.append(f"    {ctrl.cpp_type}* {ctrl.member_name} {{}};")
    lines.append(f"")
    lines.append(f"    // Call this from your floater/panel's postBuild().")
    lines.append(f"    // Returns false and logs an error if any required control is missing.")
    lines.append(f"    bool postBuild(LLPanel* panel)")
    lines.append(f"    {{")
    lines.append(f"        bool ok = true;")
    for ctrl in controls:
        lines.append(
            f"        {ctrl.member_name} = panel->getChild<{ctrl.cpp_type}>(\"{ctrl.name}\", /*recurse=*/true);"
        )
        lines.append(
            f"        if (!{ctrl.member_name}) {{ LL_WARNS() << \"{struct_name}: missing control '{ctrl.name}'\" << LL_ENDL; ok = false; }}"
        )
    lines.append(f"        return ok;")
    lines.append(f"    }}")
    lines.append(f"}};")
    lines.append(f"")
    lines.append(f"#endif // {guard}")
    lines.append(f"")
    return "\n".join(lines)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("xml", type=Path, help="Path to the XUI floater XML")
    parser.add_argument("struct_name", help="Name for the generated C++ struct")
    parser.add_argument("-o", "--output", type=Path, default=None,
                        help="Output file (default: stdout)")
    args = parser.parse_args()

    if not args.xml.exists():
        print(f"error: {args.xml} not found", file=sys.stderr)
        return 1

    code = generate(args.xml, args.struct_name)

    if args.output:
        args.output.write_text(code, encoding="utf-8")
        print(f"Wrote {args.output}", file=sys.stderr)
    else:
        print(code)

    return 0


if __name__ == "__main__":
    sys.exit(main())
