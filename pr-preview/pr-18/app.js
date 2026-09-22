import { createEngine } from './engine.js';
import { createPanel, ledPxForWidth } from './panel.js';

const $ = id => document.getElementById(id);
const MODES = [
  ['IND_NONALT', 'Individual qualification'],
  ['IND_ALT', 'Individual match, alternating'],
  ['TEAM_SIMUL', 'Team, both sides together'],
  ['TEAM_ALT', 'Team match, alternating'],
  ['MIXED_TEAM', 'Mixed team'],
  ['PRACTICE', 'Practice']
];

for (const [value, label] of MODES) {
  const option = document.createElement('option');
  option.value = value;
  option.textContent = label;
  $('mode').appendChild(option);
}

function showPage() {
  const setup = location.hash === '#setup';
  $('pageField').hidden = setup;
  $('pageSetup').hidden = !setup;
  $('navField').classList.toggle('on', !setup);
  $('navSetup').classList.toggle('on', setup);
}
window.addEventListener('hashchange', showPage);
showPage();

function paintClock(state, ms) {
  const face = $('clockFace');
  const group = window.Operator.groupOnClock(state);
  $('clock').textContent = window.Operator.clockFace(state, ms);
  $('clock').style.color = window.Operator.timerColourCss(state);
  $('clockGroup').hidden = !group;
  if (group) {
    const vertical = state.abcdVertical !== false;
    face.classList.toggle('under', !vertical);
    $('clockGroup').textContent = vertical ? group.charAt(0) + '\n' + group.charAt(1) : group;
    $('clockGroup').style.color = window.Operator.groupColourCss(state);
  } else {
    face.classList.remove('under');
  }
}

function draftFromForm(state) {
  return Object.assign({}, state, {
    mode: $('mode').value,
    eventClass: $('eventClass').value,
    arrowsPerEnd: +$('arrowsPerEnd').value,
    resumeOccupy: $('resumeOccupy').value === 'true',
    firstShooter: +$('firstShooter').value,
    abcdRotation: $('abcdRotation').value === 'true',
    details: $('abcdRotation').value === 'true' ? Math.max(state.details || 0, 2) : 1,
    clockSeconds: $('clockSeconds').value === 'true',
    showAbcd: $('showAbcd').value === 'true',
    abcdVertical: $('abcdVertical').value === 'true',
    showEndLabels: $('showEndLabels').value === 'true',
    abcdFollowTimer: $('abcdFollowTimer').value === 'true',
    abcdColour: $('abcdColour').value,
    panelLines: collectLines(),
    lineScale: $('lineScale') ? $('lineScale').value : 'FILL',
    heroLine: $('heroLine') ? +$('heroLine').value : 0,
    breakEnabled: $('breakEnabled').value === 'true',
    breakAfterEnds: +$('breakAfterEnds').value,
    breakMinutes: +$('breakMinutes').value
  });
}

const CONTENT_OPTIONS = [
  ['CLOCK', 'Clock'],
  ['CLOCK_END', 'End number'],
  ['ARROWS', 'Arrows shot'],
  ['SCORE', 'Score'],
  ['SET_POINTS', 'Set points'],
  ['SHOOTER', 'Shooting side'],
  ['BLANK', 'Blank']
];

const DEFAULT_LINE_ORDER = ['CLOCK', 'CLOCK_END', 'SCORE', 'ARROWS', 'SET_POINTS', 'SHOOTER'];

function stackedP5Count(preset) {
  return ({ P5_64X32: 1, P5_64X64: 2, P5_96X64: 3, P5_128X64: 4, P5_160X64: 5 })[preset] || 0;
}

function maxLinesFor(preset, orientation) {
  if (preset === 'LED_32X16') return 1;
  const count = stackedP5Count(preset);
  let rows = count === 1 ? 32 : 64;
  if (orientation === 'PORTRAIT') rows = count === 1 ? 64 : count * 32;
  return Math.max(1, Math.min(10, Math.floor(rows / 8)));
}

function defaultLinesFor(preset, orientation) {
  if (preset === 'LED_32X16') return ['CLOCK'];
  const maxLines = maxLinesFor(preset, orientation);
  let used = 1;
  if (orientation === 'PORTRAIT') used = maxLines;
  else if (maxLines >= 4) used = 2;
  if (used > DEFAULT_LINE_ORDER.length) used = DEFAULT_LINE_ORDER.length;
  return DEFAULT_LINE_ORDER.slice(0, used);
}

