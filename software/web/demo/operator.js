// Director-console helpers: shooter-facing names, the flow strip, and the
// setup preview. Firmware and the browser demo share this file.
(function (root) {
  const GROUPS = ['AB', 'CD', 'EF', 'GH'];

  function groupName(detail) {
    const index = Math.max(0, (detail || 1) - 1);
    return GROUPS[index] || ('D' + (index + 1));
  }

  function usesAbcd(state) {
    return !!(state && state.abcdRotation && (state.details || 1) > 1);
  }

  function detailCount(state) {
    return Math.min(Math.max(state && state.details ? state.details : 1, 1), 4);
  }

  function firstDetailForEnd(state, end) {
    const count = detailCount(state);
    if (!usesAbcd(state) || count <= 1) return 1;
    return ((Math.max(end || 1, 1) - 1) % count) + 1;
  }

  function nextDetail(detail, details) {
    const count = Math.max(details || 1, 1);
    const current = detail || 1;
    return current >= count ? 1 : current + 1;
  }

  function moreDetails(state) {
    if (!usesAbcd(state)) return false;
    return nextDetail(state.detail, state.details) !== firstDetailForEnd(state, state.end);
  }

  function upcomingFirstDetail(state) {
    const phase = state.phase || 'IDLE';
    if (phase === 'SCORING' || phase === 'FINISHED' || phase === 'BREAK') {
      return firstDetailForEnd(state, (state.end || 1) + 1);
    }
    return firstDetailForEnd(state, state.end);
  }

  function detailOrder(state, end) {
    const count = detailCount(state);
    const first = firstDetailForEnd(state, end);
    const order = [];
    for (let step = 0; step < count; step++) {
      order.push(((first - 1 + step) % count) + 1);
    }
    return order;
  }

  function timerColourCss(state) {
    if (!state) return '#ffffc8';
    if (state.light === 'RED') return '#ff1808';
    if (state.light === 'GREEN') return '#00c81e';
    if (state.light === 'YELLOW') return '#ffb400';
    return '#ffffc8';
  }

  function groupColourCss(state) {
    if (state && state.abcdFollowTimer) return timerColourCss(state);
    return (state && state.abcdColour) || '#ffffff';
  }

  function isAlternating(state) {
    return state.mode === 'IND_ALT' || state.mode === 'TEAM_ALT';
  }

  function isTeam(state) {
    return state.mode === 'TEAM_SIMUL' || state.mode === 'TEAM_ALT' || state.mode === 'MIXED_TEAM';
  }

  function usesWarning(state) {
    return !isAlternating(state) && state.mode !== 'PRACTICE';
  }

  function sideName(side, state) {
    if (isTeam(state)) return side === 2 ? 'Team B' : 'Team A';
    return side === 2 ? 'B' : 'A';
  }

  function clockLabel(ms) {
    const total = Math.ceil(Math.max(ms || 0, 0) / 1000);
    const minutes = Math.floor(total / 60);
    const seconds = total % 60;
    if (minutes === 0) return String(seconds) + ' s';
    return String(minutes) + ':' + String(seconds).padStart(2, '0');
  }

  function clockFace(state, ms) {
    if (state && state.showEndLabels !== false) {
      if (state.phase === 'FINISHED') return 'End ' + (state.end || '');
      if (state.phase === 'SCORING') return 'Scoring ' + (state.end || '');
    }
    const total = Math.ceil(Math.max(ms || 0, 0) / 1000);
    if (!state || state.clockSeconds !== false) return String(total);
    return String(Math.floor(total / 60)).padStart(2, '0') + ':' + String(total % 60).padStart(2, '0');
  }

  function groupOnClock(state) {
    if (!state || state.showAbcd === false || !usesAbcd(state)) return '';
    if (state.showEndLabels !== false && (state.phase === 'FINISHED' || state.phase === 'SCORING')) return '';
    return groupName(state.detail || 1);
  }

  function programTitle(state) {
    const titles = {
      IND_NONALT: 'Individual qualification',
      IND_ALT: 'Individual match, alternating',
      TEAM_SIMUL: 'Team, both sides together',
      TEAM_ALT: 'Team match, alternating',
      MIXED_TEAM: 'Mixed team',
      PRACTICE: 'Practice'
    };
    let title = titles[state.mode] || state.mode || 'Shot clock';
    if (usesAbcd(state)) title += ', ' + groupName(1) + ' then ' + groupName(2) + ', rotating each end';
    if (state.shootOff) title += ' — shoot-off';
    return title;
  }

  function startShootLabel(state, detail) {
    if (state.mode === 'PRACTICE') return 'Start Practice';
    if (usesAbcd(state)) return 'Start Shoot ' + groupName(detail || upcomingFirstDetail(state));
    if (isAlternating(state)) return 'Start Shoot ' + sideName(state.firstShooter || 1, state);
    return 'Start Shoot';
  }

  function running(phase) {
    return phase === 'OCCUPY' || phase === 'SHOOTING' || phase === 'WARNING';
  }

  function flowSteps(state) {
    const phase = state.phase || 'IDLE';
    const steps = [];
    const add = (id, label) => steps.push({ id: id, label: label, current: false, done: false });

    add('ready', 'Ready');
    if (usesAbcd(state)) {
      detailOrder(state, state.end || 1).forEach(function (detail) {
        add('occupy' + detail, 'Occupy ' + groupName(detail));
        add('shoot' + detail, 'Shoot ' + groupName(detail));
      });
    } else if (isAlternating(state)) {
      add('occupy', 'Occupy line');
      add('shootA', 'Shoot ' + sideName(1, state));
      add('shootB', 'Shoot ' + sideName(2, state));
    } else if (state.mode === 'PRACTICE') {
      add('occupy', 'Occupy line');
      add('shoot', 'Practice');
    } else {
      add('occupy', 'Occupy line');
      add('shoot', 'Shoot');
      add('warn', 'Yellow');
    }
    if (state.mode !== 'PRACTICE') add('score', 'Score');
    if (state.breakEnabled !== false && (state.breakAfterEnds || 12) > 0) add('break', 'Break');

    let current = 'ready';
    const detail = state.detail || 1;
    if (phase === 'OCCUPY') {
      current = usesAbcd(state) ? 'occupy' + detail : 'occupy';
    } else if (phase === 'SHOOTING' || phase === 'SUSPENDED') {
      if (usesAbcd(state)) current = 'shoot' + detail;
      else if (isAlternating(state)) current = state.shooter === 2 ? 'shootB' : 'shootA';
      else current = 'shoot';
    } else if (phase === 'WARNING') {
      if (usesAbcd(state)) current = 'shoot' + detail;
      else current = 'warn';
    } else if (phase === 'FINISHED' || phase === 'SCORING') {
      current = state.mode === 'PRACTICE' ? 'ready' : 'score';
    } else if (phase === 'BREAK') {
      current = 'break';
    } else if (phase === 'EMERGENCY') {
      current = 'emergency';
      add('emergency', 'Emergency');
    }

    let seen = false;
    for (let index = 0; index < steps.length; index++) {
      const step = steps[index];
      if (step.id === current) {
        step.current = true;
        seen = true;
      } else if (!seen) {
        step.done = true;
      }
    }
    return steps;
  }

  function situation(state) {
    const phase = state.phase || 'IDLE';
    const who = usesAbcd(state) ? groupName(state.detail || 1) : '';
    const shooter = isAlternating(state) ? sideName(state.shooter || state.firstShooter || 1, state) : '';

    if (phase === 'EMERGENCY') {
      return {
        headline: 'Emergency — all shooting stopped',
        detail: 'Five or more sounds. Clear Emergency when the field is safe.'
      };
    }
    if (phase === 'SUSPENDED') {
      return {
        headline: 'Held — ' + (who || shooter || 'this period') + ' paused',
        detail: 'Set how many arrows have already been shot, then resume. Time is recalculated from what is left.'
      };
    }
    if (phase === 'IDLE') {
      return {
        headline: 'Ready',
        detail: 'Next: ' + startShootLabel(state, upcomingFirstDetail(state)) + '. Two sounds, red, 10 seconds to occupy the line.'
      };
    }
    if (phase === 'OCCUPY') {
      return {
        headline: (who ? who + ' occupying the line' : 'Occupying the line'),
        detail: 'Green and one sound when this reaches zero.'
      };
    }
    if (phase === 'SHOOTING' || phase === 'WARNING') {
      if (isAlternating(state)) {
        const next = sideName(state.shooter === 1 ? 2 : 1, state);
        return {
          headline: shooter + ' shooting',
          detail: 'Start Shoot ' + next + ' when this arrow is gone.'
        };
      }
      if (moreDetails(state)) {
        return {
          headline: who + ' shooting',
          detail: 'Start Shoot ' + groupName(nextDetail(state.detail, state.details)) + ' when ' + who + ' has finished.'
        };
      }
      if (state.mode === 'PRACTICE') {
        return { headline: 'Practice running', detail: 'Stop practice when the session is over.' };
      }
      return {
        headline: (who ? who + ' shooting' : 'Shooting'),
        detail: phase === 'WARNING' ? 'Yellow — 30 seconds left. Stop shooting when the line is done.'
                                    : 'Stop shooting when the line is done.'
      };
    }
    if (phase === 'FINISHED') {
      return {
        headline: 'End ' + (state.end || '') + ' is red',
        detail: 'Score when the line is clear — three sounds. Athletes may then collect arrows.'
      };
    }
    if (phase === 'SCORING') {
      return {
        headline: 'Scoring' + (state.end ? ' — end ' + state.end : ''),
        detail: breakDue(state)
          ? 'Start the break when scoring is finished.'
          : 'Next: ' + startShootLabel(state, upcomingFirstDetail(state)) + ' for the next end.'
      };
    }
    if (phase === 'BREAK') {
      return {
        headline: 'Break after end ' + (state.end || ''),
        detail: 'Start Shoot when the break is over.'
      };
    }
    return { headline: phase, detail: '' };
  }

  function breakDue(state) {
    const after = state.breakAfterEnds || 0;
    return state.breakEnabled !== false && after > 0 && state.end > 0 && state.end % after === 0;
  }

  function actions(state) {
    const phase = state.phase || 'IDLE';
    const list = [];

    if (phase === 'EMERGENCY') {
      list.push({ id: 'clear_emergency', label: 'Clear Emergency', action: 'clear_emergency', primary: true });
      return list;
    }

    if (phase === 'IDLE') {
      list.push({ id: 'start', label: startShootLabel(state, upcomingFirstDetail(state)), action: 'start', primary: true });
    }

    if (phase === 'SCORING') {
      if (breakDue(state)) {
        list.push({ id: 'break', label: 'Start break', action: 'next_end', primary: true });
      } else {
        list.push({
          id: 'start',
          label: startShootLabel(state, upcomingFirstDetail(state)),
          action: 'next_then_start',
          primary: true
        });
      }
    }

    if (phase === 'BREAK') {
      list.push({
        id: 'start',
        label: startShootLabel(state, upcomingFirstDetail(state)),
        action: 'start',
        primary: true
      });
      list.push({ id: 'next', label: 'End break', action: 'next_end' });
    }

    if (phase === 'FINISHED') {
      if (state.mode !== 'PRACTICE') {
        list.push({ id: 'score', label: 'Score', action: 'line_clear', primary: true });
      }
      list.push({
        id: 'next',
        label: startShootLabel(state, upcomingFirstDetail(state)),
        action: 'next_then_start'
      });
    }

    if ((phase === 'SHOOTING' || phase === 'WARNING') && isAlternating(state)) {
      const next = state.shooter === 1 ? 2 : 1;
      list.push({
        id: 'handoff',
        label: 'Start Shoot ' + sideName(next, state),
        action: 'start',
        primary: true
      });
    } else if (running(phase) && moreDetails(state)) {
      list.push({
        id: 'jump',
        label: 'Start Shoot ' + groupName(nextDetail(state.detail, state.details)),
        action: 'stop',
        primary: true
      });
    } else if (phase === 'SHOOTING' || phase === 'WARNING') {
      const label = state.mode === 'PRACTICE' ? 'Stop practice'
        : usesAbcd(state) ? 'Stop shooting ' + groupName(state.detail || 1)
        : 'Stop shooting';
      list.push({ id: 'stop', label: label, action: 'stop', primary: true });
    } else if (phase === 'OCCUPY' && !moreDetails(state)) {
      list.push({ id: 'stop', label: 'Stop occupy', action: 'stop' });
    }

    if (running(phase)) {
      list.push({ id: 'suspend', label: 'Suspend', action: 'suspend' });
    }
    if (phase === 'SUSPENDED') {
      list.push({ id: 'resume', label: 'Resume remaining arrows', action: 'resume', primary: true });
    }

    if (phase === 'IDLE' || phase === 'FINISHED' || phase === 'SCORING' || phase === 'BREAK') {
      list.push({ id: 'reset', label: 'Reset this end', action: 'reset_end' });
    }

    list.push({ id: 'emergency', label: 'Emergency', action: 'emergency', danger: true });
    return list;
  }

  // Live arrow counting is for one athlete or team, not a whole qualification
  // line. Alternating needs it throughout; a hold needs it so resume can apply
  // Art. 11.2.4 from arrows not shot.
  function tracksLiveArrows(state) {
    if (!state || state.mode === 'PRACTICE') return false;
    return isAlternating(state) || (state.phase || '') === 'SUSPENDED';
  }

  function usesArrowDisplay(state) {
    return isAlternating(state);
  }

  function syncDisplayOptions(select, state) {
    if (!select) return;
    const arrows = select.querySelector('option[value="ARROWS"]');
    if (!arrows) return;
    arrows.hidden = !usesArrowDisplay(state) && state.display !== 'ARROWS';
  }

  function auxActions(state) {
    const phase = state.phase || 'IDLE';
    const extras = [];
    const cap = isAlternating(state) ? (state.arrowsPerEnd || 0) * 2 : (state.arrowsPerEnd || 0);
    if (tracksLiveArrows(state)) {
      if ((state.arrowsShot || 0) > 0) {
        extras.push({ id: 'remove_arrow', label: 'Remove one arrow', action: 'remove_arrow' });
      }
      if ((state.arrowsShot || 0) < cap) {
        extras.push({ id: 'add_arrow', label: 'Add one arrow', action: 'add_arrow' });
      }
    }
    if (running(phase) || phase === 'SUSPENDED') {
      extras.push({ id: 'extend', label: 'Add time', action: 'extend' });
    }
    return extras;
  }

  function timingPreview(state) {
    const occupy = '10 s · 2 sounds · red';
    const period = clockLabel(state.periodMs);
    const per = Math.round((state.perArrowMs || 0) / 1000);
    const lines = [
      { label: 'Program', value: programTitle(state) },
      { label: 'Occupy the line', value: occupy }
    ];

    if (state.mode === 'PRACTICE') {
      lines.push({
        label: 'Practice',
        value: clockLabel((state.practiceSeconds || 0) * 1000 || state.periodMs) + ' · 1 sound · green'
      });
    } else if (isAlternating(state)) {
      lines.push({
        label: 'Each turn',
        value: (state.mode === 'TEAM_ALT' ? period + ' banked · 1 sound · green'
                                         : '20 s, one arrow · 1 sound · green')
      });
      lines.push({ label: 'Shoots first', value: sideName(state.firstShooter || 1, state) });
    } else {
      let shoot = period + ' · 1 sound · green';
      if (usesWarning(state)) shoot += ' · yellow at 30 s left';
      lines.push({ label: 'Shooting', value: shoot });
      if (per) lines.push({ label: 'Per arrow', value: per + ' s · ' + (state.arrowsPerEnd || 0) + ' arrows' });
    }

    if (usesAbcd(state)) {
      lines.push({
        label: 'Details',
        value: detailOrder(state, state.end || 1).map(groupName).join(' then ') +
          ', rotating each end · 10 s changeover'
      });
    }

    if (state.mode !== 'PRACTICE') {
      lines.push({ label: 'Score', value: '3 sounds · red, when the line is clear' });
    }
    lines.push({
      label: 'Resume after a hold',
      value: state.resumeOccupy === false ? 'Straight back to shooting · 1 sound'
                                          : 'Replay the 10 s occupy · 1 sound then 2'
    });
    lines.push({ label: 'Emergency', value: '5 or more sounds · red · all shooting stops' });
    if (state.clockSeconds !== false) lines.push({ label: 'Clock on the panel', value: 'Seconds, right aligned' });
    else lines.push({ label: 'Clock on the panel', value: 'Minutes and seconds' });
    if (usesAbcd(state) && state.showAbcd !== false) {
      lines.push({
        label: 'AB / CD on the panel',
        value: state.abcdVertical === false ? 'Under the time' : 'Vertical, left of the time'
      });
      lines.push({
        label: 'AB / CD colour',
        value: state.abcdFollowTimer ? 'Follow the timer' : (state.abcdColour || '#ffffff')
      });
    }
    if (state.showEndLabels !== false) {
      lines.push({ label: 'When an end stops', value: 'End N, then Scoring N' });
    }
    if (state.breakEnabled !== false && (state.breakAfterEnds || 0) > 0) {
      lines.push({
        label: 'Break',
        value: 'After every ' + state.breakAfterEnds + ' ends, ' + (state.breakSeconds || 900) + ' s, after scoring'
      });
    }
    return lines;
  }

  function renderFlow(host, state) {
    if (!host) return;
    host.replaceChildren();
    flowSteps(state).forEach(function (step) {
      const node = document.createElement('span');
      node.className = 'step' + (step.current ? ' now' : '') + (step.done ? ' done' : '');
      node.textContent = step.label;
      host.appendChild(node);
    });
  }

  function renderPreview(host, state) {
    if (!host) return;
    host.replaceChildren();
    timingPreview(state).forEach(function (line) {
      const row = document.createElement('div');
      row.className = 'preview-row';
      const label = document.createElement('span');
      label.textContent = line.label;
      const value = document.createElement('b');
      value.textContent = line.value;
      row.appendChild(label);
      row.appendChild(value);
      host.appendChild(row);
    });
  }

  function renderActions(host, state, onAction) {
    if (!host) return;
    host.replaceChildren();
    actions(state).forEach(function (spec) {
      const button = document.createElement('button');
      button.type = 'button';
      button.textContent = spec.label;
      if (spec.primary) button.className = 'primary';
      if (spec.danger) button.className = 'danger';
      button.dataset.action = spec.action;
      button.onclick = function () { onAction(spec); };
      host.appendChild(button);
    });
  }

  function renderAux(host, state, onAction) {
    if (!host) return;
    host.replaceChildren();
    const extras = auxActions(state);
    extras.forEach(function (spec) {
      if (spec.action === 'extend') return;
      const button = document.createElement('button');
      button.type = 'button';
      button.textContent = spec.label;
      button.dataset.action = spec.action;
      button.onclick = function () { onAction(spec); };
      host.appendChild(button);
    });
    return extras.some(function (spec) { return spec.action === 'extend'; });
  }

  root.Operator = {
    clockFace: clockFace,
    groupOnClock: groupOnClock,
    groupName: groupName,
    timerColourCss: timerColourCss,
    groupColourCss: groupColourCss,
    usesAbcd: usesAbcd,
    isAlternating: isAlternating,
    tracksLiveArrows: tracksLiveArrows,
    usesArrowDisplay: usesArrowDisplay,
    syncDisplayOptions: syncDisplayOptions,
    programTitle: programTitle,
    startShootLabel: startShootLabel,
    breakDue: breakDue,
    flowSteps: flowSteps,
    situation: situation,
    actions: actions,
    auxActions: auxActions,
    timingPreview: timingPreview,
    renderFlow: renderFlow,
    renderPreview: renderPreview,
    renderActions: renderActions,
    renderAux: renderAux
  };
})(typeof window !== 'undefined' ? window : globalThis);
