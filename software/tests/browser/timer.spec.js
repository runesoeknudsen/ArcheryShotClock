const { test, expect } = require('@playwright/test');

// The page is the director's console, so these tests check that it shows the
// rulebook state truthfully and that a control the current phase forbids
// cannot be pressed at all.

function mockApi(page) {
  const state = {
    schema: 1,
    phase: 'IDLE',
    light: 'OFF',
    mode: 'IND_NONALT',
    eventClass: 'OTHER',
    display: 'CLOCK',
    clockSeconds: true,
    showAbcd: true,
    abcdVertical: true,
    showEndLabels: true,
    abcdFollowTimer: false,
    abcdColour: '#ffffff',
    remainingMs: 120000,
    periodMs: 120000,
    perArrowMs: 40000,
    end: 1,
    arrowsShot: 0,
    arrowsPerEnd: 3,
    running: false,
    finished: false,
    resumeOccupy: true,
    brightness: 16,
    beepMs: 250,
    gapMs: 200,
    soundEnabled: true,
    volume: 60,
    panelText: '120',
    traceSeq: 4,
    shooter: 0,
    sideArrows: [0, 0],
    sideRemainingMs: [0, 0],
    detail: 1,
    details: 2,
    shootOff: false,
    firstShooter: 1,
    signalEachPeriod: true,
    abcdRotation: true,
    practiceSeconds: 300,
    breakEnabled: true,
    breakAfterEnds: 12,
    breakSeconds: 900,
    matchEnabled: false,
    division: 'RECURVE',
    scoring: 'SET_PLAY',
    outcome: 'UNDECIDED',
    matchEnd: 1,
    endTotal: [0, 0],
    runningTotal: [0, 0],
    setPoints: [0, 0],
    warnings: [0, 0],
    yellowCard: [false, false],
    disqualified: [false, false],
    arrowsA: [],
    arrowsB: [],
    traceLevel: 'OFF',
    freeHeap: 122880,
    minFreeHeap: 98304,
    largestFreeBlock: 61440,
    apClients: 1
  };
  const requests = [];

  page.route('**/api/log**', route => {
    const after = Number(new URL(route.request().url()).searchParams.get('after') || 0);
    const all = [{ t: 10, seq: 1, r: 'BOOT', fw: 'test', schema: 2 }];
    return route.fulfill({
      contentType: 'application/json',
      body: JSON.stringify({ lines: all.filter(line => line.seq > after) })
    });
  });

  page.route('**/api/trace', async route => {
    const body = route.request().postDataJSON();
    requests.push({ path: '/api/trace', body });
    state.traceLevel = body.level;
    await route.fulfill({ contentType: 'application/json', body: JSON.stringify(state) });
  });

  page.route('**/api/state', route =>
    route.fulfill({ contentType: 'application/json', body: JSON.stringify(state) })
  );

  page.route('**/api/control', async route => {
    const body = route.request().postDataJSON();
    requests.push({ path: '/api/control', body });

    if (body.action === 'start') {
      state.phase = 'OCCUPY';
      state.light = 'RED';
      state.remainingMs = 10000;
    }
    if (body.action === 'stop') {
      const details = state.details || 1;
      if (state.abcdRotation && details > 1) {
        const first = ((state.end - 1) % details) + 1;
        const next = state.detail >= details ? 1 : state.detail + 1;
        if (next !== first) {
          state.detail = next;
          state.phase = 'OCCUPY';
          state.light = 'RED';
          state.remainingMs = 10000;
          await route.fulfill({ contentType: 'application/json', body: JSON.stringify(state) });
          return;
        }
      }
      state.phase = 'FINISHED';
      state.light = 'RED';
      state.remainingMs = 0;
      state.panelText = state.showEndLabels !== false ? ('End ' + state.end) : '0';
    }
    if (body.action === 'line_clear') {
      state.phase = 'SCORING';
      if (state.showEndLabels !== false) state.panelText = 'Scoring ' + state.end;
    }
    if (body.action === 'next_end') {
      const after = state.breakAfterEnds || 0;
      if (state.phase === 'BREAK') {
        state.end += 1;
        state.phase = 'IDLE';
        state.remainingMs = state.periodMs;
      } else if (state.phase === 'SCORING' && state.breakEnabled !== false && after > 0 && state.end % after === 0) {
        state.phase = 'BREAK';
        state.remainingMs = (state.breakSeconds || 900) * 1000;
        state.light = 'OFF';
      } else if (state.phase === 'SCORING' || state.phase === 'FINISHED') {
        state.end += 1;
        state.phase = 'IDLE';
        state.remainingMs = state.periodMs;
        if (state.abcdRotation && (state.details || 1) > 1) {
          state.detail = ((state.end - 1) % state.details) + 1;
        }
      }
    }
    if (body.action === 'add_arrow') state.arrowsShot += 1;
    if (body.action === 'remove_arrow') state.arrowsShot -= 1;
    if (body.action === 'suspend') state.phase = 'SUSPENDED';
    if (body.action === 'resume') state.phase = 'SHOOTING';
    if (body.action === 'extend') state.remainingMs += body.seconds * 1000;
    await route.fulfill({ contentType: 'application/json', body: JSON.stringify(state) });
  });

  page.route('**/api/session', async route => {
    const body = route.request().postDataJSON();
    requests.push({ path: '/api/session', body });
    if (body.mode) state.mode = body.mode;
    if (body.division) state.division = body.division;
    if (typeof body.matchLogic === 'boolean') state.matchEnabled = body.matchLogic;
    if (typeof body.abcdRotation === 'boolean') {
      state.abcdRotation = body.abcdRotation;
      state.details = body.abcdRotation ? Math.max(Number(body.details) || 0, 2) : 1;
    }
    if (typeof body.firstShooter === 'number') state.firstShooter = body.firstShooter;
    if (typeof body.breakEnabled === 'boolean') state.breakEnabled = body.breakEnabled;
    if (typeof body.breakAfterEnds === 'number') state.breakAfterEnds = body.breakAfterEnds;
    if (typeof body.breakSeconds === 'number') state.breakSeconds = body.breakSeconds;
    state.eventClass = body.eventClass;
    state.arrowsPerEnd = body.arrowsPerEnd;
    state.perArrowMs = body.eventClass === 'ANNOUNCED' ? 30000 : 40000;
    state.periodMs = state.perArrowMs * state.arrowsPerEnd;
    state.remainingMs = state.periodMs;
    await route.fulfill({ contentType: 'application/json', body: JSON.stringify(state) });
  });

  const arrowTotal = list => list.reduce((sum, a) => sum + (a.lost ? 0 : a.v === 11 ? 10 : a.v), 0);

  page.route('**/api/score', async route => {
    const body = route.request().postDataJSON();
    requests.push({ path: '/api/score', body });
    const list = body.side === 0 ? state.arrowsA : state.arrowsB;

    if (body.action === 'arrow') list.push({ v: body.value === 'X' ? 11 : body.value, lost: false });
    if (body.action === 'forfeit') {
      let best = -1;
      list.forEach((arrow, index) => {
        if (!arrow.lost && (best < 0 || arrow.v > list[best].v)) best = index;
      });
      if (best >= 0) list[best].lost = true;
    }
    if (body.action === 'yellow') state.yellowCard[body.side] = true;
    if (body.action === 'warn') state.warnings[body.side] += 1;
    if (body.side === 0 || body.side === 1) state.endTotal[body.side] = arrowTotal(list);

    await route.fulfill({ contentType: 'application/json', body: JSON.stringify(state) });
  });

  for (const path of ['display', 'brightness', 'sound']) {
    page.route(`**/api/${path}`, async route => {
      const body = route.request().postDataJSON();
      requests.push({ path: `/api/${path}`, body });
      Object.assign(state, body);
      if (body.content) state.display = body.content;
      await route.fulfill({ contentType: 'application/json', body: JSON.stringify(state) });
    });
  }

  page.route('**/api/sound/test', async route => {
    requests.push({ path: '/api/sound/test', body: route.request().postDataJSON() || {} });
    await route.fulfill({ contentType: 'application/json', body: JSON.stringify(state) });
  });

  return { state, requests };
}

