# Introduction Video

The finished 23-second introduction is
[`crownless.mp4`](../../assets/video/introduction/crownless.mp4).
English captions are available beside it as
[`crownless.en.srt`](../../assets/video/introduction/crownless.en.srt).
It uses the supplied track
[`the-predator-clause.mp3`](../../assets/video/introduction/the-predator-clause.mp3)
for its complete audio mix.

## Required narration

The introduction video must include this line exactly:

> Humans don't cooperate because they're good. They cooperate because something
> bigger than any of them would eat them if they didn't.

Use it as the video's opening thesis. Do not put text ahead of the voice. Each
text beat appears only after its matching phrase has been spoken. Give "eat
them if they didn't" a separate text-only slide, then reveal the dragon and cut
to the title card.

## Sequence

1. Two groups hold apart while the first phrase is spoken.
2. Show the first text beat after the phrase ends.
3. The groups repair the crossing while the next phrases are spoken.
4. Show the second text beat after "any of them."
5. Cut to a separate "eat them if they didn't" slide after the voice ends.
6. Show the dragon.
7. Cut to the `CROWNLESS` title card.

Rebuild the video with `tools/video/build_introduction_video.sh`. Set
`CROWNLESS_FFMPEG` if FFmpeg is installed at a different path.
