import { createEngine } from './engine.js';
import { createPanel } from './panel.js';

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
    breakEnabled: $('breakEnabled').value === 'true',
    breakAfterEnds: +$('breakAfterEnds').value,
    breakSeconds: +$('breakSeconds').value
  });
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
  if (document.activeElement !== $('breakSeconds') && state.breakSeconds != null) {
    $('breakSeconds').value = String(state.breakSeconds);
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
  $('sizeReadout').innerHTML = info.same
    ? `<b>${info.firmware}</b> firmware panel is <b>${firmware}</b> at ${info.pitchMm} mm pitch.`
    : `Firmware stays <b>${info.firmware}</b> (${firmware}). Preview <b>${info.preview}</b> would be <b>${preview}</b> if we drew the same digits larger.`;
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
      state.end, state.breakEnabled, state.breakAfterEnds
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
    }
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
      engine.control('extend', +$('extendSeconds').value);
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
      breakSeconds: +$('breakSeconds').value
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
  $('breakSeconds').onchange = session;
  $('matchLogic').onchange = session;
  $('display').onchange = () => { engine.display($('display').value); refresh(); };
  function savePanelOptions() {
    engine.panelOptions({
      clockSeconds: $('clockSeconds').value === 'true',
      showAbcd: $('showAbcd').value === 'true',
      abcdVertical: $('abcdVertical').value === 'true',
      showEndLabels: $('showEndLabels').value === 'true',
      abcdFollowTimer: $('abcdFollowTimer').value === 'true',
      abcdColour: $('abcdColour').value
    });
    refresh();
  }
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

  function applyPreview() {
    panel.settings.ledPx = +$('ledPx').value;
    panel.settings.pitchMm = +$('pitchMm').value;
    panel.settings.preview = $('previewSize').value;
    $('ledPxValue').textContent = panel.settings.ledPx;
    $('pitchMmValue').textContent = panel.settings.pitchMm;
    panel.resize();
    panel.draw();
    updateSize(panel);
  }
  $('ledPx').oninput = applyPreview;
  $('pitchMm').oninput = applyPreview;
  $('previewSize').onchange = applyPreview;

  refresh();
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
