// Draws the WASM firmware frame at its real column and row count.
export function createPanel(canvas, engine) {
  const ctx = canvas.getContext('2d');

  const settings = {
    ledPx: 12,
    gapPx: 2,
    pitchMm: 5
  };

  function firmwareSize() {
    return engine.panelSize();
  }

  function resize() {
    const grid = firmwareSize();
    const cell = settings.ledPx + settings.gapPx;
    canvas.width = grid.columns * cell + settings.gapPx;
    canvas.height = grid.rows * cell + settings.gapPx;
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
    const cell = settings.ledPx + settings.gapPx;
    ctx.fillStyle = '#05070a';
    ctx.fillRect(0, 0, canvas.width, canvas.height);

    for (let y = 0; y < grid.rows; y++) {
      for (let x = 0; x < grid.columns; x++) {
        const pixel = pixels[y * grid.columns + x] || 0;
        ctx.fillStyle = pixel ? colour(pixel) : '#10151b';
        ctx.beginPath();
        ctx.roundRect(
          settings.gapPx + x * cell,
          settings.gapPx + y * cell,
          settings.ledPx,
          settings.ledPx,
          Math.max(1, settings.ledPx / 4)
        );
        ctx.fill();
      }
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
    get firmware() {
      return firmwareSize();
    }
  };
}
