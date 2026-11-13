/* SPDX-License-Identifier: MIT */
(() => {
'use strict';
const $ = id => document.getElementById(id);
const canvas = $('stage'), ctx=canvas.getContext('2d');
let profile=structuredClone(tasDefaultPreset), original=structuredClone(profile), selected=0, dragging=false, yaw=0;
const channels=new Set(['L','R','M','FL','FR','FC','LFE','SL','SR','BL','BR']);
const clamp=(n,min,max)=>Math.max(min,Math.min(max,n));
const speakers=()=>profile.layout.speakers;
const current=()=>speakers()[selected];
function validate(p){
  if(!p||p.truckerAuxPreset!==1||!p.layout||!Array.isArray(p.layout.speakers))throw Error('No es un preset Trucker AUX compatible');
  if(p.layout.speakers.length<1||p.layout.speakers.length>16)throw Error('Se permiten entre 1 y 16 altavoces');
  const ids=new Set();
  for(const s of p.layout.speakers){
    if(typeof s.id!=='string'||ids.has(s.id))throw Error('ID duplicado/invalido');ids.add(s.id);
    if(!channels.has(s.channel))throw Error('Canal no compatible: '+s.channel);
    if(!Array.isArray(s.position)||s.position.length!==3||s.position.some(v=>typeof v!=='number'||!Number.isFinite(v)||Math.abs(v)>5))throw Error('Posición inválida');
    if(!Number.isFinite(s.gainDb)||s.gainDb < -100||s.gainDb>6)throw Error('Ganancia fuera de rango');
  }
}
function scenePoint(s){return {x:canvas.width/2+s.position[0]*210,y:canvas.height/2+s.position[2]*220};}
function draw(){
  let w=canvas.width;ctx.clearRect(0,0,w,canvas.height);
  ctx.fillStyle='#1c2c44';ctx.strokeStyle='#657891';ctx.lineWidth=3;
  ctx.beginPath();ctx.roundRect(104,48,512,520,42);ctx.fill();ctx.stroke();
  ctx.strokeStyle='#69819d';ctx.lineWidth=2;ctx.beginPath();ctx.moveTo(150,105);ctx.quadraticCurveTo(w/2,42,570,105);ctx.stroke();
  ctx.fillStyle='#91accf';ctx.font='bold 19px Segoe UI';ctx.textAlign='center';ctx.fillText('TABLERO',360,86);
  ctx.fillStyle='#34465c';ctx.fillRect(175,284,143,114);ctx.fillRect(407,284,143,114);
  const hx=276,hy=300;ctx.lineWidth=4;ctx.strokeStyle='#38d1a8';ctx.beginPath();ctx.arc(hx,hy,25,0,2*Math.PI);ctx.stroke();
  const t=yaw*Math.PI/180;ctx.beginPath();ctx.moveTo(hx,hy);ctx.lineTo(hx+Math.sin(t)*80,hy-Math.cos(t)*80);ctx.stroke();
  for(const [i,s] of speakers().entries()){
    const p=scenePoint(s);ctx.fillStyle=i===selected?'#60a5fa':'#2e71b4';ctx.strokeStyle=i===selected?'#e4f4ff':'#9bc7f4';ctx.lineWidth=i===selected?5:2;
    ctx.beginPath();ctx.arc(p.x,p.y,i===selected?23:18,0,Math.PI*2);ctx.fill();ctx.stroke();
    ctx.fillStyle='#fff';ctx.font='bold 13px Segoe UI';ctx.textAlign='center';ctx.fillText(s.channel,p.x,p.y+5);
  }
  ctx.font='16px Segoe UI';ctx.fillStyle='#a9bfdc';ctx.fillText('CABINA · PLANO X/Z',360,548);
}
function form(){
 const s=current();if(!s)return;
 $('name').value=s.name;$('channel').value=s.channel;$('type').value=s.type;
 $('gain').value=s.gainDb;$('gain-label').textContent=Number(s.gainDb).toFixed(1)+' dB';
 ['x','y','z'].forEach((id,i)=>$(id).value=s.position[i].toFixed(2));
 $('profile-title').textContent=profile.name||'Preset sin nombre';
 $('vehicle-name').textContent=(profile.vehicleName||profile.vehicle||'')+' · '+speakers().length+' altavoces';
 $('speaker-list').replaceChildren();
 speakers().forEach((speaker,i)=>{
  let b=document.createElement('button');b.className='chip '+(selected===i?'active':'');
  b.textContent=speaker.channel+' · '+speaker.name;b.onclick=()=>{selected=i;update();};$('speaker-list').appendChild(b);
 });
 $('validation').textContent='✓ Perfil válido · Editor 2D de posiciones y ganancias';
}
function update(){form();draw();}
function pointFromEvent(e){const r=canvas.getBoundingClientRect();return{x:(e.clientX-r.left)*canvas.width/r.width,y:(e.clientY-r.top)*canvas.height/r.height};}
canvas.addEventListener('pointerdown',e=>{
 const p=pointFromEvent(e);let winner=-1,dist=999;
 speakers().forEach((s,i)=>{const t=scenePoint(s),d=Math.hypot(t.x-p.x,t.y-p.y);if(d<dist){winner=i;dist=d;}});
 if(dist<31){selected=winner;dragging=true;canvas.setPointerCapture(e.pointerId);update();}
});
canvas.addEventListener('pointerup',()=>dragging=false);
canvas.addEventListener('pointercancel',()=>dragging=false);
canvas.addEventListener('pointermove',e=>{
 if(!dragging)return;const p=pointFromEvent(e),s=current();
 s.position[0]=Math.round(clamp((p.x-canvas.width/2)/210,-5,5)*100)/100;
 s.position[2]=Math.round(clamp((p.y-canvas.height/2)/220,-5,5)*100)/100;
 form();draw();
});
$('angle').oninput=e=>{yaw=Number(e.target.value);$('angle-value').textContent=yaw+'°';draw();};
$('name').oninput=e=>{current().name=e.target.value;$('speaker-list').children[selected].textContent=current().channel+' · '+current().name;};
$('channel').onchange=e=>{current().channel=e.target.value;update();};
$('type').onchange=e=>{current().type=e.target.value;update();};
$('gain').oninput=e=>{current().gainDb=Number(e.target.value);$('gain-label').textContent=current().gainDb.toFixed(1)+' dB';};
['x','y','z'].forEach((id,i)=>$(id).onchange=e=>{let v=Number(e.target.value);if(!Number.isFinite(v))v=0;current().position[i]=clamp(v,-5,5);update();});
$('import').onchange=async e=>{
 const file=e.target.files?.[0];if(!file)return;
 try{if(file.size>2e6)throw Error('Archivo mayor de 2 MB');const obj=JSON.parse(await file.text());validate(obj);
  profile=obj;original=structuredClone(obj);selected=0;update();$('status').textContent='Importado: '+file.name;
 }catch(err){$('status').textContent='Error: '+err.message;}
 e.target.value='';
};
$('zero-eq').onclick=()=>{speakers().forEach(s=>{s.gainDb=s.type==='sub'?-60:0;});update();$('status').textContent='Ganancias a 0 dB; combinar varios altavoces puede elevar el volumen total.';};
$('restore').onclick=()=>{profile=structuredClone(original);selected=0;update();$('status').textContent='Restaurado perfil importado.';};
$('export').onclick=()=>{
 try{validate(profile);const blob=new Blob([JSON.stringify(profile,null,2)+'\n'],{type:'application/json'});
  const url=URL.createObjectURL(blob),a=document.createElement('a');a.href=url;
  a.download='TruckAudioSim_'+(profile.vehicleName||'preset').replace(/[^\w-]/g,'_')+'.json';a.click();
  setTimeout(()=>URL.revokeObjectURL(url),3000);$('status').textContent='Preset exportado. Se conservan los campos originales.';
 }catch(e){$('status').textContent='No se puede exportar: '+e.message;}
};
update();
})();
