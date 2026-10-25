// HOUZY live demo, audio thread: the plugin's engine (houzy-engine.wasm, built by tools/build-web.sh)
// runs every 128-sample render quantum. The page hands the .wasm bytes in (a worklet cannot fetch)
// and sends knob values over the port; "locked" comes back for full-version-only settings.
import HouzyEngine from './houzy-engine.js?v=20261024a';

// The audio thread has no `performance`; JUCE's clock (emscripten_get_now) reads it. The audio clock will do.
globalThis.performance ??= { now: () => currentTime * 1000 };

class HouzyProcessor extends AudioWorkletProcessor {
  constructor(options) {
    super();
    this.ready = false;
    this.queue = [];
    this.tick = 0;
    const { wasm, host } = options.processorOptions;
    HouzyEngine({ wasmBinary: wasm }).then((M) => {
      this.M = M;
      const h = M.stringToNewUTF8(host);
      const latency = M._houzy_create(sampleRate, h);
      M._free(h);
      this.L = M._houzy_left() >> 2;
      this.R = M._houzy_right() >> 2;
      this.ready = true;
      this.queue.forEach(([id, v]) => this.set(id, v));
      this.queue = [];
      this.port.postMessage({ type: 'ready', latency });
    }).catch((e) => this.port.postMessage({ type: 'error', message: String(e) }));
    this.port.onmessage = (e) => {
      const { id, value } = e.data;
      if (this.ready) this.set(id, value); else this.queue.push([id, value]);
    };
  }

  set(id, value) {
    const p = this.M.stringToNewUTF8(id);
    const r = this.M._houzy_set(p, value);
    this.M._free(p);
    if (r === 1) this.port.postMessage({ type: 'locked', id });
  }

  process(inputs, outputs) {
    const out = outputs[0];
    if (!this.ready || !out || !out[0]) return true;
    const inp = inputs[0], n = out[0].length, H = this.M.HEAPF32;
    const il = inp && inp[0], ir = inp && (inp[1] || inp[0]);
    for (let k = 0; k < n; k++) { H[this.L + k] = il ? il[k] : 0; H[this.R + k] = ir ? ir[k] : 0; }
    this.M._houzy_process(n);
    out[0].set(H.subarray(this.L, this.L + n));
    if (out[1]) out[1].set(H.subarray(this.R, this.R + n));
    // meters + GR graph for the panel, ~30 times a second (houzy_meters: 4 + 5 x 180 floats)
    if (++this.tick % Math.max(1, Math.round(sampleRate / 128 / 30)) === 0) {
      const p = this.M._houzy_meters() >> 2;
      this.port.postMessage({ type: 'meters', m: H.slice(p, p + 904) });
    }
    return true;
  }
}
registerProcessor('houzy', HouzyProcessor);