test.beforeEach(async ({ page }) => {
  page.mock = mockApi(page);
  await page.goto('/');
  await expect(page.locator('#clock')).toHaveText('120');
});

test('shows the phase, end and what the panel is displaying', async ({ page }) => {
  await expect(page.locator('#phase')).toHaveText('IDLE');
  await expect(page.locator('#end')).toHaveText('1');
  await expect(page.locator('#arrowsBox')).toBeHidden();
  await expect(page.locator('#perArrow')).toHaveText('40 s');
  await expect(page.locator('#panel')).toHaveText('120');
});

test('lights the lamp matching the light state', async ({ page }) => {
  await expect(page.locator('#lampRed')).not.toHaveClass(/on/);

  await page.getByRole('button', { name: 'Start Shoot AB' }).click();

  await expect(page.locator('#phase')).toHaveText('OCCUPY');
  await expect(page.locator('#lampRed')).toHaveClass(/on/);
  await expect(page.locator('#lampGreen')).not.toHaveClass(/on/);
});

test('offers only the controls the current phase allows', async ({ page }) => {
  // Idle: the next press is Start Shoot AB. Stop, Score and Resume are absent.
  await expect(page.getByRole('button', { name: 'Start Shoot AB' })).toBeVisible();
  await expect(page.getByRole('button', { name: 'Stop occupy' })).toHaveCount(0);
  await expect(page.getByRole('button', { name: 'Score' })).toHaveCount(0);
  await expect(page.getByRole('button', { name: 'Resume remaining arrows' })).toHaveCount(0);
  await expect(page.getByRole('button', { name: 'Emergency' })).toBeVisible();

  await page.getByRole('button', { name: 'Start Shoot AB' }).click();
  await expect(page.getByRole('button', { name: 'Start Shoot CD' })).toBeVisible();
  await expect(page.getByRole('button', { name: 'Stop occupy' })).toHaveCount(0);

  await page.getByRole('button', { name: 'Start Shoot CD' }).click();
  await expect(page.getByRole('button', { name: 'Stop occupy' })).toBeVisible();

  // Article 11.3.1 puts the three scoring signals after the red light, so
  // Score only appears once the clock has stopped.
  await page.getByRole('button', { name: 'Stop occupy' }).click();
  await expect(page.locator('#phase')).toHaveText('FINISHED');
  await expect(page.getByRole('button', { name: 'Score' })).toBeVisible();
});

