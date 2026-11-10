/* UAPlay PS5 web frontend proof-of-concept for the EVO native host.
 * Offline catalog only; network catalog and stream resolution are not wired. */
(() => {
  'use strict';
  const $ = id => document.getElementById(id);
  const tabs = ['Головна','Фільми','Серіали','Мультфільми','Вибране'];
  const state = {tab:0,items:[],cursor:0,view:'home',favorites:new Set(),selected:null};
  const image = p => p ? `https://image.tmdb.org/t/p/w342${p}` : '';
  const backdrop = p => p ? `https://image.tmdb.org/t/p/w780${p}` : '';
  const title = m => m.title || m.name || m.original_title || m.original_name || 'Без назви';
  const description = m => m.overview || 'Опис відсутній.';
  const esc = s => String(s).replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
  function renderNav(){
    $('nav').innerHTML=tabs.map((t,i)=>`<button data-tab="${i}" class="${i===state.tab?'active':''}">${t}</button>`).join('');
  }
  function renderHero(){
    const m=state.items[state.cursor] || state.items[0];
    if(!m){$('hero').innerHTML='<p class="status">Каталог завантажується…</p>';return;}
    $('hero').innerHTML=`<div class="backdrop" style="background-image:url('${backdrop(m.backdrop_path)}')"></div><div class="hero-content"><h1>${esc(title(m))}</h1><p>${esc(description(m))}</p><div class="hero-actions"><button class="action" id="watch">Де дивитися</button><button class="action secondary" id="favorite">${state.favorites.has(m.id)?'У вибраному':'У вибране'}</button></div></div>`;
  }
  function renderItems(){
    const items=state.tab===4?state.items.filter(m=>state.favorites.has(m.id)):state.items;
    $('shelf-label').textContent=state.tab===4?'Вибране':state.tab===0?'Популярне':tabs[state.tab];
    $('posters').innerHTML=items.slice(0,9).map((m,i)=>`<button class="poster ${i===state.cursor?'selected':''}" data-index="${i}" aria-label="${esc(title(m))}">${m.poster_path?`<img alt="" src="${image(m.poster_path)}">`:`<div class="fallback">${esc(title(m))}</div>`}</button>`).join('');
  }
  function render(){
    renderNav();renderHero();renderItems();
    const detail=state.view==='details';
    $('details').classList.toggle('hidden',!detail);
    $('legend').innerHTML=`<span><b class="symbol">✕</b> Відкрити</span><span><b class="symbol">◯</b> Назад</span><span><b class="symbol">□</b> Пошук</span><span><b class="symbol">△</b> Вибране</span><span><b class="symbol">L1</b><b class="symbol">R1</b> Розділи</span>`;
    if(detail){const m=state.selected;$('details').innerHTML=`<h1>${esc(title(m))}</h1><div class="meta">${esc((m.release_date||m.first_air_date||'').slice(0,4))} · ${m.media_type==='tv'?'Серіал':'Фільм'}</div><p>${esc(description(m))}</p><label for="stream-url">Пряме посилання на HLS / MP4 (тест EVO)</label><input id="stream-url" placeholder="https://…/video.m3u8" type="url" style="display:block;width:92%;margin:12px 0 20px;padding:14px;font:18px Inter;background:#293944;color:white;border:1px solid #678;border-radius:9px"><button class="action" id="play">Відтворити через EVO</button><button class="action secondary" id="close">Назад</button><p class="status" id="play-status"></p>`;}
  }
  function openItem(index){state.selected=state.items[index];if(!state.selected)return;state.cursor=index;state.view='details';render();}
  async function play(){
    const el=$('play-status');
    const stream=$('stream-url')?.value?.trim();
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
  document.addEventListener('click',e=>{
    const btn=e.target.closest('button');if(!btn)return;
    if(btn.dataset.tab!==undefined){state.tab=Number(btn.dataset.tab);state.cursor=0;state.view='home';render();return;}
    if(btn.dataset.index!==undefined){openItem(Number(btn.dataset.index));return;}
    if(btn.id==='close'){state.view='home';render();return;}
    if(btn.id==='play'||btn.id==='watch'){if(btn.id==='watch')openItem(state.cursor);else play();return;}
    if(btn.id==='favorite'){const m=state.items[state.cursor];if(m){state.favorites.has(m.id)?state.favorites.delete(m.id):state.favorites.add(m.id);render();}}
  });
  document.addEventListener('keydown',e=>{
    if(e.key==='Escape'||e.key==='Backspace'){if(state.view==='details'){state.view='home';render();}e.preventDefault();return;}
    if(state.view==='details')return;
    if(e.key==='ArrowLeft')state.cursor=Math.max(0,state.cursor-1);
    else if(e.key==='ArrowRight')state.cursor=Math.min(state.items.length-1,state.cursor+1);
    else if(e.key==='Enter'){openItem(state.cursor);e.preventDefault();return;}
    else return;
    render();e.preventDefault();
  });
  Promise.all(['top','movies','series'].map(name=>fetch(`${name}.json`).then(r=>r.json()).catch(()=>({results:[]}))))
    .then(groups=>{const seen=new Set();state.items=groups.flatMap(g=>g.results||[]).filter(m=>m.id&&!seen.has(m.id)&&seen.add(m.id));render();});
  render();
})();
