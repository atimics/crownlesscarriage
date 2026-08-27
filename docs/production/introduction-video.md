# Introduction Video

The finished 25-second introduction is
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
text beat appears only after its matching phrase has been spoken. Give "would
eat them if they didn't" a separate text-only slide. Then use black pauses to
separate the title and dragon reveals.

## Sequence

1. Begin the voice over two divided groups.
2. Show the delayed first text and hold it longer.
3. Show the delayed second text and hold it longer.
4. Cut to a separate "would eat them if they didn't" slide.
5. Cut to black.
6. Show the `CROWNLESS` title card.
7. Cut to black.
8. Show the dragon.
9. End on black.

The supplied track plays in full. The short silence after it ends carries the
dragon into the final black frame.

Rebuild the video with `tools/video/build_introduction_video.sh`. Set
`CROWNLESS_FFMPEG` if FFmpeg is installed at a different path.
