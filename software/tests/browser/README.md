# Browser tests

The Playwright tests serve `software/web/index.html` locally and intercept the firmware
API with an in-memory state fixture. They test the real HTML, CSS, and
JavaScript without requiring an ESP32, Wi-Fi, Wokwi, or a paid gateway.
Volume and the test-tone control are part of the timer page.

Install dependencies and Chromium, then run:

```text
npm install
npx playwright install chromium
npm test
```