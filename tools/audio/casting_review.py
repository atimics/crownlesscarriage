#!/usr/bin/env python3
"""Build a portable listening page from local cast references and trial reports."""

import argparse
import html
import json
from pathlib import Path
import shutil

ROOT = Path(__file__).resolve().parents[2]


def esc(value):
    return html.escape(str(value), quote=True)


def audio(path, label):
    return f'<label>{esc(label)}<audio controls preload="none" src="{esc(path)}"></audio></label>'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    refs = args.output / 'references'
    refs.mkdir(exist_ok=True)
    cast = json.loads((ROOT / 'assets/audio/cast.json').read_text())
    brief = json.loads(Path(__file__).with_name('casting_trial.json').read_text())
    cards = []
    for voice in cast:
        filename = voice['id'] + '.wav'
        shutil.copy2(ROOT / 'assets/audio/cast' / filename, refs / filename)
        cards.append(f'<article><span class="tag">Existing reference</span><h3>{esc(voice["name"])}</h3>'
                     f'<p>{esc(voice["description"])}</p>'
                     f'<p class="direction">{esc(brief["review_directions"][voice["id"]])}</p>'
                     + audio('references/' + filename, 'Listen to reference') + '</article>')
    additions = []
    for voice in brief['proposed']:
        path = args.output / 'proposed' / (voice['id'] + '.wav')
        player = audio('proposed/' + path.name, 'Listen to audition') if path.is_file() else '<p class="pending">Audio audition pending</p>'
        additions.append(f'<article><span class="tag">Proposed addition · casting direction</span>'
                         f'<h3>{esc(voice["name"])}</h3><p>{esc(voice["description"])}</p>'
                         f'<p class="direction">{esc(voice["purpose"])}</p>{player}</article>')
    trials = []
    metrics = []
    reports = {}
    for engine in ('pocket', 'nano'):
        path = args.output / 'trial' / f'{engine}.json'
        if path.is_file():
            reports[engine] = json.loads(path.read_text())
    for engine, report in reports.items():
        rows = report['samples']
        total_audio = sum(r['seconds'] for r in rows)
        total_time = sum(r['generation_seconds'] for r in rows)
        speed = f'{total_time / total_audio:.2f}s compute / 1s audio' if total_audio else 'Pending'
        chunks = [r['first_chunk_seconds'] for r in rows if r['first_chunk_seconds'] is not None]
        start = f'{min(chunks):.2f}–{max(chunks):.2f}s first chunk' if chunks else 'Complete clips'
        error = '<p>Generation needs attention. Details are in the trial report.</p>' if report.get('error') else ''
        if 'VOICE_CLONING' in report.get('error', '') or 'accept the terms' in report.get('error', ''):
            error = '<p>Voice cloning awaits Hugging Face model access.</p>'
        metrics.append(f'<article><h3>{esc(engine.title())}</h3><p>{esc(report["status"])} · {len(rows)} clips</p>'
                       f'<p><strong>{esc(speed)}</strong><br>{esc(start)}</p>{error}</article>')
    keys = sorted({(r['voice'], r['line']) for report in reports.values() for r in report['samples']})
    for voice, line in keys:
        cells = []
        words = ''
        for engine, report in reports.items():
            row = next((r for r in report['samples'] if (r['voice'], r['line']) == (voice, line)), None)
            if row:
                words = row['text']
                cells.append(f'<div><h4>{esc(engine.title())}</h4>' + audio('trial/' + row['file'], 'Clean')
                             + audio('trial/' + row['styled_file'], 'Game texture')
                             + f'<small>{row["generation_seconds"]:.2f}s generation · {row["seconds"]:.2f}s audio</small></div>')
        trials.append(f'<article class="comparison"><h3>{esc(voice)} · {esc(line)}</h3>'
                      f'<p class="quote">{esc(words)}</p><div class="grid">{"".join(cells)}</div></article>')
    page = '''<!doctype html><html lang="en"><meta charset="utf-8"><meta name="viewport" content="width=device-width, initial-scale=1">
<title>Crownless · The casting room</title><style>
:root{color-scheme:dark;font-family:system-ui,sans-serif;background:#17231f;color:#f6eddc}*{box-sizing:border-box}
body{margin:0}main{max-width:1180px;margin:auto;padding:48px 24px 80px}h1{font-family:Georgia,serif;font-size:clamp(40px,7vw,72px);font-weight:400;line-height:1.05;margin:16px 0 24px}
h2{font-family:Georgia,serif;font-size:32px;font-weight:400;margin-top:64px}h3{margin:12px 0;font-size:21px}h4{margin:10px 0}
p{line-height:1.65}header p{max-width:780px;font-size:18px}.eyebrow,.tag{color:#dabb7d;font-size:12px;letter-spacing:.07em;text-transform:uppercase}
.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(min(100%,290px),1fr));gap:16px}article{background:#21332b;padding:24px;border-radius:10px}
.direction,small{color:#bccbbb}.direction{font-size:14px}audio{display:block;width:100%;height:38px;margin:8px 0 16px}label{display:block;font-size:13px}
.quote{color:#f0d7a4}.comparison{margin:16px 0}.comparison .grid{gap:32px}.pending{color:#dabb7d}a{color:#f0d7a4}nav{display:flex;gap:24px;flex-wrap:wrap}
.note{border-left:3px solid #dabb7d;padding:0 20px;max-width:850px}li{line-height:1.7;margin:8px 0}
</style><main><header><div class="eyebrow">Crownless Carriage · Listening trial · 12 September 2026</div>
<h1>The casting room</h1><p>Thirteen familiar voices. Three new directions. Hear the people first, then compare the speech engines using the same words.</p>
<nav><a href="#cast">Current cast</a><a href="#additions">Three additions</a><a href="#trial">Model trial</a><a href="#review">What to listen for</a></nav></header>
<section id="cast"><h2>Meet the current cast</h2><p>These are the saved synthetic voice references made with Qwen VoiceDesign. Every voice reads the same passage. Descriptions are casting intentions; listening establishes what each recording conveys.</p>
<p class="quote">REFERENCE_TEXT</p><div class="grid">CAST_CARDS</div></section>
<section id="additions"><h2>Room for three more</h2><p>A proposed cast of sixteen: five named people and eleven reusable voices. These additions broaden age, rhythm, gender presentation, and accent. The accents are audition directions. Each voice can belong to a neighbour, leader, traveller, or opponent.</p>
<div class="grid">ADDITION_CARDS</div></section>
<section id="trial"><h2>Same people, same words</h2><p>Four contrasting voices each read a welcome, an exact trade quote, and an urgent warning. Compare the clean clip with the game texture. The text and references are identical across engines.</p>
<div class="grid">METRICS</div><p class="note">Measured local CPU trial with two Torch threads per engine. Generation times include each model’s first speech call. Voice setup and model loading are separate. First chunk measures the model output; game playback timing and frame rate need a game integration test. Samples still need listening review.</p>TRIAL_CARDS</section>
<section id="review"><h2>Choose people you want to hear again</h2><ol><li>Can you recognise the person across all three lines?</li><li>Are names, quantities, and prices easy to understand?</li><li>Does the warning carry urgency while keeping the same voice?</li><li>Can this voice carry humour, care, authority, and doubt?</li><li>Which voices sound too similar after the game texture?</li></ol>
<p>Keep a voice, request another take, or change its casting direction. Casting descriptions and age targets remain proposals until listening review. Human and creature performance auditions can follow this core cast decision.</p></section></main>
<script>document.addEventListener('play',event=>{if(event.target.tagName==='AUDIO')document.querySelectorAll('audio').forEach(a=>{if(a!==event.target)a.pause()})},true)</script></html>'''
    for key, value in {'REFERENCE_TEXT': esc(brief['reference_text']), 'CAST_CARDS': ''.join(cards),
                       'ADDITION_CARDS': ''.join(additions), 'METRICS': ''.join(metrics),
                       'TRIAL_CARDS': ''.join(trials)}.items():
        page = page.replace(key, value)
    (args.output / 'index.html').write_text(page)
    print(args.output / 'index.html')


if __name__ == '__main__':
    main()
