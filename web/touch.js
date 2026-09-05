/* Touch controls are published by the game from its current menu and actions. */
(function () {
  const canvas = document.querySelector('#canvas');
  const panel = document.querySelector('#touch-panel');
  if (!canvas || !panel) return;
  if (Module.ccCoop?.preview) return;
  const actions = document.querySelector('#touch-actions');
  const title = document.querySelector('#touch-title');
  const detail = document.querySelector('#touch-detail');
  const shortcuts = document.querySelector('#touch-shortcuts');
  const reading = document.querySelector('#touch-reading');
  const readingPanel = document.querySelector('#touch-reading-panel');
  const toggle = document.querySelector('#touch-toggle');
  const smallScreen = window.matchMedia('(max-width: 680px)');
  const coarsePointer = window.matchMedia('(any-pointer: coarse)');
  let preference = null;
  let gesture = null;
  let frame = null;

  function updateMode() {
    Module.crownlessTouchEnabled = preference ??
      (coarsePointer.matches || (smallScreen.matches && navigator.maxTouchPoints > 0));
    document.body.classList.toggle('touch-mode', Module.crownlessTouchEnabled);
    panel.hidden = !Module.crownlessTouchEnabled;
    toggle.setAttribute('aria-pressed', String(Module.crownlessTouchEnabled));
    if (Module.crownlessTouchEnabled) {
      document.querySelector('#hint').textContent =
        'Tap the scene to walk or approach. Use the buttons for actions. Turn your phone for a wider view.';
      if (frame) Module.renderCrownlessTouch(frame);
    } else {
      document.querySelector('#hint').textContent =
        'Choose Offline and Play. Esc opens the game menu. Saves stay in this browser.';
    }
  }

  Module.renderCrownlessTouch = function (next) {
    frame = next;
    if (!Module.crownlessTouchEnabled) return;
    title.textContent = next.title || 'Your journey';
    detail.textContent = next.detail;
    shortcuts.hidden = Module.crownlessScreen !== 'playing';
    readingPanel.hidden = Module.crownlessScreen !== 'playing' || !next.reading;
    if (reading.textContent !== next.reading) reading.textContent = next.reading;
    // Keep unchanged buttons mounted so a frame update preserves focus and taps.
    next.buttons.forEach((item, index) => {
      let button = actions.children[index];
      if (!button) {
        button = document.createElement('button');
        button.type = 'button';
        button.addEventListener('pointerdown', () => { button.touchRevision = button.dataset.revision; });
        button.addEventListener('click', event => {
          const revision = event.detail === 0 ? button.dataset.revision : button.touchRevision;
          if (revision !== button.dataset.revision) return;
          Module._CrownlessTouchActivate(index, Number(revision));
        });
        actions.append(button);
      }
      const label = item.label.replace(/\s*\[(?:F7|Enter|Backspace)\]$/, '')
        .replace(/^\[\d\]\s*/, '').replace(/\s+(?:F5|Esc|Enter|Up|Down|[BQTMH])$/, '').trim();
      if (button.textContent !== label) {
        button.blur();
        button.textContent = label;
      }
      button.disabled = !item.enabled;
      button.setAttribute('aria-pressed', String(item.active));
      button.dataset.revision = String(next.revision);
    });
    while (actions.children.length > next.buttons.length) actions.lastElementChild.remove();
  };

  toggle.addEventListener('click', () => {
    preference = !Module.crownlessTouchEnabled;
    updateMode();
  });
  smallScreen.addEventListener('change', updateMode);
  coarsePointer.addEventListener('change', updateMode);
  panel.addEventListener('keydown', event => event.stopPropagation());
  panel.addEventListener('keyup', event => event.stopPropagation());
  panel.querySelectorAll('[data-key]').forEach(button => {
    button.addEventListener('click', () => {
      if (Module._CrownlessTouchKey) Module._CrownlessTouchKey(Number(button.dataset.key));
    });
  });

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
  updateMode();
})();
