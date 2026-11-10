/* UAPlay-nx UAFlix/UASerials discovery adapted for the PS5 browser frontend.
 * Only public HTML pages are queried; no authentication or DRM is bypassed.
 * Network access depends on the public UAFresh proxy allowing browser CORS. */
(() => {
  'use strict';
  const proxy='https://uakino.ofreshofresh.workers.dev/proxy?url=';
  const sites=[{host:'https://uafix.net',name:'UAFlix',kind:'uaflix'},
               {host:'https://uaserials.my',name:'UASerials',kind:'uaserials'}];
  const mediaPattern=/https?:\/\/[^\s"'<>\\]{8,1900}?\.(?:m3u8|mp4|mpd)(?:\?[^\s"'<>\\]*)?/ig;
  const normalize=s=>String(s||'').toLocaleLowerCase('uk-UA').normalize('NFKD')
    .replace(/[^\p{L}\p{N}]+/gu,' ').trim();
  const looksSeries=m=>m.media_type==='tv'||!!m.first_air_date;
  const htmlText=s=>String(s||'').replace(/<[^>]*>/g,' ').replace(/&amp;/g,'&')
    .replace(/&quot;/g,'"').replace(/&#039;|&apos;/g,"'").replace(/\s+/g,' ').trim();

  function safeUrl(href,base){
    try{
      const url=new URL(href,base);
      return url.protocol==='https:'?url.href:null;
    }catch(_){return null;}
  }
  async function page(url){
    if(!safeUrl(url,url))throw Error('Лише HTTPS-сторінки');
    const controller=typeof AbortController!=='undefined'?new AbortController():null;
    const timer=controller?setTimeout(()=>controller.abort(),15000):null;
    try{
      const r=await fetch(proxy+encodeURIComponent(url),controller?{signal:controller.signal}:{});
      if(!r.ok)throw Error(`UAFresh HTTP ${r.status}`);
      return r.text();
    }finally{if(timer)clearTimeout(timer);}
  }
  function matches(candidate,site,series){
    let url;
    try{url=new URL(candidate);}catch(_){return false;}
    if(url.origin!==site.host)return false;
    if(site.kind==='uaflix'){
      if(!/^\/(films|serials|anime)\/[a-z0-9-]+\/$/.test(url.pathname))return false;
      return series?!url.pathname.startsWith('/films/') : url.pathname.startsWith('/films/');
    }
    return /^\/\d+-[^/]+\.html$/.test(url.pathname);
  }
  function scrapeResults(html,site,series){
    const out=[];const seen=new Set();
    const tags=html.match(/<a\b[^>]*>[^]*?<\/a>/ig)||[];
    for(const tag of tags){
      if(site.kind==='uaflix'&&!/class\s*=\s*["'][^"']*sres-wrap/i.test(tag))continue;
      const href=/\bhref\s*=\s*["']([^"']+)["']/i.exec(tag)?.[1];
      const url=href&&safeUrl(href,site.host);
      if(!url||!matches(url,site,series)||seen.has(url))continue;
      seen.add(url);
      const heading=/<h2[^>]*>([^]*?)<\/h2>/i.exec(tag)?.[1]||
                    /class\s*=\s*["'][^"']*th-title[^"']*["'][^>]*>([^<]*)/i.exec(tag)?.[1];
      out.push({url,title:`${site.name}: ${htmlText(heading||url.split('/').filter(Boolean).pop())}`,provider:site.name});
      if(out.length>=16)break;
    }
    return out;
  }
  function score(source,movie){
    const name=normalize(source.title.split(': ').slice(1).join(': '));
    const translations=[normalize(movie.title||movie.name),normalize(movie.original_title||movie.original_name)].filter(Boolean);
    return Math.max(0,...translations.map(t=>name===t?120:name.includes(t)?80:t.includes(name)?55:0));
  }
  async function search(movie){
    const queries=[movie.title||movie.name,movie.original_title||movie.original_name].filter(Boolean);
    const out=[];const errors=[];
    for(const site of sites){
      let results=[];
      for(const query of [...new Set(queries)]){
        try{
          const url=site.host+'/index.php?do=search&subaction=search&story='+encodeURIComponent(query);
          results=scrapeResults(await page(url),site,looksSeries(movie));
        }catch(error){errors.push(error.message||String(error));}
        if(results.length)break;
      }
      out.push(...results);
    }
    const found=out.sort((a,b)=>score(b,movie)-score(a,movie));
    if(!found.length && errors.length)throw Error('Не вдалося отримати джерела через UAFresh (мережа/CORS).');
    return found;
  }
  function extractMedia(html){
    const matches=html.replace(/\\\//g,'/').match(mediaPattern)||[];
    return matches.find(url=>!/(trailer|youtube)/i.test(url))||null;
  }
  async function resolve(source){
    const direct=extractMedia(source.url);
    if(direct)return direct;
    const seen=new Set();let remaining=5;
    async function walk(url,depth){
      if(depth>2||remaining--<=0||seen.has(url))return null;
      seen.add(url);
      const html=await page(url);
      const media=extractMedia(html);
      if(media)return media;
      const frames=[...html.matchAll(/<iframe\b[^>]*\bsrc\s*=\s*["']([^"']+)["']/ig)]
        .map(match=>safeUrl(match[1],url)).filter(link=>link&&!/(youtube|trailer|doubleclick|google)/i.test(link));
      for(const host of ['zetvideo.net/vod/','hdvbua.pro/','ashdi.vip/']){
        const position=html.indexOf('https://'+host);
        if(position>=0){const match=/^https?:\/\/[^\s"'<>\\]+/.exec(html.slice(position));if(match)frames.push(match[0]);}
      }
      for(const frame of frames.slice(0,4)){
        try{const found=await walk(frame,depth+1);if(found)return found;}catch(_){/* Try next embedded player. */}
      }
      return null;
    }
    const result=await walk(source.url,0);
    if(!result)throw Error('Не знайдено прямого потоку. Спробуй інше джерело.');
    return result;
  }
  async function episodes(source){
    if(!source.url.startsWith('https://uafix.net/serials/'))return [];
    const html=await page(source.url);
    const seen=new Set();const items=[];
    for(const match of html.matchAll(/\bhref\s*=\s*["']([^"']*season-(\d+)-episode-(\d+)\/)["']/gi)){
      const link=safeUrl(match[1],source.url);
      const season=Number(match[2]),episode=Number(match[3]);
      const key=`${season}:${episode}`;
      if(!link||!link.startsWith(source.url)||seen.has(key))continue;
      seen.add(key);items.push({url:link,season,episode,title:`Сезон ${season} · Серія ${episode}`});
      if(items.length>=200)break;
    }
    return items.sort((a,b)=>a.season-b.season||a.episode-b.episode);
  }
  window.UAPlayProviders={search,resolve,episodes};
})();
