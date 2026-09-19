"""Check account patterns against literal simulation message formats."""
import ast
from pathlib import Path
import re

STRING = r'"(?:\\.|[^"\\])*"'
CHAIN = re.compile(STRING + r'(?:\s*(?:PRI[douxX]\d+\s*)?' + STRING + r')*')
PRINTF = re.compile(r'%%|%[-+ #0]*\d*(?:\.\d+)?(?:ll|l|z)?[sdiu]')


def literal_formats(root):
    paths = sorted((root / 'src/sim').glob('*.c')) + sorted((root / 'src/sim').glob('*.inc'))
    paths += [root / 'tests/fixtures/legacy_event_formats.c']
    result = []
    for path in paths:
        if not path.exists():
            continue
        text = path.read_text()
        for match in CHAIN.finditer(text):
            parts = re.findall(STRING + r'|PRI[douxX]\d+', match.group())
            try:
                value = ''.join('d' if p.startswith('PRI') else ast.literal_eval(p) for p in parts)
            except (ValueError, SyntaxError):
                continue
            if not isinstance(value, str) or len(value) < 8:
                continue
            result.append((str(path.relative_to(root)), value))
    return result


def accepts_template(fmt, rule):
    """A literal branch may fill a string argument; number slots stay numeric."""
    chunks = []
    end = 0
    for match in PRINTF.finditer(fmt):
        chunks.append(re.escape(fmt[end:match.start()]))
        if match.group() == '%%':
            chunks.append('%')
        elif match.group().endswith('s'):
            chunks.append('.*?')
        else:
            numeric = [r'\{' + str(i) + r'\}' for i, role in enumerate(rule['roles']) if role == 'quantity']
            chunks.append('(?:[0-9]+|' + '|'.join(numeric) + ')' if numeric else '[0-9]+')
        end = match.end()
    chunks.append(re.escape(fmt[end:]))
    # A bare string insertion cannot establish a reviewed event format.
    fixed = PRINTF.sub('', fmt)
    if len(fixed.strip()) < 8:
        return False
    if re.fullmatch(''.join(chunks), rule['source']) is not None:
        return True
    # Some emitters contain fixed names and small counts that the account
    # parser captures as fields, such as "one cow" or "14 nights".
    sample = PRINTF.sub(lambda m: '%' if m.group() == '%%' else
                        ('s' if m.start() and fmt[m.start()-1].isalnum() else 'Example')
                        if m.group().endswith('s') else '7', fmt)
    parts = []
    end = 0
    for slot in re.finditer(r'\{(\d+)\}', rule['source']):
        parts.append(re.escape(rule['source'][end:slot.start()]))
        role = rule['roles'][int(slot[1])]
        parts.append(r'(?:\d+|zero|one|two|three|four|five|six|seven|eight|nine|ten|eleven|twelve)'
                     if role == 'quantity' else '.+?')
        end = slot.end()
    parts.append(re.escape(rule['source'][end:]))
    return re.fullmatch(''.join(parts), sample) is not None


def ungrounded_rules(root, rules):
    formats = literal_formats(root)
    return [r['id'] for r in rules if not any(accepts_template(fmt, r) for _, fmt in formats)]
