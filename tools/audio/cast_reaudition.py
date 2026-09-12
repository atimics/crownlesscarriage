#!/usr/bin/env python3
"""Generate six revised cast auditions and a Pocket before-and-after page."""
import argparse
import hashlib
import html
import importlib.metadata
import json
from pathlib import Path
import shutil
import time

ROOT = Path(__file__).resolve().parents[2]
BRIEF = Path(__file__).with_suffix('.json')


def pocket(brief, output):
    import numpy as np
    import torch
    from pocket_tts import TTSModel
    from scipy.io.wavfile import write
    from voice_style import render_voice
    torch.set_num_threads(2)
    model = TTSModel.load_model(language='english')
    rows = []
    report = dict(model='Pocket TTS', package_version=importlib.metadata.version('pocket-tts'),
                  torch_version=torch.__version__, sample_rate=model.sample_rate,
                  brief_sha256=hashlib.sha256(BRIEF.read_bytes()).hexdigest(), samples=rows)
    folder = output / 'pocket'
    folder.mkdir(exist_ok=True)
    for voice in brief['voices']:
        for take in ('original', 'revised'):
            reference = (ROOT / 'assets/audio/cast' / (voice['original'] + '.wav') if take == 'original'
                         else output / 'revised' / (voice['id'] + '.wav'))
            state = model.get_state_for_audio_prompt(str(reference))
            for line, words in brief['lines'].items():
                torch.manual_seed(20260912)
                np.random.seed(20260912)
                start = time.perf_counter()
                with torch.no_grad():
                    samples = model.generate_audio(state, words).cpu().numpy().reshape(-1)
                seconds = time.perf_counter() - start
                duration = len(samples) / model.sample_rate
                if not np.isfinite(samples).all() or not 0.15 <= duration <= 25:
                    raise ValueError('Audio needs finite samples and a duration within 0.15–25 seconds.')
                peak = float(np.max(np.abs(samples)))
                if peak < 0.001:
                    raise ValueError('Audio needs an audible signal.')
                samples *= min(1.0, 0.82 / peak)
                path = folder / f'{voice["id"]}-{take}-{line}.wav'
                write(path, model.sample_rate, (samples * 32767).astype(np.int16))
                row = dict(voice=voice['name'], take=take, line=line, text=words, seed=20260912,
                           file=path.name, seconds=duration, generation_seconds=seconds,
                           reference_sha256=hashlib.sha256(reference.read_bytes()).hexdigest(),
                           sha256=hashlib.sha256(path.read_bytes()).hexdigest())
                styled = path.with_name(path.stem + '-game.wav')
                render_voice(path, styled, row)
                row['game_file'] = styled.name
                rows.append(row)
                (output / 'pocket.json').write_text(json.dumps(report, indent=2) + '\n')
                print(f'{voice["name"]} {take} {line}: {duration:.2f}s audio', flush=True)