function collectLines() {
  const selected = [];
  const count = $('lineCount') ? +$('lineCount').value : 10;
  for (let index = 0; index < count; index++) {
    const field = $('line' + index);
    if (!field) continue;
    selected.push(field.value);
  }
  return selected.join(',') || ($('display') ? $('display').value : 'CLOCK');
}

function fillSelect(field, count, labelFor) {
  if (!field) return;
  if (field.options.length !== count) {
    field.innerHTML = '';
    for (let index = 0; index < count; index++) {
      const option = document.createElement('option');
      option.value = String(labelFor ? index : index + 1);
      option.textContent = labelFor ? labelFor(index) : String(index + 1);
      field.appendChild(option);
    }
  }
}

function layoutHint(preset) {
  return ({
    P5_64X32: 'one horizontal P5',
    P5_64X64: 'two P5 stood on end',
    P5_96X64: 'three P5 stood on end',
    P5_128X64: 'four P5 stood on end',
    P5_160X64: 'five P5 stood on end'
  })[preset] || '';
}

let onLineChange = () => {};

function syncLineSlots(state) {
  const maxLines = state.panelMaxLines || 1;
  const chosen = (state.panelLines || state.display || 'CLOCK').split(',').filter(Boolean);
  const shown = Math.max(1, Math.min(maxLines, chosen.length || 1));
  fillSelect($('lineCount'), maxLines);
  if ($('lineCount') && document.activeElement !== $('lineCount')) $('lineCount').value = String(shown);
  fillSelect($('heroLine'), shown, index => 'Line ' + (index + 1));
  const hero = Math.min(state.heroLine || 0, shown - 1);
  if ($('heroLine') && document.activeElement !== $('heroLine')) $('heroLine').value = String(hero);
  const scale = ($('lineScale') && $('lineScale').value) || state.lineScale || 'FILL';
  if ($('lineScaleRow')) $('lineScaleRow').hidden = maxLines <= 1;
  if ($('heroLineWrap')) $('heroLineWrap').hidden = maxLines <= 1 || scale !== 'HERO';
  const box = $('lineSlots');
  if (!box) return;
  if (box.childElementCount !== shown) {
    box.innerHTML = '';
    for (let index = 0; index < shown; index++) {
      const wrap = document.createElement('div');
      const label = document.createElement('label');
      label.htmlFor = 'line' + index;
      label.textContent = shown === 1 ? 'Panel shows' : 'Line ' + (index + 1);
      const select = document.createElement('select');
      select.id = 'line' + index;
      select.onchange = () => {
        if (index === 0) $('display').value = select.value;
        onLineChange();
      };
      for (const [value, text] of CONTENT_OPTIONS) {
        const option = document.createElement('option');
        option.value = value;
        option.textContent = text;
        select.appendChild(option);
      }
      wrap.appendChild(label);
      wrap.appendChild(select);
      box.appendChild(wrap);
    }
  }
  for (let index = 0; index < shown; index++) {
    const field = $('line' + index);
    if (field && document.activeElement !== field) field.value = chosen[index] || 'BLANK';
  }
  if ($('layoutNote')) {
    const preset = state.panelPreset || '';
    let note = (state.displayDriver || 'WS2812B') + ' ' + (state.panelColumns || 32) + '×' +
      (state.panelRows || 16);
    const hint = layoutHint(preset);
    if (hint) note += ' — ' + hint;
    note += ' — ' + shown + ' of ' + maxLines + ' line' + (maxLines === 1 ? '' : 's');
    if (shown > 1 && scale === 'HERO') note += ', line ' + (hero + 1) + ' larger';
    else if (shown > 1) note += ', equal height';
    $('layoutNote').innerHTML = note;
  }
}