test('hides live arrow counting on a simultaneous line', async ({ page }) => {
  await expect(page.locator('#arrowsBox')).toBeHidden();
  await expect(page.getByRole('button', { name: 'Add one arrow' })).toHaveCount(0);
  await expect(page.getByRole('button', { name: 'Remove one arrow' })).toHaveCount(0);

  await page.getByRole('button', { name: 'Start Shoot AB' }).click();
  await expect(page.getByRole('button', { name: 'Add one arrow' })).toHaveCount(0);
  await expect(page.locator('#arrowsBox')).toBeHidden();
});

test('counts arrows on a hold and stops at the end size', async ({ page }) => {
  await page.getByRole('button', { name: 'Start Shoot AB' }).click();
  await page.getByRole('button', { name: 'Suspend' }).click();

  await expect(page.locator('#arrowsBox')).toBeVisible();
  await expect(page.locator('#arrows')).toHaveText('0/3');
  await expect(page.getByRole('button', { name: 'Remove one arrow' })).toHaveCount(0);

  await page.getByRole('button', { name: 'Add one arrow' }).click();
  await expect(page.locator('#arrows')).toHaveText('1/3');
  await page.getByRole('button', { name: 'Add one arrow' }).click();
  await page.getByRole('button', { name: 'Add one arrow' }).click();

  await expect(page.locator('#arrows')).toHaveText('3/3');
  await expect(page.getByRole('button', { name: 'Add one arrow' })).toHaveCount(0);
});

test('counts arrows throughout alternating shooting', async ({ page }) => {
  page.mock.state.mode = 'IND_ALT';
  page.mock.state.phase = 'SHOOTING';
  page.mock.state.shooter = 1;

  await expect(page.locator('#arrowsBox')).toBeVisible();
  await expect(page.locator('#arrows')).toHaveText('0/3');
  await expect(page.getByRole('button', { name: 'Add one arrow' })).toBeVisible();
});

test('changing the event class changes the per-arrow time', async ({ page }) => {
  await page.getByRole('link', { name: 'Setup' }).click();
  await page.locator('#eventClass').selectOption('ANNOUNCED');
  await page.getByRole('link', { name: 'Field' }).click();

  await expect(page.locator('#perArrow')).toHaveText('30 s');
  await expect(page.locator('#clock')).toHaveText('90');
  const session = page.mock.requests.filter(entry => entry.path === '/api/session').pop();
  expect(session.body).toMatchObject({ mode: 'IND_NONALT', eventClass: 'ANNOUNCED', arrowsPerEnd: 3 });
});

