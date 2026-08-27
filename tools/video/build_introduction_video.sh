#!/bin/sh

set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
project_root=$(CDPATH= cd -- "$script_dir/../.." && pwd)
asset_dir="$project_root/assets/video/introduction"
output_path=${1:-"$asset_dir/crownless-introduction.mp4"}
ffmpeg_bin=${CROWNLESS_FFMPEG:-/opt/homebrew/opt/ffmpeg-full/bin/ffmpeg}
video_font_file=${CROWNLESS_VIDEO_FONT:-/System/Library/Fonts/SFNSMono.ttf}

if [ ! -x "$ffmpeg_bin" ]; then
    ffmpeg_bin=$(command -v ffmpeg || true)
fi

if [ -z "$ffmpeg_bin" ] || [ ! -x "$ffmpeg_bin" ]; then
    echo "ffmpeg is required to build the introduction video." >&2
    exit 1
fi

if [ -f "$video_font_file" ]; then
    drawtext_font="fontfile='$video_font_file'"
else
    drawtext_font="font='monospace'"
fi

mkdir -p "$(dirname -- "$output_path")"

"$ffmpeg_bin" -hide_banner -y \
    -loop 1 -framerate 24 -t 5.2 -i "$asset_dir/01-human-division.png" \
    -loop 1 -framerate 24 -t 13.6 -i "$asset_dir/03-forced-cooperation.png" \
    -loop 1 -framerate 24 -t 3.2 -i "$asset_dir/02-larger-threat.png" \
    -f lavfi -t 2.48 -i "color=c=090B0C:s=1920x1080:r=24" \
    -i "$asset_dir/the-predator-clause.mp3" \
    -filter_complex "
        [0:v]scale=1920:1080:force_original_aspect_ratio=increase,
             crop=1920:1080,
             zoompan=z='min(zoom+0.00034,1.045)':
                     x='iw/2-(iw/zoom/2)':y='ih/2-(ih/zoom/2)':
                     d=1:s=1920x1080:fps=24,
             settb=AVTB,setpts=PTS-STARTPTS[v0];
        [1:v]scale=1920:1080:force_original_aspect_ratio=increase,
             crop=1920:1080,
             zoompan=z='min(zoom+0.00028,1.04)':
                     x='iw/2-(iw/zoom/2)':y='ih/2-(ih/zoom/2)':
                     d=1:s=1920x1080:fps=24,
             settb=AVTB,setpts=PTS-STARTPTS[v1];
        [2:v]scale=1920:1080:force_original_aspect_ratio=increase,
             crop=1920:1080,
             zoompan=z='min(zoom+0.00018,1.04)':
                     x='iw/2-(iw/zoom/2)':y='ih/2-(ih/zoom/2)':
                     d=1:s=1920x1080:fps=24,
             settb=AVTB,setpts=PTS-STARTPTS[v2];
        [3:v]format=yuv420p,settb=AVTB,setpts=PTS-STARTPTS[v3];
        [v0][v1]xfade=transition=fade:duration=0.4:offset=4.8[v01];
        [v01][v2]xfade=transition=fade:duration=0.4:offset=18.0[v012];
        [v012][v3]xfade=transition=fade:duration=0.4:offset=20.8[vbase];
        [vbase]drawtext=$drawtext_font:
                  textfile='$asset_dir/sentence-1.txt':
                  fontcolor=F2E9D0:fontsize=46:line_spacing=10:
                  text_align=C:x=(w-text_w)/2:y=h-170:
                  box=1:boxcolor=090B0C@0.72:boxborderw=22:
                  borderw=1:bordercolor=000000@0.9:
                  enable='between(t,0.15,4.9)',
              drawtext=$drawtext_font:
                  textfile='$asset_dir/sentence-2.txt':
                  fontcolor=F2E9D0:fontsize=46:line_spacing=10:
                  text_align=C:x=(w-text_w)/2:y=h-170:
                  box=1:boxcolor=090B0C@0.72:boxborderw=22:
                  borderw=1:bordercolor=000000@0.9:
                  enable='between(t,5.4,13.0)',
              drawtext=$drawtext_font:
                  textfile='$asset_dir/sentence-3.txt':
                  fontcolor=F2E9D0:fontsize=46:line_spacing=10:
                  text_align=C:x=(w-text_w)/2:y=h-170:
                  box=1:boxcolor=090B0C@0.72:boxborderw=22:
                  borderw=1:bordercolor=000000@0.9:
                  enable='between(t,13.8,17.9)',
              drawtext=$drawtext_font:
                  textfile='$asset_dir/title.txt':
                  fontcolor=F2E9D0:fontsize=104:
                  text_align=C:x=(w-text_w)/2:y=(h-text_h)/2:
                  borderw=3:bordercolor=080A0B@0.95:
                  shadowx=5:shadowy=6:shadowcolor=000000@0.7:
                  alpha='if(lt(t,21.4),(t-20.8)/0.6,if(lt(t,22.88),1,(23.28-t)/0.4))':
                  enable='between(t,20.8,23.28)',
              format=yuv420p[vout];
        [4:a:0]aresample=48000,atrim=duration=23.28,
             asetpts=PTS-STARTPTS[aout]
    " \
    -map "[vout]" -map "[aout]" \
    -t 23.28 -r 24 \
    -c:v libx264 -preset slow -crf 18 -pix_fmt yuv420p \
    -c:a aac -b:a 192k -ar 48000 \
    -movflags +faststart \
    -metadata title="Crownless Carriage Introduction" \
    -metadata artist="ratimics" \
    -metadata comment="Audio: The Predator Clause" \
    "$output_path"
