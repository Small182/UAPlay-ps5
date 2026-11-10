/* UAPlay PS5 browser frontend hosted by EVO's native playback process. */
(() => {
  'use strict';
  const $ = id => document.getElementById(id);
  const tabs = ['Головна','Фільми','Серіали','Мультфільми','Вибране'];
  const saved = (()=>{try{return JSON.parse(localStorage.getItem('uaplay-favorites')||'[]');}catch(_){return [];}})();
  const state = {tab:0,items:[],all:[],cursor:0,view:'home',previousView:'home',focus:'posters',favorites:new Set(saved),selected:null,sources:[],episodes:[],query:''};
  const key=window.UAPLAY_TMDB_API_KEY||'';
  const tmdbPaths=['trending/movie/week','movie/popular','tv/popular','discover/movie?with_genres=16'];
  const image = p => p ? `https://image.tmdb.org/t/p/w342${p}` : '';
  const backdrop = p => p ? `https://image.tmdb.org/t/p/w780${p}` : '';
  const title = m => m.title || m.name || m.original_title || m.original_name || 'Без назви';
  const description = m => m.overview || 'Опис відсутній.';
  const esc = s => String(s).replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
  const allowed = m => m && m.id && !((m.origin_country||m.production_countries||[]).some(c=>(typeof c==='string'?c:c.iso_3166_1)==='RU'));
  const favoriteId = m => `${m.media_type==='tv'||m.first_air_date?'tv':'movie'}:${m.id}`;
  const storeFavorites=()=>{try{localStorage.setItem('uaplay-favorites',JSON.stringify([...state.favorites]));}catch(_){}};
  function fitViewport(){
    const factor=Math.min(window.innerWidth/1280,window.innerHeight/720);
    $('app').style.transform=`scale(${factor})`;
  }
  window.addEventListener('resize',fitViewport);
  fitViewport();
  function currentItems(){return state.tab===4?state.all.filter(m=>state.favorites.has(favoriteId(m))):state.items;}
  async function getTmdb(path){
    if(!key)return null;
    const separator=path.includes('?')?'&':'?';
    const response=await fetch(`https://api.themoviedb.org/3/${path}${separator}api_key=${encodeURIComponent(key)}&language=uk-UA`);
    if(!response.ok)throw Error(`TMDb HTTP ${response.status}`);
    return response.json();
  }
  async function loadTab(){
    state.cursor=0;
    state.focus='posters';
    if(state.tab===4){state.items=currentItems();render();return;}
    try{
      const data=await getTmdb(tmdbPaths[state.tab]);
      if(data && Array.isArray(data.results)){
        state.items=data.results.filter(allowed).map(m=>({...m,media_type:state.tab===2?'tv':'movie'}));
        const known=new Set(state.all.map(favoriteId));
        for(const m of state.items)if(!known.has(favoriteId(m))){state.all.push(m);known.add(favoriteId(m));}
      }else state.items=state.all;
    }catch(error){console.warn('TMDb unavailable, showing cached catalog',error);state.items=state.all;}
    render();
  }
  async function search(){
    const query=$('search-input').value.trim();
    if(!query)return;
    $('search-panel').classList.add('hidden');
    state.view='home';state.cursor=0;
    try{
      const response=await getTmdb(`search/multi?include_adult=false&query=${encodeURIComponent(query)}`);
      if(response?.results){
        state.items=response.results.filter(m=>allowed(m)&&['movie','tv'].includes(m.media_type));
        const known=new Set(state.all.map(favoriteId));
        for(const m of state.items)if(!known.has(favoriteId(m))){state.all.push(m);known.add(favoriteId(m));}
      }else state.items=state.all.filter(m=>title(m).toLowerCase().includes(query.toLowerCase()));
    }catch(_){state.items=state.all.filter(m=>title(m).toLowerCase().includes(query.toLowerCase()));}
    render();$('shelf-label').textContent=`Пошук: ${query}`;
  }
  function renderNav(){
    $('nav').innerHTML=tabs.map((t,i)=>`<button data-tab="${i}" class="${i===state.tab?'active':''}">${t}</button>`).join('');
  }
  function renderHero(){
    const items=currentItems(),m=items[state.cursor] || items[0];
    if(!m){$('hero').innerHTML='<p class="status">Каталог завантажується…</p>';return;}
    $('hero').innerHTML=`<div class="backdrop" style="background-image:url('${backdrop(m.backdrop_path)}')"></div><div class="hero-content"><h1>${esc(title(m))}</h1><p>${esc(description(m))}</p><div class="hero-actions"><button class="action" id="watch">Де дивитися</button><button class="action secondary" id="favorite">${state.favorites.has(favoriteId(m))?'У вибраному':'У вибране'}</button></div></div>`;
  }
  function renderItems(){
    const items=currentItems();
    $('shelf-label').textContent=state.tab===4?'Вибране':state.tab===0?'Популярне':tabs[state.tab];
    const start=Math.max(0,Math.min(state.cursor-6,items.length-7));
    $('posters').innerHTML=items.slice(start,start+7).map((m,i)=>`<button class="poster ${i+start===state.cursor?'selected':''}" data-index="${i+start}" aria-label="${esc(title(m))}">${m.poster_path?`<img alt="" src="${image(m.poster_path)}">`:`<div class="fallback">${esc(title(m))}</div>`}</button>`).join('');
  }
  function renderMore(){
    const items=currentItems();
    $('more-panel').classList.toggle('hidden',state.view!=='more');
    if(state.view==='more')$('more-panel').innerHTML=`<div class="more-header"><h2>${esc(tabs[state.tab])}: усі фільми та серіали</h2><button id="more-back">◯ Назад</button></div><div class="more-grid">${items.map((m,i)=>`<button class="poster ${i===state.cursor?'selected':''}" data-index="${i}" aria-label="${esc(title(m))}">${m.poster_path?`<img alt="" src="${image(m.poster_path)}">`:`<div class="fallback">${esc(title(m))}</div>`}</button>`).join('')}</div>`;
  }
  function render(){
    renderNav();renderHero();renderItems();
    renderMore();
    const detail=state.view==='details';
    $('details').classList.toggle('hidden',!detail);
    $('legend').innerHTML=`<span><b class="symbol">✕</b> Відкрити</span><span><b class="symbol">◯</b> Назад</span><span><b class="symbol">□</b> Пошук</span><span><b class="symbol">△</b> Вибране</span><span><b class="symbol">L1</b><b class="symbol">R1</b> Розділи</span>`;
    if(detail){const m=state.selected;$('details').innerHTML=`<h1>${esc(title(m))}</h1><div class="meta">${esc((m.release_date||m.first_air_date||'').slice(0,4))} · ${m.media_type==='tv'?'Серіал':'Фільм'}</div><p>${esc(description(m))}</p><div id="sources" class="source-list"><p class="status">Пошук UAFlix / UASerials…</p></div><div id="episodes" class="episode-list"></div><label for="stream-url">Або пряме посилання на HLS / MP4</label><input id="stream-url" placeholder="https://…/video.m3u8" type="url" style="display:block;width:92%;margin:12px 0 20px;padding:14px;font:18px Inter;background:#293944;color:white;border:1px solid #678;border-radius:9px"><button class="action" id="play">Відтворити через EVO</button><button class="action secondary" id="close">Назад</button><p class="status" id="play-status"></p>`;}
  }
  function openItem(index){state.selected=currentItems()[index];if(!state.selected)return;state.cursor=index;state.sources=[];state.episodes=[];state.previousView=state.view;state.view='details';render();loadDetails(state.selected);loadSources(state.selected);}
  async function loadSources(movie){
    if(!window.UAPlayProviders){$('sources').textContent='Джерела не підключені. Можна ввести прямий URL.';return;}
    try{
      const sources=await window.UAPlayProviders.search(movie);
      if(state.selected!==movie||state.view!=='details')return;
      state.sources=sources;
      $('sources').innerHTML=sources.length?`<h3>Де дивитися</h3>${sources.map((s,i)=>`<button class="action secondary source-choice" data-source="${i}">${esc(s.title)}</button>`).join('')}`:'<p class="status">Джерел не знайдено. Можна ввести пряме посилання.</p>';
    }catch(error){if(state.selected===movie&&state.view==='details')$('sources').textContent='Джерела недоступні: '+(error.message||error);}
  }
  async function chooseSource(index){
    const source=state.sources[index],movie=state.selected;
    if(!source)return;
    const el=$('play-status');if(el)el.textContent='Отримуємо потік…';
    try{
      if(movie.media_type==='tv'||movie.first_air_date){
        const episodes=await window.UAPlayProviders.episodes(source);
        if(state.selected!==movie||state.view!=='details')return;
        if(episodes.length){
          state.episodes=episodes;
          $('episodes').innerHTML=`<h3>Вибери серію</h3>${episodes.map((ep,i)=>`<button class="action secondary episode-choice" data-episode="${i}">${esc(ep.title)}</button>`).join('')}`;
          if(el)el.textContent='Обери серію для відтворення.';
          return;
        }
      }
      await playStream(await window.UAPlayProviders.resolve(source));
    }catch(error){if(el)el.textContent='Не вдалося відкрити джерело: '+(error.message||error);}
  }
  async function chooseEpisode(index){
    const episode=state.episodes[index];if(!episode)return;
    const el=$('play-status');if(el)el.textContent='Шукаємо потік серії…';
    try{await playStream(await window.UAPlayProviders.resolve(episode));}
    catch(error){if(el)el.textContent='Не вдалося відкрити серію: '+(error.message||error);}
  }
  async function loadDetails(movie){
    try{
      const kind=movie.media_type==='tv'||movie.first_air_date?'tv':'movie';
      const data=await getTmdb(`${kind}/${movie.id}`);
      if(state.view!=='details'||state.selected!==movie||!data)return;
      const meta=$('details').querySelector('.meta');
      const count=kind==='tv'?`${data.number_of_seasons||0} сезонів · ${data.number_of_episodes||0} серій`:`${data.runtime||0} хв`;
      if(meta)meta.textContent+=` · ${count}`;
    }catch(_){/* Catalog already contains enough data for offline details. */}
  }
  async function playStream(stream){
    const el=$('play-status');
    if(!stream || !/^https?:\/\/[^\s]+$/i.test(stream)){
      if(el)el.textContent='Введи прямий URL потоку HLS або MP4.';
      return;
    }
    if(el)el.textContent='Передаємо потік у EVO…';
    try{
      const query=new URLSearchParams({url:stream,title:title(state.selected),return:location.href});
      const response=await fetch('/evo/play?'+query,{redirect:'error'});
      if(!response.ok)throw new Error('HTTP '+response.status);
      if(el)el.textContent='Потік передано. Очікуємо відкриття EVO Player.';
    }catch(error){if(el)el.textContent='EVO недоступний: '+String(error.message||error);}
  }
  async function play(){await playStream($('stream-url')?.value?.trim());}
  document.addEventListener('click',e=>{
    const btn=e.target.closest('button');if(!btn)return;
    if(btn.dataset.tab!==undefined){state.tab=Number(btn.dataset.tab);state.view='home';loadTab();return;}
    if(btn.dataset.index!==undefined){openItem(Number(btn.dataset.index));return;}
    if(btn.dataset.source!==undefined){chooseSource(Number(btn.dataset.source));return;}
    if(btn.dataset.episode!==undefined){chooseEpisode(Number(btn.dataset.episode));return;}
    if(btn.id==='close'){state.view=state.previousView;render();return;}
    if(btn.id==='more'){state.view='more';render();return;}
    if(btn.id==='more-back'){state.view='home';render();return;}
    if(btn.id==='search-toggle'){$('search-panel').classList.remove('hidden');$('search-input').focus();return;}
    if(btn.id==='search-close'){$('search-panel').classList.add('hidden');return;}
    if(btn.id==='search-submit'){search();return;}
    if(btn.id==='play'||btn.id==='watch'){if(btn.id==='watch')openItem(state.cursor);else play();return;}
    if(btn.id==='favorite'){const m=currentItems()[state.cursor];if(m){const id=favoriteId(m);state.favorites.has(id)?state.favorites.delete(id):state.favorites.add(id);storeFavorites();render();}}
  });
  document.addEventListener('keydown',e=>{
    if(e.target.id==='search-input'){
      if(e.key==='Enter'){search();e.preventDefault();}
      else if(e.key==='Escape'){$('search-panel').classList.add('hidden');e.preventDefault();}
      return;
    }
    if(e.key==='/' || e.key.toLowerCase()==='s'){$('search-panel').classList.remove('hidden');$('search-input').focus();e.preventDefault();return;}
    if(e.key==='Escape'||e.key==='Backspace'){if(state.view==='details'){state.view=state.previousView;render();}else if(state.view==='more'){state.view='home';render();}e.preventDefault();return;}
    if(state.view==='details')return;
    if(state.view==='more'){
      if(e.key==='ArrowLeft')state.cursor=Math.max(0,state.cursor-1);
      else if(e.key==='ArrowRight')state.cursor=Math.min(currentItems().length-1,state.cursor+1);
      else if(e.key==='ArrowUp')state.cursor=Math.max(0,state.cursor-7);
      else if(e.key==='ArrowDown')state.cursor=Math.min(currentItems().length-1,state.cursor+7);
      else if(e.key==='Enter'){openItem(state.cursor);e.preventDefault();return;}
      else return;
      render();$('more-panel').querySelector(`[data-index="${state.cursor}"]`)?.focus();e.preventDefault();return;
    }
    if(e.key==='ArrowUp' && state.cursor>=6){state.focus='more';$('more').focus();e.preventDefault();return;}
    if(e.key==='ArrowDown' && state.focus==='more'){state.focus='posters';render();e.preventDefault();return;}
    if(e.key==='ArrowLeft'){state.focus='posters';state.cursor=Math.max(0,state.cursor-1);}
    else if(e.key==='ArrowRight'){state.focus='posters';state.cursor=Math.min(currentItems().length-1,state.cursor+1);}
    else if(e.key==='PageUp'){state.tab=(state.tab+tabs.length-1)%tabs.length;loadTab();return;}
    else if(e.key==='PageDown'){state.tab=(state.tab+1)%tabs.length;loadTab();return;}
    else if(e.key==='Enter'){if(state.focus==='more'){state.view='more';render();}else openItem(state.cursor);e.preventDefault();return;}
    else return;
    render();$('posters').querySelector(`[data-index="${state.cursor}"]`)?.focus();e.preventDefault();
  });
  Promise.all(['top','movies','series'].map(name=>fetch(`${name}.json`).then(r=>r.json()).catch(()=>({results:[]}))))
    .then(groups=>{const seen=new Set();state.all=groups.flatMap(g=>g.results||[]).filter(m=>allowed(m)&&!seen.has(favoriteId(m))&&seen.add(favoriteId(m)));state.items=state.all;render();loadTab();});
  render();
})();