test('six-arrow ends double the period', async ({ page }) => {
  await page.getByRole('link', { name: 'Setup' }).click();
  await page.locator('#arrowsPerEnd').selectOption('6');
  await page.getByRole('link', { name: 'Field' }).click();
  await expect(page.locator('#clock')).toHaveText('240');
});

test('offers every implemented mode and not the retired plain countdown', async ({ page }) => {
  await page.getByRole('link', { name: 'Setup' }).click();
  await expect(page.locator('#mode option[value="IND_ALT"]')).toBeEnabled();
  await expect(page.locator('#mode option[value="TEAM_ALT"]')).toBeEnabled();
  await expect(page.locator('#mode option[value="PRACTICE"]')).toBeEnabled();
  await expect(page.locator('#mode option[value="PLAIN"]')).toHaveCount(0);
});

test('start becomes the handoff during alternating shooting', async ({ page }) => {
  await expect(page.locator('#shooterBox')).toBeHidden();

  page.mock.state.mode = 'IND_ALT';
  page.mock.state.phase = 'SHOOTING';
  page.mock.state.shooter = 1;

  // Article 11.2.3.2 makes stopping one clock and starting the opponent's a
  // single action, named for the athlete who shoots next.
  await expect(page.getByRole('button', { name: 'Start Shoot B' })).toBeVisible();
  await expect(page.locator('#shooter')).toHaveText('A');
});

test('shows the detail number when AB/CD rotation is on', async ({ page }) => {
  await expect(page.locator('#detailBox')).toBeVisible();
  await expect(page.locator('#detail')).toHaveText('1/2');

  page.mock.state.abcdRotation = false;
  page.mock.state.details = 1;
  await page.goto('/');

  await expect(page.locator('#detailBox')).toBeHidden();
});

test('sends the extension in seconds and reports the new time', async ({ page }) => {
  await page.getByRole('button', { name: 'Start Shoot AB' }).click();
  await page.locator('#extendSeconds').fill('45');
  await page.getByRole('button', { name: 'Add time' }).click();

  await expect(page.locator('#clock')).toHaveText('55');
  expect(page.mock.requests).toContainEqual({ path: '/api/control', body: { action: 'extend', seconds: 45 } });
});

test('changes what the panel shows', async ({ page }) => {
  await page.getByRole('link', { name: 'Setup' }).click();
  await page.locator('#display').selectOption('CLOCK_END');
  expect(page.mock.requests).toContainEqual({
    path: '/api/display',
    body: {
      content: 'CLOCK_END',
      clockSeconds: true,
      showAbcd: true,
      abcdVertical: true,
      showEndLabels: true,
      abcdFollowTimer: false,
      abcdColour: '#ffffff'
    }
  });
});

test('offers arrows on the panel only for alternating shooting', async ({ page }) => {
  await page.getByRole('link', { name: 'Setup' }).click();
  await expect(page.locator('#display option[value="ARROWS"]')).toHaveJSProperty('hidden', true);

  await page.locator('#mode').selectOption('IND_ALT');
  await expect(page.locator('#display option[value="ARROWS"]')).toHaveJSProperty('hidden', false);
  await page.locator('#display').selectOption('ARROWS');
  expect(page.mock.requests).toContainEqual({
    path: '/api/display',
    body: {
      content: 'ARROWS',
      clockSeconds: true,
      showAbcd: true,
      abcdVertical: true,
      showEndLabels: true,
      abcdFollowTimer: false,
      abcdColour: '#ffffff'
    }
  });
});

test('defaults to seconds and can switch to minutes', async ({ page }) => {
  await expect(page.locator('#clock')).toHaveText('120');
  await page.getByRole('link', { name: 'Setup' }).click();
  await page.locator('#clockSeconds').selectOption('false');
  await page.getByRole('link', { name: 'Field' }).click();
  await expect(page.locator('#clock')).toHaveText('02:00');
  expect(page.mock.requests).toContainEqual({
    path: '/api/display',
    body: {
      content: 'CLOCK',
      clockSeconds: false,
      showAbcd: true,
      abcdVertical: true,
      showEndLabels: true,
      abcdFollowTimer: false,
      abcdColour: '#ffffff'
    }
  });
});

