/* Canvas controls share the native game's input and layout. */
(function () {
  const canvas = document.querySelector('#canvas');
  if (!canvas) return;
  let gesture = null;
  Module.crownlessTouchEnabled = true;
  Module.renderCrownlessTouch = frame => {
    canvas.setAttribute('aria-label', frame.title || 'Crownless Carriage game');
  };
  const fields = new Map();
  Module.renderCrownlessFields = descriptors => {
    const present = new Set();
    const bounds = canvas.getBoundingClientRect();
    for (const field of descriptors) {
      present.add(field.action);
      let input = fields.get(field.action);
      if (!input) {
        input = document.createElement('input');
        input.className = 'game-field';
        input.type = field.secret ? 'password' : 'text';
        input.autocomplete = 'off';
        input.autocapitalize = 'off';
        input.spellcheck = false;
        input.setAttribute('aria-label', field.label);
        const publish = () => {
          const size = lengthBytesUTF8(input.value) + 1;
          const pointer = _malloc(size);
          if (!pointer) return;
          stringToUTF8(input.value, pointer, size);
          Module._CrownlessCompanyText(field.action, pointer);
          _free(pointer);
        };
        input.addEventListener('input', publish);
        input.addEventListener('focus', publish);
        input.addEventListener('blur', () => Module._CrownlessCompanyText(0, 0));
        input.addEventListener('keydown', event => {
          event.stopPropagation();
          if (event.key === 'Enter' || event.key === 'Escape') { event.preventDefault(); input.blur(); canvas.focus(); }
        });
        input.addEventListener('keyup', event => event.stopPropagation());
        input.addEventListener('keypress', event => event.stopPropagation());
        document.body.append(input);
        fields.set(field.action, input);
      }
      if (document.activeElement !== input) input.value = field.value;
      input.maxLength = field.capacity;
      input.style.left = (bounds.left + field.x * bounds.width / canvas.width) + 'px';
      input.style.top = (bounds.top + field.y * bounds.height / canvas.height) + 'px';
      input.style.width = (field.width * bounds.width / canvas.width) + 'px';
      input.style.height = (field.height * bounds.height / canvas.height) + 'px';
    }
    for (const [id, input] of fields) if (!present.has(id)) { input.remove(); fields.delete(id); }
  };
  canvas.addEventListener('pointerdown', event => {
    if (event.pointerType === 'mouse') return;
    event.preventDefault();
    if (!event.isPrimary || gesture) { gesture = null; return; }
    gesture = {id: event.pointerId, x: event.clientX, y: event.clientY, moved: false};
    canvas.setPointerCapture(event.pointerId);
  });
  canvas.addEventListener('pointermove', event => {
    if (gesture && gesture.id === event.pointerId &&
        Math.hypot(event.clientX - gesture.x, event.clientY - gesture.y) > 12) gesture.moved = true;
  });
  canvas.addEventListener('pointerup', event => {
    if (!gesture || gesture.id !== event.pointerId) return;
    const tap = gesture;
    gesture = null;
    event.preventDefault();
    if (tap.moved || !Module._CrownlessTouchTap) return;
    const bounds = canvas.getBoundingClientRect();
    Module._CrownlessTouchTap(
      (event.clientX - bounds.left) * canvas.width / bounds.width,
      (event.clientY - bounds.top) * canvas.height / bounds.height);
  });
  function cancelGesture() { gesture = null; }
  canvas.addEventListener('pointercancel', cancelGesture);
  canvas.addEventListener('lostpointercapture', cancelGesture);
  window.addEventListener('blur', cancelGesture);
  window.addEventListener('resize', cancelGesture);
  document.addEventListener('visibilitychange', cancelGesture);
  // The pointer path above owns touch taps. Keep legacy touch emulation separate.
  for (const type of ['touchstart', 'touchmove', 'touchend', 'touchcancel']) {
    canvas.addEventListener(type, event => {
      event.preventDefault();
      event.stopImmediatePropagation();
    }, {capture: true, passive: false});
  }
})();
