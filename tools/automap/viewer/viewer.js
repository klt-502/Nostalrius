/* Offline, read-only geometry browser. No network control or game-server connection. */
(() => {
  'use strict';
  const clamp = (v, lo, hi) => Math.max(lo, Math.min(hi, v));
  const parseCoordinates = text => {
    const match = text.trim().match(/^(?:\/gotopos\s+)?(\d+)\s*[, ]\s*(\d+)\s*[, ]\s*(\d+)$/i);
    if (!match) return null;
    const p = match.slice(1).map(Number);
    return p[0] <= 65535 && p[1] <= 65535 && p[2] <= 15 ? p : null;
  };
  const worldAt = (px, py, cx, cy, scale, width, height) => [Math.floor(cx+(px-width/2)/scale), Math.floor(cy+(py-height/2)/scale)];
  const commandFor = p => `/gotopos ${p[0]},${p[1]},${p[2]}`;
  if (typeof module !== 'undefined') module.exports = {parseCoordinates,worldAt,commandFor};
  if (typeof document === 'undefined') return;
  const data = window.ZEDAS_MAP;
  const el = id => document.getElementById(id);
  const canvas = el('map'), ctx = canvas.getContext('2d');
  const floors = Object.keys(data.report.floors).map(Number).sort((a,b)=>a-b);
  const state = {x:0,y:0,z:floors.includes(7)?7:floors[0],scale:2,selected:null,entity:null,matches:[],hover:null};
  const images = new Map();
  let width=1,height=1,scheduled=false,drag=null;
  const drawSoon = () => { if (!scheduled) { scheduled=true; requestAnimationFrame(draw); } };
  for (const z of floors) { const opt=document.createElement('option');opt.value=z;opt.textContent=`Z ${z}`;el('floor').append(opt); }
  const screen = (x,y) => [(x-state.x)*state.scale+width/2,(y-state.y)*state.scale+height/2];
  function imageFor(name) {
    if (!images.has(name)) {
      const img=new Image(); img.onload=drawSoon;
      img.src=`tiles/${name}.png`;images.set(name,img);
    }
    const img=images.get(name);images.delete(name);images.set(name,img);
    while(images.size>160) images.delete(images.keys().next().value);
    return img;
  }
  function draw() {
    scheduled=false; const dpr=window.devicePixelRatio||1;
    ctx.setTransform(dpr,0,0,dpr,0,0);ctx.imageSmoothingEnabled=false;
    ctx.fillStyle='#0d1118';ctx.fillRect(0,0,width,height);
    for (const [bx,by,z] of data.chunks) {
      if(z!==state.z)continue;
      const [x,y]=screen(bx*256,by*256), size=256*state.scale;
      if(x>width||y>height||x+size<0||y+size<0)continue;
      const name=String(bx).padStart(3,'0')+String(by).padStart(3,'0')+String(z).padStart(2,'0');
      const img=imageFor(name);if(img.complete&&img.naturalWidth)ctx.drawImage(img,x,y,size,size);
    }
    if(state.entity?.kind==='house') {
      ctx.fillStyle='#7ee8fc80';ctx.strokeStyle='#7ee8fc';
      for(const [x,y,z] of state.entity.footprint)if(z===state.z){const [sx,sy]=screen(x,y);ctx.fillRect(sx,sy,state.scale,state.scale);}
    }
    ctx.fillStyle='#81e2f0';
    for(const e of state.matches.slice(0,5000)) {
      if(e.position[2]!==state.z)continue;const [x,y]=screen(e.position[0]+.5,e.position[1]+.5);
      if(x<0||y<0||x>width||y>height)continue;ctx.beginPath();ctx.arc(x,y,3,0,Math.PI*2);ctx.fill();
    }
    if(state.selected){const [x,y]=screen(state.selected[0]+.5,state.selected[1]+.5);ctx.strokeStyle='#fff0a8';ctx.lineWidth=2;ctx.beginPath();ctx.moveTo(x-10,y);ctx.lineTo(x+10,y);ctx.moveTo(x,y-10);ctx.lineTo(x,y+10);ctx.stroke();}
    el('scale').textContent=`${state.scale.toFixed(1)}×`;el('floor').value=state.z;
  }
  function resize(){const box=canvas.getBoundingClientRect();width=box.width;height=box.height;const dpr=window.devicePixelRatio||1;canvas.width=Math.round(width*dpr);canvas.height=Math.round(height*dpr);drawSoon();}
  function fit(){const b=data.report.bounds[state.z];state.x=(b[0]+b[2]+1)/2;state.y=(b[1]+b[3]+1)/2;state.scale=clamp(Math.min(width/(b[2]-b[0]+1),height/(b[3]-b[1]+1))*.92,.08,24);drawSoon();}
  function select(p,entity=null){state.selected=p;state.entity=entity;el('position').textContent=p.join(', ');el('command').value=commandFor(p);el('copy').disabled=false;el('copy-status').textContent='Copies text only. No server connection.';el('entity').textContent=entity?`${entity.kind}: ${entity.name}${entity.identity!=null?' · ID '+entity.identity:''} · ${entity.source}${entity.group?' · group '+entity.group+', slot '+entity.slot:''}${entity.kind==='house'?' · live ownership unknown':''}`:'Source tile coordinate · confirm placement in-game.';drawSoon();}
  function jump(p,entity=null){if(!floors.includes(p[2])){el('coordinate-error').textContent='That floor has no source tiles.';return;}el('coordinate-error').textContent='';[state.x,state.y,state.z]=p;state.x+=.5;state.y+=.5;state.scale=Math.max(2,state.scale);select(p,entity);}
  function go(){const p=parseCoordinates(el('coordinates').value);if(p)jump(p);else el('coordinate-error').textContent='Use X,Y,Z; X/Y 0–65535, Z 0–15.';}
  function search(){const q=el('search').value.trim().toLocaleLowerCase();state.matches=q?data.entities.filter(e=>`${e.name} ${e.kind} ${e.identity??''}`.toLocaleLowerCase().includes(q)):[];const list=el('results');list.replaceChildren();el('matches').textContent=q?`${state.matches.length.toLocaleString()} matches · first 200 listed${state.matches.length>5000?' · first 5,000 marked':''}`:'Search towns, waypoints, houses, NPC homes or monster spawns.';for(const entity of state.matches.slice(0,200)){const b=document.createElement('button'),name=document.createElement('strong'),detail=document.createElement('small');name.textContent=entity.name;detail.textContent=`${entity.kind}${entity.identity!=null?' #'+entity.identity:''} · ${entity.position.join(', ')}${entity.seconds?' · '+entity.seconds+' sec':''}`;b.append(name,detail);b.addEventListener('click',()=>jump(entity.position,entity));list.append(b);}drawSoon();}
  el('search').addEventListener('input',search);el('go').addEventListener('click',go);el('coordinates').addEventListener('keydown',e=>{if(e.key==='Enter')go();});
  el('floor').addEventListener('change',()=>{state.z=Number(el('floor').value);state.selected=state.entity=null;el('position').textContent='Click a tile';el('command').value='';el('copy').disabled=true;drawSoon();});
  el('fit').addEventListener('click',fit);
  function zoom(factor,px=width/2,py=height/2){const old=state.scale;state.scale=clamp(old*factor,.08,24);state.x+=(px-width/2)*(1/old-1/state.scale);state.y+=(py-height/2)*(1/old-1/state.scale);drawSoon();}
  el('zoom-in').addEventListener('click',()=>zoom(1.5));el('zoom-out').addEventListener('click',()=>zoom(1/1.5));
  canvas.addEventListener('wheel',e=>{e.preventDefault();const b=canvas.getBoundingClientRect();zoom(Math.exp(-e.deltaY*.0015),e.clientX-b.left,e.clientY-b.top);},{passive:false});
  canvas.addEventListener('pointerdown',e=>{if(e.button!==0)return;drag={x:e.clientX,y:e.clientY,cx:state.x,cy:state.y,moved:false};canvas.setPointerCapture(e.pointerId);});
  canvas.addEventListener('pointermove',e=>{const b=canvas.getBoundingClientRect();const p=worldAt(e.clientX-b.left,e.clientY-b.top,state.x,state.y,state.scale,width,height);state.hover=p;el('cursor').textContent=`X ${p[0]} · Y ${p[1]} · Z ${state.z}`;if(drag){if(Math.hypot(e.clientX-drag.x,e.clientY-drag.y)>4)drag.moved=true;if(drag.moved){state.x=drag.cx-(e.clientX-drag.x)/state.scale;state.y=drag.cy-(e.clientY-drag.y)/state.scale;drawSoon();}}});
  canvas.addEventListener('pointerup',e=>{if(!drag)return;const moved=drag.moved;drag=null;if(canvas.hasPointerCapture(e.pointerId))canvas.releasePointerCapture(e.pointerId);if(!moved){const b=canvas.getBoundingClientRect(),p=worldAt(e.clientX-b.left,e.clientY-b.top,state.x,state.y,state.scale,width,height);if(p.every(v=>v>=0&&v<=65535))select([...p,state.z]);}});
  canvas.addEventListener('pointercancel',()=>{drag=null;});
  canvas.addEventListener('keydown',e=>{const dirs={ArrowLeft:[-1,0],ArrowRight:[1,0],ArrowUp:[0,-1],ArrowDown:[0,1]};if(dirs[e.key]){e.preventDefault();state.x+=dirs[e.key][0]*64/state.scale;state.y+=dirs[e.key][1]*64/state.scale;drawSoon();}});
  el('copy').addEventListener('click',async()=>{try{if(navigator.clipboard&&window.isSecureContext)await navigator.clipboard.writeText(el('command').value);else{el('command').focus();el('command').select();if(!document.execCommand('copy'))throw Error('clipboard unavailable');}el('copy-status').textContent='Copied. Paste into your GM character to teleport.';}catch{el('command').focus();el('command').select();el('copy-status').textContent='Text selected. Press Ctrl+C to copy.';}});
  const hash=Object.entries(data.report.sources).find(([name])=>name.endsWith('.otbm'))?.[1]||'';
  el('source').textContent=`${data.report.tiles.toLocaleString()} source tiles · ${floors.length} floors\nMap SHA-256 ${hash.slice(0,16)}…`;
  window.addEventListener('resize',resize);resize();search();const town=data.entities.find(e=>e.kind==='town'&&e.name.toLowerCase()==='ankrahmun');if(town)jump(town.position,town);else fit();
})();
