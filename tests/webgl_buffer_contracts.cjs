/* Read real GPU buffers after the upload sequences used by the game. */
module.exports = async function bufferContracts(page) {
  return page.evaluate(() => {
    const cases = [
      ['zero-length-rest', gl => {
        gl.bufferData(gl.ARRAY_BUFFER, 4, gl.DYNAMIC_DRAW);
        gl.bufferSubData(gl.ARRAY_BUFFER, 0, new Uint8Array([1, 2, 3, 4]), 0, 0);
        return [1, 2, 3, 4];
      }],
      ['unwritten-prefix', gl => {
        gl.bufferData(gl.ARRAY_BUFFER, new Uint8Array([9, 9, 9, 9]), gl.DYNAMIC_DRAW);
        gl.bufferSubData(gl.ARRAY_BUFFER, 2, new Uint8Array([7, 8]));
        gl.bufferSubData(gl.ARRAY_BUFFER, 0, new Uint8Array([0, 0]));
        return [0, 0, 7, 8];
      }],
      ['alias-upload', (gl, buffer) => {
        gl.bufferData(gl.ARRAY_BUFFER, 4, gl.DYNAMIC_DRAW);
        gl.bufferSubData(gl.ARRAY_BUFFER, 0, new Uint8Array([1, 2, 3, 4]));
        gl.bindBuffer(gl.COPY_WRITE_BUFFER, buffer);
        gl.bufferSubData(gl.COPY_WRITE_BUFFER, 0, new Uint8Array([9, 9, 9, 9]));
        gl.bufferSubData(gl.ARRAY_BUFFER, 0, new Uint8Array([1, 2, 3, 4]));
        return [1, 2, 3, 4];
      }],
      ['alias-copy', (gl, buffer) => {
        gl.bufferData(gl.ARRAY_BUFFER, 4, gl.DYNAMIC_DRAW);
        gl.bufferSubData(gl.ARRAY_BUFFER, 0, new Uint8Array([1, 2, 3, 4]));
        const source = gl.createBuffer();
        gl.bindBuffer(gl.COPY_READ_BUFFER, source);
        gl.bufferData(gl.COPY_READ_BUFFER, new Uint8Array([9, 9, 9, 9]), gl.STATIC_DRAW);
        gl.bindBuffer(gl.COPY_WRITE_BUFFER, buffer);
        gl.copyBufferSubData(gl.COPY_READ_BUFFER, gl.COPY_WRITE_BUFFER, 0, 0, 4);
        gl.bufferSubData(gl.ARRAY_BUFFER, 0, new Uint8Array([1, 2, 3, 4]));
        gl.deleteBuffer(source);
        return [1, 2, 3, 4];
      }],
      ['view-offset-rest', gl => {
        const storage = new Uint8Array([99, 99, 1, 2, 3, 4, 99]);
        const view = new Uint8Array(storage.buffer, 2, 4);
        gl.bufferData(gl.ARRAY_BUFFER, 4, gl.DYNAMIC_DRAW);
        gl.bufferSubData(gl.ARRAY_BUFFER, 1, view, 1, 0);
        return [0, 2, 3, 4];
      }],
      ['reallocated-store', gl => {
        gl.bufferData(gl.ARRAY_BUFFER, 4, gl.DYNAMIC_DRAW);
        gl.bufferSubData(gl.ARRAY_BUFFER, 0, new Uint8Array([1, 2, 3, 4]));
        gl.bufferData(gl.ARRAY_BUFFER, new Uint8Array([9, 9, 9, 9]), gl.DYNAMIC_DRAW);
        gl.bufferSubData(gl.ARRAY_BUFFER, 2, new Uint8Array([3, 4]));
        gl.bufferSubData(gl.ARRAY_BUFFER, 0, new Uint8Array([0, 0]));
        return [0, 0, 3, 4];
      }],
    ];
    return cases.map(([name, run]) => {
      const canvas = document.createElement('canvas');
      const gl = canvas.getContext('webgl2');
      if (!gl) throw new Error('WebGL2 context required');
      const buffer = gl.createBuffer();
      gl.bindBuffer(gl.ARRAY_BUFFER, buffer);
      const expected = run(gl, buffer);
      const actual = new Uint8Array(expected.length);
      gl.getBufferSubData(gl.ARRAY_BUFFER, 0, actual);
      const error = gl.getError();
      gl.deleteBuffer(buffer);
      gl.getExtension('WEBGL_lose_context')?.loseContext();
      return {name, expected, actual: [...actual], error};
    });
  });
};
