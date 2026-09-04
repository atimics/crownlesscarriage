#!/usr/bin/env python3

from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

ROOT=Path(__file__).resolve().parents[2]
FRAMES_DIR=Path("/tmp/crownless_hero_action_frames")
OUTPUT_DIR=ROOT/"assets"/"previews"/"hero"/"actions"
FONT_PATH="/System/Library/Fonts/Supplemental/Arial Bold.ttf"
ACTIONS=(("walk","WALK",1,48),("jump","JUMP",49,96),("climb","CLIMB",97,144),
         ("swim","SWIM",145,192),("fight","FIGHT",193,240))

def font(size:int):
    try: return ImageFont.truetype(FONT_PATH,size)
    except OSError: return ImageFont.load_default()

def labeled(path:Path,label:str)->Image.Image:
    source=Image.open(path).convert("RGB")
    canvas=Image.new("RGB",(512,560),"#11161a"); canvas.paste(source,(0,48))
    ImageDraw.Draw(canvas).text((20,10),label,fill="#f1eadc",font=font(27))
    return canvas.quantize(colors=128,method=Image.Quantize.MEDIANCUT)

OUTPUT_DIR.mkdir(parents=True,exist_ok=True)
reel=[]
for slug,label,start,end in ACTIONS:
    frames=[labeled(FRAMES_DIR/f"action_{frame:04d}.png",label) for frame in range(start,end+1,2)]
    if len(frames)!=24: raise RuntimeError(f"Expected 24 sampled frames for {slug}")
    frames[0].save(OUTPUT_DIR/f"hero_{slug}_v02.gif",save_all=True,append_images=frames[1:],
                   duration=83,loop=0,optimize=True,disposal=2)
    reel.extend(frames)
reel[0].save(OUTPUT_DIR/"hero_action_reel_v02.gif",save_all=True,append_images=reel[1:],
             duration=83,loop=0,optimize=True,disposal=2)
print(f"Encoded five action loops and {len(reel)} reel frames to {OUTPUT_DIR}")