async function openMatchScoring(page) {
  await page.getByRole('link', { name: 'Setup' }).click();
  await page.locator('#matchLogic').selectOption('true');
  await page.getByRole('link', { name: 'Field' }).click();
  await expect(page.locator('#pageField')).toBeVisible();
  await expect(page.locator('#scoring')).toBeVisible();
  await page.locator('#scoring').scrollIntoViewIfNeeded();
}

test('hides scoring until match logic is switched on', async ({ page }) => {
  await expect(page.locator('#scoring')).toBeHidden();

  await openMatchScoring(page);

  await expect(page.locator('#scoringSystem')).toHaveText('Set play');
});

test('a forfeited arrow stays on the card with its value lost', async ({ page }) => {
  await openMatchScoring(page);

  await page.locator('#keypad0 button', { hasText: /^10$/ }).click();
  await page.locator('#keypad0 button', { hasText: /^8$/ }).click();
  await page.locator('#keypad0 button', { hasText: /^7$/ }).click();
  await expect(page.locator('#endTotal0')).toHaveText('25');

  // Article 13.3 forfeits the highest-scoring arrow of the end, not the
  // offending one, and every arrow still appears on the scorecard.
  await page.locator('#sideA').getByRole('button', { name: 'Forfeit highest (13.3)' }).click();

  await expect(page.locator('#arrows0')).toHaveText('(10) 8 7');
  await expect(page.locator('#endTotal0')).toHaveText('15');
});

test('records a yellow card and a warning against a side', async ({ page }) => {
  await openMatchScoring(page);

  await page.locator('#sideB').getByRole('button', { name: 'Yellow card (13.6.1)' }).click();
  await expect(page.locator('#flags1')).toHaveText('- yellow card');

  await page.locator('#sideB').getByRole('button', { name: 'Warning (13.4)' }).click();
  await expect(page.locator('#flags1')).toHaveText('- yellow card, 1 warning(s)');
});

test('shows the trace log', async ({ page }) => {
  await page.getByRole('link', { name: 'Setup' }).click();
  await expect(page.locator('#log')).toContainText('BOOT');
});

test('reports the board memory alongside the clock', async ({ page }) => {
  // A fragmented heap and a bad radio environment both present as an access
  // point that keeps dropping, so the numbers are on the page.
  await page.getByRole('link', { name: 'Setup' }).click();
  await expect(page.locator('#heap')).toHaveText('120 kB');
  await expect(page.locator('#heapMin')).toHaveText('96 kB');
  await expect(page.locator('#apClients')).toHaveText('1');
});

test('recording is off until testing mode is switched on', async ({ page }) => {
  await page.getByRole('link', { name: 'Setup' }).click();
  await expect(page.locator('#traceLevel')).toHaveValue('OFF');

  await page.locator('#traceLevel').selectOption('MINIMAL');

  expect(page.mock.requests).toContainEqual({ path: '/api/trace', body: { level: 'MINIMAL' } });
});

test('saves speaker volume with the sound settings', async ({ page }) => {
  await page.getByRole('link', { name: 'Setup' }).click();
  await expect(page.locator('#volume')).toHaveValue('60');
  await expect(page.locator('#volumeValue')).toHaveText('60');

  await page.locator('#volume').fill('25');
  await page.locator('#volume').dispatchEvent('change');

  expect(page.mock.requests).toContainEqual({
    path: '/api/sound',
    body: { beepMs: 250, gapMs: 200, volume: 25 }
  });
  await expect(page.locator('#volumeValue')).toHaveText('25');
});

test('plays a test tone on the wired speaker', async ({ page }) => {
  await page.getByRole('link', { name: 'Setup' }).click();
  await page.getByRole('button', { name: 'Test sound' }).click();
  expect(page.mock.requests).toContainEqual({ path: '/api/sound/test', body: {} });
});

