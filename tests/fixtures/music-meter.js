class MusicMeter extends AudioWorkletProcessor {
  constructor() {
    super();
    this.counting = false;
    this.frames = 0;
    this.silent = 0;
    this.longest = 0;
    this.port.onmessage = ({ data }) => {
      if (data === 'start') {
        this.frames = this.silent = this.longest = 0;
        this.counting = true;
        this.port.postMessage({ started: true });
      } else if (data === 'report') {
        this.counting = false;
        this.port.postMessage({ frames: this.frames, longestSilence: this.longest });
      }
    };
  }
  process(inputs, outputs) {
    const input = inputs[0]?.[0];
    if (this.counting) {
      const length = outputs[0][0].length;
      for (let i = 0; i < length; ++i) {
        this.frames++;
        this.silent = input && Math.abs(input[i]) > 0.00001 ? 0 : this.silent + 1;
        this.longest = Math.max(this.longest, this.silent);
      }
    }
    return true;
  }
}
registerProcessor('music-meter', MusicMeter);