function apply(state) {
  // The WASM core is ticked on every refresh, so remainingMs is already now.
  paintClock(state, state.remainingMs);
  $('phase').textContent = state.phase;
  $('end').textContent = state.end;
  $('arrows').textContent = state.arrowsShot + '/' + state.arrowsPerEnd;
  $('arrowsBox').hidden = !window.Operator.tracksLiveArrows(state);
  $('perArrow').textContent = (state.perArrowMs / 1000) + ' s';
  $('panelText').textContent = state.panelText || '(blank)';
  const alternating = state.mode === 'IND_ALT' || state.mode === 'TEAM_ALT';
  window.Operator.syncDisplayOptions($('display'), state);
  $('shooterBox').hidden = !alternating;
  $('shooter').textContent = state.shooter === 1 ? 'A' : state.shooter === 2 ? 'B' : '-';
  $('detailBox').hidden = state.details <= 1;
  $('detail').textContent = state.detail + '/' + state.details;
  $('shootOffBox').hidden = !state.shootOff;
  for (const [id, colour] of [['lampRed', 'RED'], ['lampGreen', 'GREEN'], ['lampYellow', 'YELLOW']]) {
    $(id).classList.toggle('on', state.light === colour);
  }
  if (document.activeElement !== $('mode')) $('mode').value = state.mode;
  if (document.activeElement !== $('eventClass')) $('eventClass').value = state.eventClass;
  if (document.activeElement !== $('arrowsPerEnd')) $('arrowsPerEnd').value = String(state.arrowsPerEnd);
  if (document.activeElement !== $('resumeOccupy')) $('resumeOccupy').value = String(state.resumeOccupy);
  if (document.activeElement !== $('firstShooter')) $('firstShooter').value = String(state.firstShooter || 1);
  if (document.activeElement !== $('abcdRotation')) $('abcdRotation').value = String(!!state.abcdRotation);
  if (document.activeElement !== $('display')) $('display').value = state.display;
  if ($('panelPreset') && document.activeElement !== $('panelPreset')) {
    $('panelPreset').value = state.panelPreset || 'LED_32X16';
  }
  if ($('orientation') && document.activeElement !== $('orientation')) {
    $('orientation').value = state.orientation || 'LANDSCAPE';
  }
  if ($('lineScale') && document.activeElement !== $('lineScale')) {
    $('lineScale').value = state.lineScale || 'FILL';
  }
  syncLineSlots(state);
  if (document.activeElement !== $('clockSeconds')) $('clockSeconds').value = String(state.clockSeconds !== false);
  if (document.activeElement !== $('showAbcd')) $('showAbcd').value = String(state.showAbcd !== false);
  if (document.activeElement !== $('abcdVertical')) $('abcdVertical').value = String(state.abcdVertical !== false);
  if (document.activeElement !== $('showEndLabels')) $('showEndLabels').value = String(state.showEndLabels !== false);
  if (document.activeElement !== $('abcdFollowTimer')) {
    $('abcdFollowTimer').value = String(!!state.abcdFollowTimer);
  }
  if (document.activeElement !== $('abcdColour')) {
    $('abcdColour').value = state.abcdColour || '#ffffff';
  }
  $('abcdColour').disabled = $('abcdFollowTimer').value === 'true';
  if (document.activeElement !== $('breakEnabled')) $('breakEnabled').value = String(state.breakEnabled !== false);
  if (document.activeElement !== $('breakAfterEnds') && state.breakAfterEnds != null) {
    $('breakAfterEnds').value = String(state.breakAfterEnds);
  }
  if (document.activeElement !== $('breakMinutes')) {
    if (state.breakMinutes != null) $('breakMinutes').value = String(state.breakMinutes);
    else if (state.breakSeconds != null) $('breakMinutes').value = String(Math.round(state.breakSeconds / 60));
  }
  if (document.activeElement !== $('matchLogic')) $('matchLogic').value = String(state.matchEnabled);
  if (document.activeElement !== $('volume')) $('volume').value = state.volume;
  $('volumeValue').textContent = state.volume;
  if (document.activeElement !== $('traceLevel')) $('traceLevel').value = state.traceLevel;

  const story = window.Operator.situation(state);
  $('headline').textContent = story.headline;
  $('situation').textContent = story.detail;
  window.Operator.renderFlow($('flow'), state);
  window.Operator.renderPreview($('preview'), draftFromForm(state));
}

function updateSize(panel) {
  const info = panel.summary();
  const firmware = `${info.firmwareMm.width} × ${info.firmwareMm.height} mm`;
  const preview = `${info.previewMm.width} × ${info.previewMm.height} mm`;
  $('sizeReadout').innerHTML =
    `<b>${info.firmware}</b> firmware panel is <b>${firmware}</b> at ${info.pitchMm} mm pitch.`;
}

function setupBeep(engine) {
  let ctx = null;
  let oscillator = null;
  let gain = null;
  let wasActive = false;

  function ensure() {
    if (ctx) return;
    ctx = new (window.AudioContext || window.webkitAudioContext)();
    gain = ctx.createGain();
    gain.gain.value = 0;
    gain.connect(ctx.destination);
    oscillator = ctx.createOscillator();
    oscillator.frequency.value = 880;
    oscillator.connect(gain);
    oscillator.start();
  }

  return function tick() {
    const active = engine.soundActive();
    if (active && !wasActive) {
      try {
        ensure();
        ctx.resume();
        const volume = Number($('volume').value) / 100;
        gain.gain.setTargetAtTime(0.08 * volume, ctx.currentTime, 0.01);
      } catch (error) {
        // Autoplay can block until a tap. The next control click retries.
      }
    } else if (!active && wasActive && gain && ctx) {
      gain.gain.setTargetAtTime(0, ctx.currentTime, 0.01);
    }
    wasActive = active;
  };
}