test('names the next stage for the shooter and draws the flow', async ({ page }) => {
  await expect(page.locator('#headline')).toHaveText('Ready');
  await expect(page.locator('#flow .step.now')).toHaveText('Ready');
  await expect(page.getByRole('button', { name: 'Start Shoot AB' })).toBeVisible();
  await expect(page.locator('#clockGroup')).toBeVisible();
  await expect(page.locator('#clockGroup')).toHaveText(/A\s*B/);
  await expect(page.locator('#clockGroup')).toHaveCSS('color', 'rgb(255, 255, 255)');
  const labels = await page.locator('#flow .step').allTextContents();
  expect(labels.slice(0, 5)).toEqual(['Ready', 'Occupy AB', 'Shoot AB', 'Occupy CD', 'Shoot CD']);

  await page.getByRole('link', { name: 'Setup' }).click();
  await expect(page.locator('#abcdRotation')).toHaveValue('true');
  await expect(page.locator('#preview')).toContainText('AB then CD, rotating each end');
});

test('can turn AB CD rotation off for a single-detail end', async ({ page }) => {
  await page.getByRole('link', { name: 'Setup' }).click();
  await page.locator('#abcdRotation').selectOption('false');
  await page.getByRole('link', { name: 'Field' }).click();
  await expect(page.getByRole('button', { name: /^Start Shoot$/ })).toBeVisible();
  await expect(page.locator('#clockGroup')).toBeHidden();
});

test('second end starts with CD then AB', async ({ page }) => {
  page.mock.state.abcdRotation = true;
  page.mock.state.details = 2;
  page.mock.state.end = 2;
  page.mock.state.detail = 2;
  await page.goto('/');

  await expect(page.getByRole('button', { name: 'Start Shoot CD' })).toBeVisible();
  await expect(page.locator('#clockGroup')).toHaveText(/C\s*D/);
  const labels = await page.locator('#flow .step').allTextContents();
  expect(labels.slice(0, 5)).toEqual(['Ready', 'Occupy CD', 'Shoot CD', 'Occupy AB', 'Shoot AB']);
});

test('occupying CD on an even end does not skip AB in the flow', async ({ page }) => {
  page.mock.state.abcdRotation = true;
  page.mock.state.details = 2;
  page.mock.state.end = 2;
  page.mock.state.detail = 2;
  page.mock.state.phase = 'OCCUPY';
  page.mock.state.light = 'RED';
  page.mock.state.running = true;
  page.mock.state.remainingMs = 10000;
  await page.goto('/');

  await expect(page.locator('#flow .step.now')).toHaveText('Occupy CD');
  await expect(page.locator('#flow .step', { hasText: /^Occupy AB$/ })).toBeVisible();
  await expect(page.locator('#flow .step', { hasText: /^Shoot AB$/ })).toBeVisible();
  await expect(page.locator('#flow .step', { hasText: /^Occupy AB$/ })).not.toHaveClass(/done/);
  await expect(page.locator('#flow .step', { hasText: /^Shoot AB$/ })).not.toHaveClass(/done/);
  await expect(page.getByRole('button', { name: 'Start Shoot AB' })).toBeVisible();
  await expect(page.getByRole('button', { name: 'Score' })).toHaveCount(0);
});

test('fourth end starts with CD then AB like every even end', async ({ page }) => {
  page.mock.state.abcdRotation = true;
  page.mock.state.details = 2;
  page.mock.state.end = 4;
  page.mock.state.detail = 2;
  await page.goto('/');

  await expect(page.getByRole('button', { name: 'Start Shoot CD' })).toBeVisible();
  const labels = await page.locator('#flow .step').allTextContents();
  expect(labels.slice(0, 5)).toEqual(['Ready', 'Occupy CD', 'Shoot CD', 'Occupy AB', 'Shoot AB']);
});

test('third end starts with AB then CD again', async ({ page }) => {
  page.mock.state.abcdRotation = true;
  page.mock.state.details = 2;
  page.mock.state.end = 3;
  page.mock.state.detail = 1;
  await page.goto('/');

  await expect(page.getByRole('button', { name: 'Start Shoot AB' })).toBeVisible();
  await expect(page.locator('#clockGroup')).toHaveText(/A\s*B/);
  const labels = await page.locator('#flow .step').allTextContents();
  expect(labels.slice(0, 5)).toEqual(['Ready', 'Occupy AB', 'Shoot AB', 'Occupy CD', 'Shoot CD']);
});

