#!/usr/bin/env python3
"""
Blocks new code from introducing banned legacy C++ patterns.

Checks only lines ADDED in the current commit/PR, not existing code, so
contributors are not blocked by pre-existing violations in files they touch.

Banned patterns (new code must not add these):
  - boost::bind       → use a lambda
  - boost::thread     → use std::jthread
  - LLSingleton<> inheritance → use dependency injection
"""
import os
import re
import subprocess
import sys

BANNED: list[tuple[re.Pattern, str]] = [
    (
        re.compile(r'boost::bind\s*\('),
        'use a lambda instead of boost::bind (C++11)',
    ),
    (
        re.compile(r'\bboost::thread\b'),
        'use std::jthread instead of boost::thread (C++20)',
    ),
    (
        re.compile(r'\bpublic\s+LLSingleton\s*<'),
        'avoid LLSingleton<> — pass dependencies via constructor instead',
    ),
]


def _added_lines(filename: str) -> list[tuple[int, str]]:
    """Return (line_number, text) for every line added by the current staged changes."""
    from_ref = os.environ.get('PRE_COMMIT_FROM_REF', '')
    to_ref   = os.environ.get('PRE_COMMIT_TO_REF',   '')

    if from_ref and to_ref:
        cmd = ['git', 'diff', '--unified=0', f'{from_ref}..{to_ref}', '--', filename]
    else:
        cmd = ['git', 'diff', '--cached', '--unified=0', '--', filename]

    diff = subprocess.run(
        cmd, capture_output=True, text=True, errors='ignore'
    ).stdout

    # If --cached produced nothing (e.g. running in CI with no staged changes
    # or invoked manually), fall back to the last committed diff so the hook
    # still catches violations that landed in HEAD.
    if not diff and not (from_ref and to_ref):
        fallback = ['git', 'diff', '--unified=0', 'HEAD~1..HEAD', '--', filename]
        diff = subprocess.run(
            fallback, capture_output=True, text=True, errors='ignore'
        ).stdout

    result: list[tuple[int, str]] = []
    line_num = 0
    for raw in diff.splitlines():
        if raw.startswith('@@'):
            m = re.search(r'\+(\d+)', raw)
            line_num = int(m.group(1)) - 1 if m else 0
        elif raw.startswith('+') and not raw.startswith('+++'):
            line_num += 1
            result.append((line_num, raw[1:]))
        elif not raw.startswith('-'):
            line_num += 1

    return result


def main(filenames: list[str]) -> int:
    violations: list[str] = []

    for filename in filenames:
        for line_num, text in _added_lines(filename):
            for pattern, message in BANNED:
                if pattern.search(text):
                    violations.append(f'{filename}:{line_num}: {message}')
                    violations.append(f'  + {text.rstrip()}')

    if violations:
        print('New code introduces banned legacy patterns:\n')
        for v in violations:
            print(v)
        print('\nSee CONTRIBUTING.md "New code standards" for alternatives.')
        return 1

    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
