/* Offline runtime smoke test for the PS5 WebUI; no console required. */
'use strict';
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');

const root = path.resolve(__dirname, '../webui');
const html = fs.readFileSync(path.join(root, 'index.html'), 'utf8');
const ids = [...html.matchAll(/id="([^"]+)"/g)].map(m => m[1]);
const elements = new Map();
let focusId = '';
const element = id => {
  if (!elements.has(id)) {
    const classes = new Set();
    elements.set(id, {
      id, innerHTML: '', textContent: '', value: '', style: {}, dataset: {},
      classList: {
        add: cls => classes.add(cls),
        remove: cls => classes.delete(cls),
        toggle: (cls, condition) => condition ? classes.add(cls) : classes.delete(cls),
        contains: cls => classes.has(cls)
      },
      focus: () => { focusId = id; },
      querySelector: () => ({ focus: () => { focusId = 'poster'; } })
    });
  }
  return elements.get(id);
};
ids.forEach(element);
const listeners = {};
const mockFetch = async url => {
  if (String(url).startsWith('/evo/play?')) return { ok: true, status: 200 };
  const match = /^(top|movies|series)\.json$/.exec(String(url));
  if (!match) throw Error(`Unexpected network URL: ${url}`);
  return { ok: true, json: async () => JSON.parse(fs.readFileSync(path.join(root, match[0]), 'utf8')) };
};
const context = {
  window: {
    innerWidth: 1920, innerHeight: 1080, UAPLAY_TMDB_API_KEY: '', addEventListener() {},
    UAPlayProviders: {
      search: async () => [{ title: 'UAFlix: Test movie', url: 'https://uafix.net/films/test/' }],
      episodes: async () => [],
      resolve: async () => 'https://example.org/source.m3u8'
    }
  },
  document: { getElementById: element, addEventListener: (event, handler) => { listeners[event] = handler; } },
  localStorage: { getItem: () => null, setItem() {} },
  fetch: mockFetch,
  location: { href: 'http://127.0.0.1:8686/' },
  URLSearchParams,
  console
};

function click(btn) {
  listeners.click({ target: { closest: () => ({ dataset: {}, ...btn }) } });
}
function press(key) {
  listeners.keydown({ key, target: { id: '' }, preventDefault() {} });
}
async function main() {
  vm.runInNewContext(fs.readFileSync(path.join(root, 'app.js'), 'utf8'), context, { filename: 'app.js' });
  await new Promise(resolve => setImmediate(resolve));
  await new Promise(resolve => setImmediate(resolve));
  assert.equal(element('app').style.transform, 'scale(1.5)');
  assert.match(element('hero').innerHTML, /Де дивитися/);
  assert.match(element('legend').innerHTML, /✕/);
  assert.ok((element('posters').innerHTML.match(/data-index=/g) || []).length <= 7);

  click({ id: 'more' });
  assert.equal(element('more-panel').classList.contains('hidden'), false);
  assert.match(element('more-panel').innerHTML, /more-grid/);
  press('Escape');
  assert.equal(element('more-panel').classList.contains('hidden'), true);

  for (let i = 0; i < 6; ++i) press('ArrowRight');
  press('ArrowUp');
  assert.equal(focusId, 'more');
  press('Enter');
  assert.equal(element('more-panel').classList.contains('hidden'), false);

  click({ dataset: { index: '0' } });
  assert.equal(element('details').classList.contains('hidden'), false);
  assert.match(element('details').innerHTML, /Відтворити через EVO/);
  await new Promise(resolve => setImmediate(resolve));
  assert.match(element('sources').innerHTML, /data-source="0"/);
  click({ dataset: { source: '0' } });
  await new Promise(resolve => setImmediate(resolve));
  assert.match(element('play-status').textContent, /Потік передано/);
  element('stream-url').value = 'https://example.org/sample.m3u8';
  click({ id: 'play' });
  await new Promise(resolve => setImmediate(resolve));
  assert.match(element('play-status').textContent, /Потік передано/);
  click({ id: 'close' });
  assert.equal(element('details').classList.contains('hidden'), true);
  console.log('UAPlay PS5 WebUI runtime smoke test passed');
}
main().catch(error => { console.error(error); process.exitCode = 1; });