test('AB CD colour can follow the timer or use a custom colour', async ({ page }) => {
  await page.getByRole('link', { name: 'Setup' }).click();
  await expect(page.locator('#abcdRotation')).toHaveValue('true');
  await expect(page.locator('#abcdFollowTimer')).toHaveValue('false');
  await expect(page.locator('#abcdColour')).toHaveValue('#ffffff');

  await page.evaluate(() => {
    const colour = document.getElementById('abcdColour');
    colour.value = '#3366cc';
    colour.dispatchEvent(new Event('input', { bubbles: true }));
    colour.dispatchEvent(new Event('change', { bubbles: true }));
  });
  expect(page.mock.requests).toContainEqual({
    path: '/api/display',
    body: {
      content: 'CLOCK',
      clockSeconds: true,
      showAbcd: true,
      abcdVertical: true,
      showEndLabels: true,
      abcdFollowTimer: false,
      abcdColour: '#3366cc'
    }
  });

  await page.getByRole('link', { name: 'Field' }).click();
  await expect(page.locator('#clockGroup')).toHaveCSS('color', 'rgb(51, 102, 204)');
  await expect(page.locator('#clock')).toHaveCSS('color', 'rgb(255, 255, 200)');

  await page.getByRole('button', { name: 'Start Shoot AB' }).click();
  await expect(page.locator('#clockGroup')).toHaveCSS('color', 'rgb(51, 102, 204)');
  await expect(page.locator('#clock')).toHaveCSS('color', 'rgb(255, 24, 8)');

  await page.getByRole('link', { name: 'Setup' }).click();
  await page.locator('#abcdFollowTimer').selectOption('true');
  await expect(page.locator('#abcdColour')).toBeDisabled();
  expect(page.mock.requests).toContainEqual({
    path: '/api/display',
    body: {
      content: 'CLOCK',
      clockSeconds: true,
      showAbcd: true,
      abcdVertical: true,
      showEndLabels: true,
      abcdFollowTimer: true,
      abcdColour: '#3366cc'
    }
  });

  await page.getByRole('link', { name: 'Field' }).click();
  await expect(page.locator('#clockGroup')).toHaveCSS('color', 'rgb(255, 24, 8)');
  await expect(page.locator('#clock')).toHaveCSS('color', 'rgb(255, 24, 8)');
});

test('can place AB and CD under the time', async ({ page }) => {
  await page.getByRole('link', { name: 'Setup' }).click();
  await page.locator('#abcdVertical').selectOption('false');
  await page.getByRole('link', { name: 'Field' }).click();

  await expect(page.locator('#clockFace')).toHaveClass(/under/);
  await expect(page.locator('#clockGroup')).toHaveText('AB');
});

test('shows End and Scoring instead of zero when the line stops', async ({ page }) => {
  await page.getByRole('button', { name: 'Start Shoot AB' }).click();
  await page.getByRole('button', { name: 'Start Shoot CD' }).click();
  await page.getByRole('button', { name: 'Stop occupy' }).click();
  await expect(page.locator('#clock')).toHaveText('End 1');

  await page.getByRole('button', { name: 'Score' }).click();
  await expect(page.locator('#clock')).toHaveText('Scoring 1');
});

test('starts a break after scoring the configured number of ends', async ({ page }) => {
  await page.getByRole('link', { name: 'Setup' }).click();
  await page.locator('#breakAfterEnds').fill('1');
  await page.locator('#breakAfterEnds').dispatchEvent('change');
  await page.getByRole('link', { name: 'Field' }).click();

  await page.getByRole('button', { name: 'Start Shoot AB' }).click();
  await page.getByRole('button', { name: 'Start Shoot CD' }).click();
  await page.getByRole('button', { name: 'Stop occupy' }).click();
  await page.getByRole('button', { name: 'Score' }).click();
  await expect(page.getByRole('button', { name: 'Start break' })).toBeVisible();
  await page.getByRole('button', { name: 'Start break' }).click();

  await expect(page.locator('#phase')).toHaveText('BREAK');
  await expect(page.locator('#headline')).toContainText('Break after end 1');
  await expect(page.getByRole('button', { name: 'Start Shoot CD' })).toBeVisible();
});

test('reports a rejected command instead of failing silently', async ({ page }) => {
  await page.route('**/api/control', route =>
    route.fulfill({
      status: 409,
      contentType: 'application/json',
      body: JSON.stringify({ error: 'mode not implemented' })
    })
  );

  await page.getByRole('button', { name: 'Start Shoot AB' }).click();
  await expect(page.locator('#note')).toHaveText('mode not implemented');
});
