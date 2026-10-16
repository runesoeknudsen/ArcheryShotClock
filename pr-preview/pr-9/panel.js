// Draws the WASM firmware frame at its real column and row count.
export const REFERENCE_PITCH_MM = 5;
export const LED_PX_MIN = 0.5;
export const LED_PX_MAX = 32;

export function clampLedPx(value) {
  return Math.min(LED_PX_MAX, Math.max(LED_PX_MIN, value));
}

export function cellMetrics(ledPx, pitchMm) {
  const pitch = pitchMm > 0 ? pitchMm : REFERENCE_PITCH_MM;
  const cell = Math.max(0.25, ledPx * (pitch / REFERENCE_PITCH_MM));
  const gap = Math.max(0.15, cell * 0.18);
  const disc = Math.max(0.2, cell - gap);
  return { cell, gap, disc };
}

export function ledPxForWidth(columns, pitchMm, availableWidth) {
  const cols = Math.max(1, columns);
  const cell = Math.max(0.25, availableWidth / (cols + 0.18));
  const pitch = pitchMm > 0 ? pitchMm : REFERENCE_PITCH_MM;
  return clampLedPx(cell * REFERENCE_PITCH_MM / pitch);
}

const STACKED_P5 = {
  P5_64X64: 2,
  P5_96X64: 3,
  P5_128X64: 4,
  P5_160X64: 5
};

export function moduleSeams(preset, orientation) {
  const count = STACKED_P5[preset];
  if (!count) return [];
  const seams = [];
  for (let index = 1; index < count; index++) {
    const at = index * 32;
    if (orientation === 'PORTRAIT') seams.push({ x: 0, y: at, w: 64, h: 0 });
    else seams.push({ x: at, y: 0, w: 0, h: 64 });
  }
  return seams;
}

export function createPanel(canvas, engine) {
  const ctx = canvas.getContext('2d');

  const settings = {
    ledPx: 8,
    pitchMm: 5,
    preset: 'LED_32X16',
    orientation: 'LANDSCAPE'
  };

  function firmwareSize() {
    return engine.panelSize();
  }

  function metrics() {
    return cellMetrics(settings.ledPx, settings.pitchMm);
  }

  function rememberMetrics(drawn) {
    canvas.dataset.cell = String(drawn.cell);
    canvas.dataset.gap = String(drawn.gap);
    canvas.dataset.disc = String(drawn.disc);
  }

  function resize() {
    const grid = firmwareSize();
    const drawn = metrics();
    canvas.width = Math.max(1, Math.round(grid.columns * drawn.cell + drawn.gap));
    canvas.height = Math.max(1, Math.round(grid.rows * drawn.cell + drawn.gap));
    rememberMetrics(drawn);
  }

  function colour(pixel) {
    const r = (pixel >> 16) & 255;
    const g = (pixel >> 8) & 255;
    const b = pixel & 255;
    return `rgb(${r},${g},${b})`;
  }

  function draw() {
    const grid = firmwareSize();
    const pixels = engine.pixels();
    const drawn = metrics();
    rememberMetrics(drawn);
    ctx.fillStyle = '#05070a';
    ctx.fillRect(0, 0, canvas.width, canvas.height);

    for (let y = 0; y < grid.rows; y++) {
      for (let x = 0; x < grid.columns; x++) {
        const pixel = pixels[y * grid.columns + x] || 0;
        ctx.fillStyle = pixel ? colour(pixel) : '#10151b';
        const x0 = drawn.gap + x * drawn.cell;
        const y0 = drawn.gap + y * drawn.cell;
        if (drawn.disc >= 1.5 && typeof ctx.roundRect === 'function') {
          ctx.beginPath();
          ctx.roundRect(x0, y0, drawn.disc, drawn.disc, drawn.disc / 4);
          ctx.fill();
        } else {
          ctx.fillRect(x0, y0, drawn.disc, drawn.disc);
        }
      }
    }
    drawSeams(drawn);
  }

  function drawSeams(drawn) {
    const seams = moduleSeams(settings.preset, settings.orientation);
    if (!seams.length) return;
    ctx.strokeStyle = 'rgba(255,150,56,0.55)';
    ctx.lineWidth = Math.max(1, drawn.gap);
    for (const seam of seams) {
      const x0 = drawn.gap + seam.x * drawn.cell;
      const y0 = drawn.gap + seam.y * drawn.cell;
      ctx.beginPath();
      if (seam.w) {
        ctx.moveTo(x0, y0);
        ctx.lineTo(x0 + seam.w * drawn.cell, y0);
      } else {
        ctx.moveTo(x0, y0);
        ctx.lineTo(x0, y0 + seam.h * drawn.cell);
      }
      ctx.stroke();
    }
  }

  function physicalMm(columns, rows) {
    return { width: columns * settings.pitchMm, height: rows * settings.pitchMm };
  }

  function summary() {
    const grid = firmwareSize();
    const size = physicalMm(grid.columns, grid.rows);
    return {
      firmware: `${grid.columns}×${grid.rows}`,
      preview: `${grid.columns}×${grid.rows}`,
      pitchMm: settings.pitchMm,
      firmwareMm: size,
      previewMm: size,
      same: true
    };
  }

  resize();

  return {
    settings,
    resize,
    draw,
    summary,
    metrics,
    get firmware() {
      return firmwareSize();
    }
  };
}
