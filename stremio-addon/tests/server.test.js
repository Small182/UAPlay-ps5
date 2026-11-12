'use strict';
const { test } = require('node:test');
const assert = require('node:assert/strict');
const http = require('node:http');
const { manifest, parseId, sanitizeStreams, resolveStream, createHandler } = require('../server');

test('manifest supports both IMDb movie and episode stream requests', () => {
  assert.equal(manifest.resources[0].name, 'stream');
  assert.deepEqual(manifest.types, ['movie', 'series']);
  assert.deepEqual(parseId('movie','tt1254207'), { imdb:'tt1254207', season:0, episode:0 });
  assert.deepEqual(parseId('series','tt0898266:9:17'), { imdb:'tt0898266', season:9, episode:17 });
  assert.equal(parseId('movie','tt1254207:1:2'), null);
  assert.equal(parseId('series','tt1254207:0:2'), null);
  assert.equal(parseId('movie','../etc/passwd'), null);
});
test('resolves all seven UAPlay providers and drops repeated/invalid streams', async () => {
  const called = [];
  const result = await resolveStream('series','tt0898266:1:2', {
    getMeta: async () => ({ title:'Тест', original:'Test', year:'2024' }),
    runBridge: async (provider, type, meta, season, episode) => {
      called.push([provider, type, meta.title, season, episode]);
      return [{ name:'UAPlay', title:'Українською', url:'https://cdn.example/video.mp4' },
              { name:'Bad', url:'file:///etc/passwd' }];
    }
  });
  assert.equal(called.length, 7);
  assert.equal(result.streams.length, 1);
  assert.equal(result.streams[0].url, 'https://cdn.example/video.mp4');
});
test('HTTP endpoints serve manifest, empty malformed request and stream payload', async (t) => {
  const server = http.createServer(createHandler({
    resolveStream: async () => ({ streams: [{ name:'UAPlay', url:'https://example.org/video.mp4' }] })
  }));
  await new Promise(resolve => server.listen(0,'127.0.0.1',resolve));
  t.after(() => new Promise(resolve => server.close(resolve)));
  const base = 'http://127.0.0.1:' + server.address().port;
  const manifestResult = await fetch(base + '/manifest.json').then(r=>r.json());
  assert.equal(manifestResult.id, manifest.id);
  const bad = await fetch(base + '/stream/movie/not-imdb.json').then(r=>r.json());
  assert.deepEqual(bad, {streams:[]});
  const ok = await fetch(base + '/stream/movie/tt1254207.json').then(r=>r.json());
  assert.equal(ok.streams[0].name,'UAPlay');
});
