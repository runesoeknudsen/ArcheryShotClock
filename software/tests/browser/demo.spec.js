const { test, expect } = require('@playwright/test');

test('browser demo loads the WASM core and shows a 32x16 panel', async ({ page }) => {
  await page.goto('/demo/');
  await expect(page.locator('#error')).toBeHidden();
  await expect(page.locator('#clock')).not.toHaveText('--:--');
  await expect(page.locator('#phase')).toHaveText('IDLE');
  const box = await page.locator('#panel').boundingBox();
  expect(box.width).toBeGreaterThan(100);
  expect(box.height).toBeGreaterThan(40);
});

test('start runs the occupy period from the real core', async ({ page }) => {
  await page.goto('/demo/');
  await expect(page.getByRole('button', { name: 'Start Shoot AB' })).toBeVisible();
  await page.getByRole('button', { name: 'Start Shoot AB' }).click();
  await expect(page.locator('#phase')).toHaveText('OCCUPY');
  await expect(page.locator('#lampRed')).toHaveClass(/on/);
  await expect(page.locator('#clock')).toHaveText('10');
});

test('director clock stays in step with the panel while occupy runs', async ({ page }) => {
  await page.goto('/demo/');
  await page.getByRole('button', { name: 'Start Shoot AB' }).click();
  await expect(page.locator('#phase')).toHaveText('OCCUPY');
  await page.waitForTimeout(1500);
  const { clock, panel } = await page.evaluate(() => ({
    clock: document.querySelector('#clock').textContent,
    panel: document.querySelector('#panelText').textContent
  }));
  expect(clock).not.toBe('0');
  expect(panel).toContain(clock);
});

test('what-if size control keeps the firmware 32x16 note', async ({ page }) => {
  await page.goto('/demo/');
  await page.locator('#previewSize').selectOption('64x32');
  await expect(page.locator('#sizeReadout')).toContainText('Firmware stays');
  await expect(page.locator('#sizeReadout')).toContainText('64×32');
});

test('clock format defaults to seconds and can switch to minutes', async ({ page }) => {
  await page.goto('/demo/');
  await expect(page.locator('#clock')).toHaveText('120');
  await expect(page.locator('#panelText')).toHaveText('AB 120');
  await page.getByRole('link', { name: 'Setup' }).click();
  await page.locator('#clockSeconds').selectOption('false');
  await page.getByRole('link', { name: 'Field' }).click();
  await expect(page.locator('#clock')).toHaveText('02:00');
  await expect(page.locator('#panelText')).toHaveText('AB 02:00');
});

test('shows End on the clock when the line is stopped', async ({ page }) => {
  await page.goto('/demo/');
  await page.getByRole('button', { name: 'Start Shoot AB' }).click();
  await expect(page.locator('#phase')).toHaveText('OCCUPY');
  await page.getByRole('button', { name: 'Start Shoot CD' }).click();
  await page.getByRole('button', { name: 'Stop occupy' }).click();
  await expect(page.locator('#phase')).toHaveText('FINISHED');
  await expect(page.locator('#clock')).toHaveText('End 1');
  await expect(page.locator('#panelText')).toHaveText('End 1');
});

test('hides live arrow counting until a hold', async ({ page }) => {
  await page.goto('/demo/');
  await expect(page.locator('#arrowsBox')).toBeHidden();
  await expect(page.getByRole('button', { name: 'Add one arrow' })).toHaveCount(0);

  await page.getByRole('button', { name: 'Start Shoot AB' }).click();
  await expect(page.getByRole('button', { name: 'Suspend' })).toBeVisible();
  await page.getByRole('button', { name: 'Suspend' }).click();
  await expect(page.locator('#arrowsBox')).toBeVisible();
  await expect(page.getByRole('button', { name: 'Add one arrow' })).toBeVisible();
});

test('defaults to AB next to the seconds', async ({ page }) => {
  await page.goto('/demo/');
  await expect(page.locator('#error')).toBeHidden();
  await expect(page.locator('#clock')).not.toHaveText('--:--');
  await expect(page.locator('#abcdRotation')).toHaveValue('true');
  await expect(page.locator('#clockGroup')).toBeVisible();
  await expect(page.locator('#panelText')).toHaveText('AB 120');
  await expect(page.locator('#clockGroup')).toHaveCSS('color', 'rgb(255, 255, 255)');
});

