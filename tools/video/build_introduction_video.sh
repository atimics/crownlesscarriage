#!/bin/sh

set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
project_root=$(CDPATH= cd -- "$script_dir/../.." && pwd)
asset_dir="$project_root/assets/video/introduction"
output_path=${1:-"$asset_dir/crownless.mp4"}
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
    -loop 1 -framerate 24 -t 8.0 -i "$asset_dir/01-human-division.png" \
    -loop 1 -framerate 24 -t 10.4 -i "$asset_dir/03-forced-cooperation.png" \
    -f lavfi -t 2.0 -i "color=c=090B0C:s=1920x1080:r=24" \
    -f lavfi -t 0.6 -i "color=c=090B0C:s=1920x1080:r=24" \
    -loop 1 -framerate 24 -t 1.4 -i "$asset_dir/02-larger-threat.png" \
    -f lavfi -t 0.4 -i "color=c=090B0C:s=1920x1080:r=24" \
    -f lavfi -t 1.5 -i "color=c=090B0C:s=1920x1080:r=24" \
    -f lavfi -t 0.7 -i "color=c=090B0C:s=1920x1080:r=24" \
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
        [2:v]format=yuv420p,settb=AVTB,setpts=PTS-STARTPTS[v2];
        [3:v]format=yuv420p,settb=AVTB,setpts=PTS-STARTPTS[v3];
        [4:v]scale=1920:1080:force_original_aspect_ratio=increase,
             crop=1920:1080,
             zoompan=z='min(zoom+0.00018,1.04)':
                     x='iw/2-(iw/zoom/2)':y='ih/2-(ih/zoom/2)':
                     d=1:s=1920x1080:fps=24,
             settb=AVTB,setpts=PTS-STARTPTS[v4];
        [5:v]format=yuv420p,settb=AVTB,setpts=PTS-STARTPTS[v5];
        [6:v]format=yuv420p,settb=AVTB,setpts=PTS-STARTPTS[v6];
        [7:v]format=yuv420p,settb=AVTB,setpts=PTS-STARTPTS[v7];
        [v0][v1][v2][v3][v4][v5][v6][v7]concat=n=8:v=1:a=0[vbase];
        [vbase]drawtext=$drawtext_font:
                  textfile='$asset_dir/sentence-1.txt':
                  fontcolor=F2E9D0:fontsize=46:line_spacing=10:
                  text_align=C:x=(w-text_w)/2:y=h-170:
                  box=1:boxcolor=090B0C@0.72:boxborderw=22:
                  borderw=1:bordercolor=000000@0.9:
                  enable='between(t,6.05,14.05)',
              drawtext=$drawtext_font:
                  textfile='$asset_dir/sentence-2.txt':
                  fontcolor=F2E9D0:fontsize=46:line_spacing=10:
                  text_align=C:x=(w-text_w)/2:y=h-170:
                  box=1:boxcolor=090B0C@0.72:boxborderw=22:
                  borderw=1:bordercolor=000000@0.9:
                  enable='between(t,14.05,18.4)',
              drawtext=$drawtext_font:
                  textfile='$asset_dir/sentence-3.txt':
                  fontcolor=F2E9D0:fontsize=64:line_spacing=10:
                  text_align=C:x=(w-text_w)/2:y=(h-text_h)/2:
                  borderw=2:bordercolor=000000@0.9:
                  shadowx=4:shadowy=5:shadowcolor=000000@0.7:
                  enable='between(t,18.4,20.4)',
              drawtext=$drawtext_font:
                  textfile='$asset_dir/title.txt':
                  fontcolor=F2E9D0:fontsize=104:
                  text_align=C:x=(w-text_w)/2:y=(h-text_h)/2:
                  borderw=3:bordercolor=080A0B@0.95:
                  shadowx=5:shadowy=6:shadowcolor=000000@0.7:
                  alpha='if(lt(t,23.1),(t-22.8)/0.3,if(lt(t,24.0),1,(24.3-t)/0.3))':
                  enable='between(t,22.8,24.3)',
              format=yuv420p[vout];
        [8:a:0]aresample=48000,apad=pad_dur=1.72,atrim=duration=25,
             asetpts=PTS-STARTPTS[aout]
    " \
    -map "[vout]" -map "[aout]" \
    -t 25 -r 24 \
    -c:v libx264 -preset slow -crf 18 -pix_fmt yuv420p \
    -c:a aac -b:a 192k -ar 48000 \
    -movflags +faststart \
    -metadata title="Crownless" \
    -metadata artist="ratimics" \
    -metadata comment="Audio: The Predator Clause" \
    "$output_path"
