/* Deterministic provider parsing tests: local fixtures, no third-party calls. */
'use strict';
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');

const html = {
  'https://uafix.net/index.php': '<a class="sres-wrap" href="/films/test/"><h2>Тест</h2></a>',
  'https://uaserials.my/index.php': '<a href="/123-test.html"><div class="th-title">Тест</div></a>',
  'https://uafix.net/films/test/': '<iframe src="https://zetvideo.net/vod/demo"></iframe>',
  'https://zetvideo.net/vod/demo': 'file:"https://video.example.org/demo.m3u8"',
  'https://uafix.net/serials/test/': '<a href="/serials/test/season-1-episode-2/">E2</a><a href="/serials/test/season-1-episode-1/">E1</a>'
};
const requests = [];
const context = {
  window: {}, URL, AbortController, setTimeout, clearTimeout, encodeURIComponent,
  fetch: async proxy => {
    assert.ok(String(proxy).startsWith('https://uakino.ofreshofresh.workers.dev/proxy?url='));
    const destination = new URL(proxy).searchParams.get('url');
    requests.push(destination);
    const pathname = destination.includes('/index.php') ? new URL(destination).origin+'/index.php' : destination;
    if (!Object.hasOwn(html, pathname)) throw Error(`Unexpected request: ${destination}`);
    return { ok: true, text: async () => html[pathname] };
  }
};
vm.runInNewContext(fs.readFileSync(path.join(__dirname,'../webui/providers.js'),'utf8'), context);

async function main() {
  const providers = context.window.UAPlayProviders;
  const sources = await providers.search({ title: 'Тест', media_type: 'movie' });
  assert.equal(sources.length, 2);
  assert.equal(sources[0].provider, 'UAFlix');
  assert.equal(await providers.resolve(sources[0]), 'https://video.example.org/demo.m3u8');
  const episodes = await providers.episodes({ url: 'https://uafix.net/serials/test/' });
  assert.equal(episodes.length, 2);
  assert.equal(episodes[0].episode, 1);
  assert.equal(episodes[1].episode, 2);
  assert.ok(requests.length >= 4);
  console.log('UAPlay provider parsing smoke test passed');
}
main().catch(error => { console.error(error); process.exitCode = 1; });
