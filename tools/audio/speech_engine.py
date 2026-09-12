"""Local speech generation using saved cast references."""

import hashlib
import os
from pathlib import Path
import tempfile
import wave

from speech_format import ROOT, check_wav
from voice_style import render_voice


class SpeechEngine:
    def __init__(self, device='cpu', references=ROOT / 'assets/audio/cast', engine='pocket', allow_download=False, take=0, cfg_weight=0.5):
        if not allow_download:
            os.environ['HF_HUB_OFFLINE'] = '1'
            os.environ['TRANSFORMERS_OFFLINE'] = '1'
        import torch
        self.torch = torch
        self.references = Path(references)
        self.engine = engine
        self.prompts = {}
        self.take = take
        self.cfg_weight = cfg_weight
        if engine != 'pocket':
            raise ValueError('Crownless speech uses Pocket TTS')
        from pocket_tts import TTSModel
        torch.set_num_threads(2)
        self.model = TTSModel.load_model(language='english')

    def __call__(self, record, destination):
        import json
        import numpy as np
        import soundfile as sf
        reference = self.references / (record['voice'] + '.wav')
        if not reference.is_file():
            raise ValueError(f"Prepare the reference for {record['voice']}")
        seed = (int(record['key'][:8], 16) + self.take) & 0xffffffff
        self.torch.manual_seed(seed)
        with self.torch.no_grad():
            if record['voice'] not in self.prompts:
                self.prompts[record['voice']] = self.model.get_state_for_audio_prompt(str(reference))
            chunks = list(self.model.generate_audio_stream(self.prompts[record['voice']], record['text']))
            if not chunks:
                raise ValueError('Pocket returned an empty speech stream')
            samples = self.torch.cat(chunks).detach().float().cpu().numpy().reshape(-1)
            rate = self.model.sample_rate
        if rate != 24000 or not np.isfinite(samples).all() or not 0.15 <= len(samples) / rate <= 25:
            raise ValueError('The model returned invalid speech samples')
        peak = float(np.max(np.abs(samples)))
        if peak < 0.001:
            raise ValueError('The model returned silent speech')
        samples = samples * min(1.0, 0.82 / peak)
        destination = Path(destination)
        destination.parent.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(dir=destination.parent) as temporary:
            master = Path(temporary) / 'master.wav'
            rendered = Path(temporary) / 'speech.wav'
            sf.write(master, samples, rate, subtype='PCM_16')
            receipt = dict(record, model=self.engine, seed=seed, take=self.take, cfg_weight=self.cfg_weight,
                reference_sha256=hashlib.sha256(reference.read_bytes()).hexdigest())
            if record['voice'] == 'goblin-v1':
                from goblin_voice import render as render_goblin
                effect = render_goblin(master, rendered, 'bass')
                rendered.with_suffix('.json').write_text(json.dumps(dict(receipt, **effect), indent=2) + '\n')
            else:
                render_voice(master, rendered, receipt)
            check_wav(rendered)
            rendered.replace(destination)
            rendered.with_suffix('.json').replace(destination.with_suffix('.json'))


def design_cast(cast, destination, device='cpu', allow_download=False):
    if not allow_download:
        os.environ['HF_HUB_OFFLINE'] = '1'
        os.environ['TRANSFORMERS_OFFLINE'] = '1'
    import json
    import torch
    import soundfile as sf
    from qwen_tts import Qwen3TTSModel
    model = Qwen3TTSModel.from_pretrained('Qwen/Qwen3-TTS-12Hz-1.7B-VoiceDesign',
        device_map=device, dtype=torch.float32 if device != 'cuda' else torch.bfloat16,
        attn_implementation='sdpa')
    destination = Path(destination)
    destination.mkdir(parents=True, exist_ok=True)
    text = 'The road is quiet this morning. Come inside, and tell me where you are going. We can find a place for your horses.'
    for voice in cast.values():
        path = destination / (voice['id'] + '.wav')
        if path.exists():
            check_wav(path)
            continue
        seed = int(hashlib.sha256(voice['id'].encode()).hexdigest()[:8], 16)
        torch.manual_seed(seed)
        wavs, rate = model.generate_voice_design(text=text, language='English',
            instruct=voice['description'] + ' Clean studio recording.', max_new_tokens=400)
        with tempfile.TemporaryDirectory(dir=destination) as temporary:
            clip = Path(temporary) / 'reference.wav'
            sf.write(clip, wavs[0], rate, subtype='PCM_16')
            check_wav(clip)
            clip.replace(path)
        path.with_suffix('.json').write_text(json.dumps(dict(voice, text=text, seed=seed,
            model='Qwen/Qwen3-TTS-12Hz-1.7B-VoiceDesign', sample_rate=rate,
            sha256=hashlib.sha256(path.read_bytes()).hexdigest()), indent=2) + '\n')
        print(f"Designed {voice['id']}", flush=True)
