'use strict';

/**
 * UAPlay stream-only Stremio addon.
 * Resolves IMDb IDs through Cinemeta and uses the existing UAPlay C catalog
 * implementation in a separate, time-limited Linux process per provider.
 * No Nuvio / WebKit / PS5 browser is required.
 */
const http = require('node:http');
const path = require('node:path');
const { execFile } = require('node:child_process');

const PROVIDERS = ['UAFlix', 'UASerials', 'Eneyida', 'UAFilm', 'UAKino', 'Mikai', 'AnimeON'];
const BRIDGE_PATH = process.env.BRIDGE_PATH || path.join(__dirname, 'bin', 'uaplay_bridge');
const PORT = Number(process.env.PORT || 7000);
const BRIDGE_TIMEOUT_MS = Number(process.env.BRIDGE_TIMEOUT_MS || 21000);
const CACHE_TTL_MS = 10 * 60 * 1000;
const CACHE_MAX = 128;
const cache = new Map();
const pending = new Map();

const manifest = {
  id: 'com.small182.uaplay.sources',
  version: '0.1.0',
  name: 'UAPlay — українські джерела',
  description: 'Пошук потоків UAPlay для фільмів, серіалів та аніме. Потрібен сервер UAPlay Addon.',
  resources: [{ name: 'stream', types: ['movie', 'series'], idPrefixes: ['tt'] }],
  types: ['movie', 'series'],
  idPrefixes: ['tt'],
  catalogs: []
};

function parseId(type, id) {
  if (type === 'movie' && /^tt\d{7,10}$/.test(id))
    return { imdb: id, season: 0, episode: 0 };
  if (type === 'series') {
    const m = /^((?:tt)\d{7,10}):(\d{1,3}):(\d{1,4})$/.exec(id);
    if (m && +m[2] >= 1 && +m[3] >= 1)
      return { imdb: m[1], season: +m[2], episode: +m[3] };
  }
  return null;
}

function json(res, status, obj, cacheSeconds = 0) {
  res.writeHead(status, {
    'Content-Type': 'application/json; charset=utf-8',
    'Access-Control-Allow-Origin': '*',
    'Cache-Control': cacheSeconds ? 'public, max-age=' + cacheSeconds : 'no-store',
    'X-Content-Type-Options': 'nosniff'
  });
  res.end(JSON.stringify(obj));
}

async function fixedFetch(url, timeout = 9000) {
  const response = await fetch(url, {
    headers: { 'Accept': 'application/json' },
    signal: AbortSignal.timeout(timeout)
  });
  if (!response.ok) throw new Error('Metadata API status ' + response.status);
  return response.json();
}

async function getMeta(type, imdb) {
  const data = await fixedFetch('https://v3-cinemeta.strem.io/meta/' + type + '/' + imdb + '.json');
  const m = data && data.meta;
  if (!m || typeof m.name !== 'string' || !m.name.trim())
    throw new Error('Cinemeta has no matching metadata');

  let title = m.name.trim();
  const original = title;
  const year = String(m.releaseInfo || m.year || '').match(/\d{4}/)?.[0] || '';

  // Optional. Resolving Ukrainian TMDb names substantially improves matching
  // against Ukrainian-language provider searches.
  if (process.env.TMDB_API_KEY) {
    try {
      const key = encodeURIComponent(process.env.TMDB_API_KEY);
      const lookup = await fixedFetch('https://api.themoviedb.org/3/find/' + imdb +
        '?external_source=imdb_id&api_key=' + key, 5000);
      const found = type === 'movie' ? lookup.movie_results?.[0] : lookup.tv_results?.[0];
      if (found?.id) {
        const detail = await fixedFetch('https://api.themoviedb.org/3/' +
          (type === 'movie' ? 'movie/' : 'tv/') + found.id + '?language=uk-UA&api_key=' + key, 5000);
        const uk = type === 'movie' ? detail.title : detail.name;
        if (typeof uk === 'string' && uk.trim()) title = uk.trim();
      }
    } catch {
      // Ukrainian localization is best-effort. Cinemeta remains usable.
    }
  }
  return { title: title.slice(0, 190), original: original.slice(0, 190), year };
}

