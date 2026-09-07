/* The immediate mode batch re-sends its normals and texture coordinates every
   flush, unchanged, dozens of times a frame. A browser pays for each upload
   whether or not the bytes differ, and the allocator serving them keeps the
   high water mark it reaches. Keeping a copy of what each vertex buffer
   already holds lets an unchanged range skip the trip.

   Only ARRAY_BUFFER is tracked. Its binding is context state rather than
   vertex array state, so it can be followed from bindBuffer alone, and it is
   where every repeated upload lands. */
(function () {
  if (typeof WebGL2RenderingContext === 'undefined') return;
  const prototype = WebGL2RenderingContext.prototype;
  const shadowLimit = 1024 * 1024;
  const shadows = new WeakMap();
  const arrayBuffer = new WeakMap();
  const counts = window.crownlessUploads = {sent: 0, skipped: 0, skippedBytes: 0};
  let watching = true;

  function bytes(source, offset, length) {
    if (!source || !source.buffer) return null;
    const scale = source.BYTES_PER_ELEMENT || 1;
    const start = source.byteOffset + (offset || 0) * scale;
    const size = length !== undefined ? length * scale : source.byteLength - (offset || 0) * scale;
    if (size < 0 || start + size > source.buffer.byteLength) return null;
    return new Uint8Array(source.buffer, start, size);
  }

  /* Whole words where the alignment allows it; these buffers hold floats. */
  function same(held, offset, sent) {
    if (((offset | held.byteOffset | sent.byteOffset | sent.byteLength) & 3) === 0) {
      const a = new Uint32Array(held.buffer, held.byteOffset + offset, sent.byteLength >> 2);
      const b = new Uint32Array(sent.buffer, sent.byteOffset, sent.byteLength >> 2);
      for (let i = 0; i < a.length; i++) if (a[i] !== b[i]) return false;
      return true;
    }
    for (let i = 0; i < sent.length; i++) if (held[offset + i] !== sent[i]) return false;
    return true;
  }

  const bind = prototype.bindBuffer;
  prototype.bindBuffer = function (target, buffer) {
    if (target === this.ARRAY_BUFFER) arrayBuffer.set(this, buffer);
    return bind.apply(this, arguments);
  };

  const allocate = prototype.bufferData;
  prototype.bufferData = function (target, source, usage, sourceOffset, length) {
    const buffer = target === this.ARRAY_BUFFER ? arrayBuffer.get(this) : null;
    if (buffer) {
      const scale = (source && source.BYTES_PER_ELEMENT) || 1;
      const size = typeof source === 'number' ? source :
        length !== undefined ? length * scale : (source && source.byteLength) || 0;
      /* A fresh store: nothing is known to be on the card until it is written. */
      shadows.set(buffer, size > 0 && size <= shadowLimit ?
        {held: new Uint8Array(size), filled: 0} : null);
    }
    return allocate.apply(this, arguments);
  };

  const upload = prototype.bufferSubData;
  prototype.bufferSubData = function (target, offset, source, sourceOffset, length) {
    const buffer = watching && target === this.ARRAY_BUFFER ? arrayBuffer.get(this) : null;
    const shadow = buffer ? shadows.get(buffer) : null;
    if (!shadow) { counts.sent++; return upload.apply(this, arguments); }
    const sent = bytes(source, sourceOffset, length);
    if (!sent || offset < 0 || offset + sent.byteLength > shadow.held.length) {
      shadows.set(buffer, null);
      counts.sent++;
      return upload.apply(this, arguments);
    }
    if (offset + sent.byteLength <= shadow.filled && same(shadow.held, offset, sent)) {
      counts.skipped++;
      counts.skippedBytes += sent.byteLength;
      return undefined;
    }
    shadow.held.set(sent, offset);
    if (offset + sent.byteLength > shadow.filled) shadow.filled = offset + sent.byteLength;
    counts.sent++;
    return upload.apply(this, arguments);
  };

  const copy = prototype.copyBufferSubData;
  if (copy) prototype.copyBufferSubData = function (readTarget, writeTarget) {
    if (writeTarget === this.ARRAY_BUFFER) {
      const buffer = arrayBuffer.get(this);
      if (buffer) shadows.set(buffer, null);
    }
    return copy.apply(this, arguments);
  };

  /* Transform feedback writes buffers behind this bookkeeping. Nothing in the
     game uses it; if something starts, stop guessing rather than guess wrong. */
  const feedback = prototype.beginTransformFeedback;
  if (feedback) prototype.beginTransformFeedback = function () {
    watching = false;
    return feedback.apply(this, arguments);
  };
})();