def build_page(brief, output):
    def player(folder, name, label):
        path = output / folder / name
        if not path.is_file():
            path = path.with_suffix('.mp3')
        if not path.is_file():
            return '<p>Audition is being prepared.</p>'
        return f'<label>{html.escape(label)}<audio controls preload="metadata" src="{html.escape(str(path.relative_to(output)))}"></audio></label>'

    originals = output / 'originals'
    originals.mkdir(exist_ok=True)
    cards = []
    for voice in brief['voices']:
        shutil.copy2(ROOT / 'assets/audio/cast' / (voice['original'] + '.wav'), originals / (voice['original'] + '.wav'))
        pairs = ''
        for line, words in brief['lines'].items():
            columns = ''
            for take in ('original', 'revised'):
                prefix = f'{voice["id"]}-{take}-{line}'
                columns += '<div>' + player('pocket', prefix + '.wav', take.title() + ' · Pocket')
                columns += player('pocket', prefix + '-game.wav', take.title() + ' · game texture') + '</div>'
            pairs += f'<h4>{html.escape(words)}</h4><div class="pair">{columns}</div>'
        cards.append(f'<article id="{voice["id"]}"><h2>{html.escape(voice["name"])}</h2><p class="direction">{html.escape(voice["direction"])}</p>'
                     '<details><summary>Compare voice references</summary><div class="pair">'
                     + player('originals', voice['original'] + '.wav', 'Original · Qwen reference')
                     + player('revised', voice['id'] + '.wav', 'Revised · Qwen reference')
                     + '</div></details>' + pairs + '</article>')
    nav = ' '.join(f'<a href="#{v["id"]}">{html.escape(v["name"])}</a>' for v in brief['voices'])
    page = '''<!doctype html><html lang="en"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Crownless · Six fresh voices</title><style>
:root{color-scheme:dark;background:#17231f;color:#f6eddc;font-family:system-ui,sans-serif}*{box-sizing:border-box}body{margin:0}
main{max-width:1100px;margin:auto;padding:40px 24px 80px}h1,h2{font-family:Georgia,serif;font-weight:400}h1{font-size:clamp(42px,7vw,70px);margin:12px 0}h2{font-size:34px;margin:0 0 12px}
p{line-height:1.65;max-width:850px}.eyebrow{color:#dabb7d;text-transform:uppercase;letter-spacing:.08em;font-size:12px}nav{display:flex;gap:22px;flex-wrap:wrap;margin:24px 0}a,.direction{color:#f0d7a4}
article{background:#21332b;border-radius:10px;padding:28px;margin:24px 0;scroll-margin-top:20px}.pair{display:grid;grid-template-columns:1fr 1fr;gap:28px}h4{font-weight:500;line-height:1.6;margin:28px 0 16px}audio{display:block;width:100%;height:38px;margin:10px 0 16px}label,summary{font-size:14px}summary{cursor:pointer;color:#bfd0be}details{margin:18px 0}details[open] summary{margin-bottom:20px}@media(max-width:650px){.pair{grid-template-columns:1fr;gap:12px}article{padding:20px}}
</style><main><header><div class="eyebrow">Crownless Carriage · Second audition</div><h1>Six fresh voices</h1>
<p>Hearth, Flint, Lark, Brook, Reed, and Oak each get a stronger identity. Compare the original and revised voices reading the same words through Pocket. Hear each clean take and its game texture.</p>
<p>The new voices are synthetic auditions made with Qwen VoiceDesign. The directions describe the intended performance. Listening decides how well each take meets it.</p><nav>NAV</nav></header>CARDS
<p>These six replace about half of the original thirteen in the proposed cast. Mara, Jory, Tomas, Ilyra, Bren, Ash, and Stone remain the familiar anchors. Cedar, Wren, and Copper remain the three additions from the first audition.</p>
</main><script>document.addEventListener('play',e=>{if(e.target.tagName==='AUDIO')document.querySelectorAll('audio').forEach(a=>{if(a!==e.target)a.pause()})},true)</script></html>'''
    (output / 'index.html').write_text(page.replace('NAV', nav).replace('CARDS', ''.join(cards)))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', required=True, type=Path)
    parser.add_argument('--design', action='store_true')
    parser.add_argument('--allow-download', action='store_true')
    parser.add_argument('--pocket', action='store_true')
    parser.add_argument('--compact', action='store_true')
    parser.add_argument('--device', choices=('cpu', 'mps'), default='cpu')
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    brief = json.loads(BRIEF.read_text())
    if args.design:
        from speech_engine import design_cast
        design_cast({v['id']: v for v in brief['voices']}, args.output / 'revised',
                    device=args.device, allow_download=args.allow_download)
    if args.pocket:
        pocket(brief, args.output)
    if args.compact:
        from casting_review import compress_review
        compress_review(args.output, ('revised', 'pocket'))
    build_page(brief, args.output)
    print(args.output / 'index.html')


if __name__ == '__main__':
    main()