function runBridge(index, type, meta, season, episode, bridgePath = BRIDGE_PATH) {
  const args = [type, meta.title, meta.original, meta.year || '',
    String(season), String(episode), String(index)];
  return new Promise(resolve => {
    execFile(bridgePath, args, {
      timeout: BRIDGE_TIMEOUT_MS,
      maxBuffer: 512 * 1024,
      windowsHide: true
    }, (error, stdout) => {
      if (error) return resolve([]);
      try {
        const parsed = JSON.parse(stdout);
        return resolve(Array.isArray(parsed.streams) ? parsed.streams : []);
      } catch {
        return resolve([]);
      }
    });
  });
}

function sanitizeStreams(groups) {
  const seen = new Set();
  const streams = [];
  for (const group of groups) {
    for (const s of group) {
      if (!s || typeof s.url !== 'string' || !/^https?:\/\/[^/\s]+/.test(s.url)) continue;
      // Keep source choices distinct while collapsing exact duplicate URLs.
      if (seen.has(s.url)) continue;
      seen.add(s.url);
      const stream = {
        name: String(s.name || 'UAPlay').slice(0, 100),
        title: String(s.title || '').slice(0, 240),
        url: s.url
      };
      if (Array.isArray(s.subtitles)) {
        const subs = s.subtitles.filter(v =>
          v && typeof v.url === 'string' && /^https?:\/\//.test(v.url)
        ).slice(0, 12);
        if (subs.length) stream.subtitles = subs.map((v, n) => ({
          id: 'uaplay-' + n, lang: String(v.lang || 'ukr').slice(0, 16), url: v.url
        }));
      }
      streams.push(stream);
    }
  }
  return streams.slice(0, 60);
}

async function resolveStream(type, id, dependencies = {}) {
  const parsed = parseId(type, id);
  if (!parsed) return { streams: [] };
  const meta = await (dependencies.getMeta || getMeta)(type, parsed.imdb);
  const runner = dependencies.runBridge || runBridge;
  const results = await Promise.all(PROVIDERS.map((_, i) =>
    runner(i + 1, type, meta, parsed.season, parsed.episode)
      .catch(() => [])
  ));
  return { streams: sanitizeStreams(results) };
}

function createHandler(dependencies = {}) {
  const resolver = dependencies.resolveStream || ((type, id) => resolveStream(type, id, dependencies));
  return async (req, res) => {
    if (req.method === 'OPTIONS') {
      res.writeHead(204, { 'Access-Control-Allow-Origin': '*',
        'Access-Control-Allow-Methods': 'GET, OPTIONS', 'Access-Control-Allow-Headers': 'Content-Type' });
      return res.end();
    }
    if (req.method !== 'GET') return json(res, 405, { error: 'Method not allowed' });
    let pathname;
    try { pathname = new URL(req.url, 'http://localhost').pathname; }
    catch { return json(res, 400, { error: 'Invalid URL' }); }

    if (pathname === '/manifest.json') return json(res, 200, manifest, 3600);
    if (pathname === '/health') return json(res, 200, { ok: true, addon: manifest.id });
    const match = /^\/stream\/(movie|series)\/([^/]+)\.json$/.exec(pathname);
    if (!match) return json(res, 404, { error: 'Not found' });
    const type = match[1], id = match[2];
    if (!parseId(type, id)) return json(res, 200, { streams: [] }, 60);
    const key = type + ':' + id;
    const saved = cache.get(key);
    if (saved && Date.now() < saved.expires) return json(res, 200, saved.value, 600);
    if (pending.size >= 4 && !pending.has(key)) return json(res, 503, { streams: [] });
    if (!pending.has(key)) {
      const task = Promise.resolve().then(() => resolver(type, id))
        .then(value => {
          const response = value && Array.isArray(value.streams) ? value : { streams: [] };
          if (cache.size >= CACHE_MAX) cache.delete(cache.keys().next().value);
          cache.set(key, { value: response, expires: Date.now() + CACHE_TTL_MS });
          return response;
        })
        .catch(err => {
          console.error('UAPlay resolve:', String(err.message || err));
          return { streams: [] };
        })
        .finally(() => pending.delete(key));
      pending.set(key, task);
    }
    return json(res, 200, await pending.get(key), 60);
  };
}

if (require.main === module) {
  if (!Number.isInteger(PORT) || PORT < 1 || PORT > 65535) throw new Error('Invalid PORT');
  http.createServer(createHandler()).listen(PORT, '0.0.0.0', () => {
    console.log('UAPlay Stremio addon: http://0.0.0.0:' + PORT + '/manifest.json');
  });
}
module.exports = { manifest, parseId, getMeta, runBridge, sanitizeStreams, resolveStream, createHandler };