test('second end keeps AB in the flow after CD starts', async ({ page }) => {
  await page.goto('/demo/');

  await page.getByRole('button', { name: 'Start Shoot AB' }).click();
  await expect(page.locator('#phase')).toHaveText('OCCUPY');
  await page.getByRole('button', { name: 'Start Shoot CD' }).click();
  await expect(page.locator('#clockGroup')).toHaveText(/C\s*D/);
  await page.getByRole('button', { name: 'Stop occupy' }).click();
  await expect(page.locator('#phase')).toHaveText('FINISHED');
  await page.getByRole('button', { name: 'Score' }).click();
  await page.getByRole('button', { name: 'Start Shoot CD' }).click();

  await expect(page.locator('#phase')).toHaveText('OCCUPY');
  await expect(page.locator('#clockGroup')).toHaveText(/C\s*D/);
  await expect(page.locator('#end')).toHaveText('2');
  const labels = await page.locator('#flow .step').allTextContents();
  expect(labels.slice(0, 5)).toEqual(['Ready', 'Occupy CD', 'Shoot CD', 'Occupy AB', 'Shoot AB']);
  await expect(page.locator('#flow .step.now')).toHaveText('Occupy CD');
  await expect(page.locator('#flow .step', { hasText: /^Occupy AB$/ })).not.toHaveClass(/done/);
  await expect(page.locator('#flow .step', { hasText: /^Shoot AB$/ })).not.toHaveClass(/done/);
  await expect(page.getByRole('button', { name: 'Start Shoot AB' })).toBeVisible();
  await expect(page.getByRole('button', { name: 'Score' })).toHaveCount(0);

  await page.getByRole('button', { name: 'Start Shoot AB' }).click();
  await expect(page.locator('#phase')).toHaveText('OCCUPY');
  await expect(page.locator('#clockGroup')).toHaveText(/A\s*B/);
  await expect(page.locator('#flow .step.now')).toHaveText('Occupy AB');
  await expect(page.getByRole('button', { name: 'Score' })).toHaveCount(0);
});

async function panelPixel(page, x, y) {
  return page.evaluate(({ x, y }) => {
    const canvas = document.getElementById('panel');
    const ledPx = Number(document.getElementById('ledPx').value);
    const gap = 2;
    const cell = ledPx + gap;
    const px = Math.floor(gap + x * cell + ledPx / 2);
    const py = Math.floor(gap + y * cell + ledPx / 2);
    const pixel = canvas.getContext('2d').getImageData(px, py, 1, 1).data;
    return [pixel[0], pixel[1], pixel[2]];
  }, { x, y });
}

test('LED panel keeps AB white while the timer follows the light', async ({ page }) => {
  await page.goto('/demo/');
  await page.getByRole('button', { name: 'Start Shoot AB' }).click();
  await expect(page.locator('#phase')).toHaveText('OCCUPY');
  await expect(page.locator('#clockGroup')).toHaveCSS('color', 'rgb(255, 255, 255)');
  await expect(page.locator('#clock')).toHaveCSS('color', 'rgb(255, 24, 8)');
  expect(await panelPixel(page, 2, 2)).toEqual([255, 255, 255]);
  expect(await panelPixel(page, 26, 2)).toEqual([255, 24, 8]);
});

test('LED panel uses a custom AB CD colour from settings', async ({ page }) => {
  await page.goto('/demo/');
  await page.getByRole('link', { name: 'Setup' }).click();
  await page.evaluate(() => {
    const colour = document.getElementById('abcdColour');
    colour.value = '#3366cc';
    colour.dispatchEvent(new Event('input', { bubbles: true }));
    colour.dispatchEvent(new Event('change', { bubbles: true }));
  });
  await page.getByRole('link', { name: 'Field' }).click();
  await expect(page.locator('#clockGroup')).toHaveCSS('color', 'rgb(51, 102, 204)');
  await page.getByRole('button', { name: 'Start Shoot AB' }).click();
  await expect(page.locator('#phase')).toHaveText('OCCUPY');
  await expect(page.locator('#clockGroup')).toHaveCSS('color', 'rgb(51, 102, 204)');
  expect(await panelPixel(page, 2, 2)).toEqual([51, 102, 204]);
  expect(await panelPixel(page, 26, 2)).toEqual([255, 24, 8]);
});