try {
  const engine = await createEngine();
  const panel = createPanel($('panel'), engine);
  const beep = setupBeep(engine);
  let lastLogSeq = 0;
  let lastState = null;
  let lastActionKey = '';

  function actionKey(state) {
    return [
      state.phase, state.mode, state.detail, state.details, state.shooter,
      state.arrowsShot, state.arrowsPerEnd, state.abcdRotation, state.firstShooter,
      state.end, state.breakEnabled, state.breakAfterEnds, state.breakMinutes
    ].join('|');
  }

  function paint(state) {
    lastState = state;
    apply(state);
    const key = actionKey(state);
    if (key !== lastActionKey) {
      lastActionKey = key;
      window.Operator.renderActions($('actions'), state, runAction);
      const canExtend = window.Operator.renderAux($('aux'), state, runAction);
      $('extendBox').hidden = !canExtend;
      $('extras').hidden = !canExtend && !$('aux').childElementCount;
    }
    panel.settings.preset = state.panelPreset || 'LED_32X16';
    panel.settings.orientation = state.orientation || 'LANDSCAPE';
    panel.draw();
    updateSize(panel);
    beep();
  }

  function refresh() {
    paint(engine.state());
  }

  async function runAction(spec) {
    if (spec.action === 'next_then_start') {
      engine.control('next_end');
      engine.control('start');
      refresh();
      return;
    }
    if (spec.action === 'extend') {
      const seconds = spec.seconds != null ? spec.seconds : +$('extendSeconds').value;
      engine.control('extend', seconds);
      refresh();
      return;
    }
    if (spec.action === 'adjust_break') {
      engine.control('adjust_break', spec.seconds);
      refresh();
      return;
    }
    engine.control(spec.action);
    refresh();
  }

  function refreshLog() {
    const data = engine.log(lastLogSeq);
    const pane = $('log');
    if (!data.lines.length) {
      if (!pane.textContent) pane.textContent = 'Nothing recorded yet.';
      return;
    }
    const text = data.lines.map(line => typeof line === 'string' ? line : JSON.stringify(line)).join('\n');
    pane.textContent = lastLogSeq === 0 ? text : pane.textContent + '\n' + text;
    const last = data.lines[data.lines.length - 1];
    lastLogSeq = (typeof last === 'object' && last.seq) ? last.seq : lastLogSeq;
  }

  function session() {
    if ($('abcdRotation').value === 'true' && (!lastState || (lastState.details || 1) < 2)) {
      lastState = Object.assign({}, lastState || {}, { details: 2 });
    }
    if (lastState) window.Operator.renderPreview($('preview'), draftFromForm(lastState));
    const code = engine.session({
      mode: $('mode').value,
      eventClass: $('eventClass').value,
      arrowsPerEnd: +$('arrowsPerEnd').value,
      firstShooter: +$('firstShooter').value,
      details: 2,
      practiceSeconds: 300,
      division: 'RECURVE',
      matchLogic: $('matchLogic').value === 'true',
      resumeOccupy: $('resumeOccupy').value === 'true',
      signalEachPeriod: true,
      abcdRotation: $('abcdRotation').value === 'true',
      shootOff: false,
      breakEnabled: $('breakEnabled').value === 'true',
      breakAfterEnds: +$('breakAfterEnds').value,
      breakMinutes: +$('breakMinutes').value
    });
    $('note').textContent = code === 2 ? 'Mode not implemented' : '';
    refresh();
  }

  $('btnExtend').onclick = () => runAction({ action: 'extend' });
  $('mode').onchange = session;
  $('eventClass').onchange = session;
  $('arrowsPerEnd').onchange = session;
  $('resumeOccupy').onchange = session;
  $('firstShooter').onchange = session;
  $('abcdRotation').onchange = session;
  $('breakEnabled').onchange = session;
  $('breakAfterEnds').onchange = session;
  $('breakMinutes').onchange = session;
  $('matchLogic').onchange = session;
  $('display').onchange = () => {
    engine.display($('display').value);
    savePanelOptions();
  };
  function savePanelOptions() {
    engine.panelOptions({
      clockSeconds: $('clockSeconds').value === 'true',
      showAbcd: $('showAbcd').value === 'true',
      abcdVertical: $('abcdVertical').value === 'true',
      showEndLabels: $('showEndLabels').value === 'true',
      abcdFollowTimer: $('abcdFollowTimer').value === 'true',
      abcdColour: $('abcdColour').value,
      panelPreset: $('panelPreset').value,
      orientation: $('orientation').value,
      lines: collectLines(),
      lineScale: $('lineScale') ? $('lineScale').value : 'FILL',
      heroLine: $('heroLine') ? +$('heroLine').value : 0
    });
    refresh();
  }
  onLineChange = savePanelOptions;
  $('clockSeconds').onchange = savePanelOptions;
  $('showAbcd').onchange = savePanelOptions;
  $('abcdVertical').onchange = savePanelOptions;
  $('showEndLabels').onchange = savePanelOptions;
  $('abcdFollowTimer').onchange = savePanelOptions;
  $('abcdColour').onchange = savePanelOptions;
  $('abcdColour').oninput = savePanelOptions;
  $('volume').onchange = () => { engine.sound({ volume: +$('volume').value }); refresh(); };
  $('btnTestSound').onclick = () => { engine.testTone(2000); refresh(); };
  $('traceLevel').onchange = () => { engine.traceLevel($('traceLevel').value); refresh(); refreshLog(); };

  function availablePanelWidth() {
    const stage = $('stage');
    const style = getComputedStyle(stage);
    return Math.max(64, stage.clientWidth - parseFloat(style.paddingLeft) - parseFloat(style.paddingRight));
  }

  function formatLedPx(value) {
    const rounded = Math.round(value * 10) / 10;
    return Number.isInteger(rounded) ? String(rounded) : rounded.toFixed(1);
  }

  let autoFitLed = true;

  function fitLedToStage() {
    $('ledPx').value = String(ledPxForWidth(panel.firmware.columns, +$('pitchMm').value, availablePanelWidth()));
  }

  function applyPreview() {
    panel.settings.ledPx = +$('ledPx').value;
    panel.settings.pitchMm = +$('pitchMm').value;
    panel.settings.preset = $('panelPreset').value;
    panel.settings.orientation = $('orientation').value;
    $('ledPxValue').textContent = formatLedPx(panel.settings.ledPx);
    $('pitchMmValue').textContent = panel.settings.pitchMm;
    panel.resize();
    panel.draw();
    updateSize(panel);
  }
  function onLineCountChange() {
    const count = $('lineCount') ? +$('lineCount').value : 1;
    const current = collectLines().split(',').filter(Boolean);
    while (current.length < count) current.push(DEFAULT_LINE_ORDER[current.length] || 'BLANK');
    if (lastState) {
      lastState.panelLines = current.slice(0, count).join(',');
      lastState.lineScale = $('lineScale') ? $('lineScale').value : lastState.lineScale;
      lastState.heroLine = $('heroLine') ? +$('heroLine').value : lastState.heroLine;
      syncLineSlots(lastState);
    }
    savePanelOptions();
  }
  function onLayoutChange() {
    const preset = $('panelPreset').value;
    const orientation = $('orientation').value;
    const lines = defaultLinesFor(preset, orientation);
    $('display').value = lines[0];
    if ($('lineScale')) $('lineScale').value = 'FILL';
    if ($('heroLine')) $('heroLine').value = '0';
    if (lastState) {
      lastState.panelPreset = preset;
      lastState.orientation = orientation;
      lastState.panelLines = lines.join(',');
      lastState.panelMaxLines = maxLinesFor(preset, orientation);
      lastState.lineScale = 'FILL';
      lastState.heroLine = 0;
      syncLineSlots(lastState);
    }
    engine.display(lines[0]);
    autoFitLed = true;
    savePanelOptions();
    fitLedToStage();
    applyPreview();
  }
  $('ledPx').oninput = () => {
    autoFitLed = false;
    applyPreview();
  };
  $('pitchMm').oninput = applyPreview;
  $('panelPreset').onchange = onLayoutChange;
  $('orientation').onchange = onLayoutChange;
  if ($('lineCount')) $('lineCount').onchange = onLineCountChange;
  if ($('lineScale')) $('lineScale').onchange = () => {
    if ($('heroLineWrap')) $('heroLineWrap').hidden = $('lineScale').value !== 'HERO';
    savePanelOptions();
  };
  if ($('heroLine')) $('heroLine').onchange = savePanelOptions;
  window.addEventListener('resize', () => {
    if (!autoFitLed) return;
    fitLedToStage();
    applyPreview();
  });

  refresh();
  fitLedToStage();
  applyPreview();
  setInterval(() => {
    paint(engine.state());
  }, 100);
  setInterval(refreshLog, 2000);
} catch (error) {
  $('error').hidden = false;
  $('error').textContent = 'The WASM core failed to load: ' + error.message;
  throw error;
}
