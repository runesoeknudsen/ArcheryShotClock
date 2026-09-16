// Draws the WASM 32x16 frame and the size-conversation overlays.
export function createPanel(canvas, engine) {
  const ctx = canvas.getContext('2d');
  const firmware = engine.panelSize();

  const settings = {
    ledPx: 12,
    gapPx: 2,
    pitchMm: 5,
    preview: '32x16'
  };

  function gridSize() {
    if (settings.preview === '64x32') return { columns: 64, rows: 32 };
    if (settings.preview === '64x16') return { columns: 64, rows: 16 };
    return { columns: firmware.columns, rows: firmware.rows };
  }

  function resize() {
    const grid = gridSize();
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
    const grid = gridSize();
    const pixels = engine.pixels();
    const cell = settings.ledPx + settings.gapPx;
    ctx.fillStyle = '#05070a';
    ctx.fillRect(0, 0, canvas.width, canvas.height);

    const scaleX = grid.columns / firmware.columns;
    const scaleY = grid.rows / firmware.rows;

    for (let y = 0; y < grid.rows; y++) {
      for (let x = 0; x < grid.columns; x++) {
        const srcX = Math.floor(x / scaleX);
        const srcY = Math.floor(y / scaleY);
        const pixel = pixels[srcY * firmware.columns + srcX] || 0;
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
    const grid = gridSize();
    const firmwareMm = physicalMm(firmware.columns, firmware.rows);
    const previewMm = physicalMm(grid.columns, grid.rows);
    const same = grid.columns === firmware.columns && grid.rows === firmware.rows;
    return {
      firmware: `${firmware.columns}×${firmware.rows}`,
      preview: `${grid.columns}×${grid.rows}`,
      pitchMm: settings.pitchMm,
      firmwareMm,
      previewMm,
      same
    };
  }

  resize();

  return {
    settings,
    resize,
    draw,
    summary,
    firmware
  };
}
