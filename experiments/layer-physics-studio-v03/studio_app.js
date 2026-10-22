/* Local-only v0.3 editor. Classic script intentionally supports file:// double-click. */
(function(){'use strict';
const P=window.LayerPhysicsCore;
const $=id=>document.getElementById(id);
const canvas=$('stage'),ctx=canvas.getContext('2d'),WIDTH=canvas.width,HEIGHT=canvas.height,center={x:WIDTH*.50,y:HEIGHT*.64};
const fieldSpec=[
 ['mass','Khối lượng','kg',.01,3,.01],['stretchStiffness','Stretch stiffness','N/m',1,60000,100],
 ['shearStiffness','Shear stiffness','N/m',1,25000,100],['bendStiffness','Bending stiffness','N/m',1,16000,50],
 ['colliderRadiusPx','Bán kính va chạm','px',0,25,.5],['damping','Damping','1/s',0,12,.1],['gravityScale','Gravity scale','×',0,3,.05],['windScale','Wind scale','×',0,3,.05]
];
const geoSpec=[
 ['anchorX','Điểm neo X','px',-220,220,1],['anchorY','Điểm neo Y','px',-220,220,1],
 ['lengthPx','Chiều dài','px',10,350,1],['widthPx','Chiều rộng','px',6,300,1],
 ['segments','Số đoạn tóc','',2,22,1],['columns','Cột vải','',2,18,1],['rows','Hàng vải','',2,18,1],
 ['restAngle','Góc ban đầu','°',-160,160,1]
];
const topologicalResetProps=new Set(['kind','lengthPx','widthPx','segments','columns','rows','restAngle','pinMode']);
let doc=(window.STUDIO_BOOTSTRAP&&P.validateDocument(window.STUDIO_BOOTSTRAP).length===0)?window.STUDIO_BOOTSTRAP:P.createDefaultDocument(),state=P.resetState(doc),selected=(doc.layers.some(x=>x.id==='hair-left')?'hair-left':doc.layers.find(x=>x.parentId===null).id),playing=true,steps=0,accum=0,lastTime=0,drag=null,windPulseUntil=0,creatingJoint=false,jointStart=null;
const images=new Map(),pointsOnScreen=new Map();const colors={hair:'#a69bfc',cloth:'#efa778',static:'#789ac2'};
const st=(id)=>doc.layers.find(l=>l.id===id),curr=()=>st(selected)||doc.layers[0];
function inform(msg,bad=false){const el=$('feedback');el.textContent=msg;el.classList.toggle('error',bad);}
function labelValue(value,unit){return (Number.isInteger(value)?String(value):value.toFixed(value<1?2:1))+(unit?' '+unit:'');}
function getChildren(id){return doc.layers.filter(l=>l.parentId===id);}
function root(){return doc.layers.find(l=>l.parentId===null);}
function depth(l){let n=0;while(l&&l.parentId!==null&&n<=P.MAX_LAYERS){l=st(l.parentId);n++;}return n;}
function rebuildTree(){const tree=$('layerTree');tree.textContent='';function walk(l,lev){const b=document.createElement('button');b.type='button';b.className='tree-node'+(l.id===selected?' sel':'');b.style.paddingLeft=(7+Math.min(lev,10)*13)+'px';b.setAttribute('aria-pressed',l.id===selected?'true':'false');
  const dot=document.createElement('span');dot.className='dot';dot.style.background=colors[l.kind];b.append(dot);
  const title=document.createElement('span');title.className='label-text';title.textContent=l.name;b.append(title);
  const note=document.createElement('small');note.textContent=l.kind==='hair'?l.segments+' joints':l.kind==='cloth'?l.columns+'×'+l.rows:'static';b.append(note);b.onclick=()=>selectLayer(l.id);tree.append(b);
  for(const x of getChildren(l.id))walk(x,lev+1);
 }walk(root(),0);$('layerCount').textContent=doc.layers.length+' layers';}
function updateFields(){const l=curr();$('selectedName').textContent=l.id;$('layerName').value=l.name;$('kind').value=l.kind;$('attachment').value=l.attachment;$('enabled').checked=l.enabled;$('collisionLayer').checked=l.collisionEnabled;$('pinMode').value=l.pinMode;$('pinField').hidden=l.kind!=='cloth';
 for(const fields of [fieldSpec,geoSpec])for(const [key,,unit] of fields){const inp=$('field-'+key),out=$('value-'+key);if(!inp||!out)continue;inp.value=l[key];out.textContent=labelValue(l[key],unit);}
 const s=state[l.id];$('selectedParticles').textContent=s.positions.length;$('selectedStretch').textContent=s.constraints.filter(c=>c.type==='structural').length;
 $('selectedOther').textContent=s.constraints.filter(c=>c.type!=='structural').length;rebuildJoints();updateSelectedError();}
function updateSelectedError(){const s=state[selected];if(!s){$('selectedError').textContent='—';return;}let max=0;for(const c of s.constraints){if(c.rest<=0)continue;const a=s.positions[c.a],b=s.positions[c.b];if(!a||!b)continue;max=Math.max(max,Math.abs(Math.hypot(a.x-b.x,a.y-b.y)/c.rest-1));}$('selectedError').textContent=(100*max).toFixed(1)+'%';}
function drawInputs(){for(const [list,target]of [[fieldSpec,'dynamicsFields'],[geoSpec,'geometryFields']]){const el=$(target);el.replaceChildren();for(const [key,label,unit,min,max,step]of list){const wrap=document.createElement('label');wrap.className='field';wrap.dataset.key=key;const caption=document.createElement('span');caption.className='field-label';const t=document.createElement('span');t.textContent=label;const out=document.createElement('output');out.id='value-'+key;out.className='field-output';caption.append(t,out);const input=document.createElement('input');input.type='range';input.id='field-'+key;input.min=String(min);input.max=String(max);input.step=String(step);input.addEventListener('input',()=>changeField(key,Number(input.value)));wrap.append(caption,input);el.append(wrap);}}}
function worldUI(){$('collisionWorld').checked=doc.world.collisionEnabled;for(const key of ['gravity','windAcceleration','solverIterations']){const input=$(key);input.value=doc.world[key];$('out-'+key).textContent=labelValue(doc.world[key],key==='solverIterations'?'':'m/s²');}}
function refresh(){rebuildTree();updateFields();worldUI();loadImages();draw();}
function reset(){state=P.resetState(doc);steps=0;accum=0;drag=null;updateFields();draw();}
function selectLayer(id){if(!st(id))return;selected=id;rebuildTree();updateFields();draw();}
function changeField(key,value){const l=curr(),old=l[key];l[key]=value;const errors=P.validateDocument(doc);if(errors.length){l[key]=old;const inp=$('field-'+key);if(inp)inp.value=old;inform('Giới hạn cấu hình: '+errors.slice(0,2).join('; '),true);return;}
 if(topologicalResetProps.has(key))reset();$('value-'+key).textContent=labelValue(value,([...(fieldSpec),...(geoSpec)].find(x=>x[0]===key)||[])[2]||'');rebuildTree();draw();}
function changeFieldSelect(key,value){const l=curr(),old=l[key];if(l.parentId===null&&key==='kind'&&value!=='static'){inform('Root phải giữ static.',true);$('kind').value=old;return;}
 l[key]=value;const errors=P.validateDocument(doc);if(errors.length){l[key]=old;updateFields();inform('Không thay đổi được: '+errors.slice(0,2).join('; '),true);return;}if(topologicalResetProps.has(key)){doc.joints=doc.joints.filter(j=>{const lim=e=>{const q=st(e.layerId);return q&&q.kind!=='static'&&e.index<(q.kind==='hair'?q.segments+1:q.columns*q.rows);};return lim(j.a)&&lim(j.b);});reset();}updateFields();rebuildTree();draw();}
function uniqueId(prefix){let i=1,id;do{id=prefix+'-'+i++;}while(st(id));return id;}
function addChild(){const parent=curr();if(doc.layers.length>=P.MAX_LAYERS||depth(parent)>=11){inform('Đạt giới hạn layer/độ sâu.',true);return;}
 const id=uniqueId('layer'),newLayer=P.layer(id,'Layer con '+id,parent.id,'hair',0,25,{...P.PRESETS.hair});doc.layers.push(newLayer);
 if(P.validateDocument(doc).length){doc.layers.pop();inform('Đã vượt ngân sách particle/constraint.',true);return;}selected=id;reset();refresh();inform('Đã tạo layer con có chuỗi XPBD.');}
function duplicate(){const l=curr();if(l.parentId===null){inform('Không thể nhân bản root.',true);return;}if(doc.layers.length>=P.MAX_LAYERS){inform('Đạt giới hạn layer.',true);return;}
 const clone={...l,id:uniqueId('copy'),name:l.name+' (copy)',anchorX:l.anchorX+18};doc.layers.push(clone);if(P.validateDocument(doc).length){doc.layers.pop();inform('Vượt giới hạn tài nguyên.',true);return;}
 selected=clone.id;reset();refresh();inform('Đã nhân bản layer.');}
function remove(){const l=curr();if(l.parentId===null){inform('Không được xóa root.',true);return;}const ids=new Set([l.id]);let n=-1;while(n!==ids.size){n=ids.size;for(const item of doc.layers)if(ids.has(item.parentId))ids.add(item.id);}if(!window.confirm('Xóa '+ids.size+' layer thuộc nhánh '+l.name+'?'))return;
 doc.layers=doc.layers.filter(item=>!ids.has(item.id));doc.joints=doc.joints.filter(j=>!ids.has(j.a.layerId)&&!ids.has(j.b.layerId));selected=l.parentId;reset();refresh();inform('Đã xóa '+ids.size+' layer.');}
function updatePreset(name){const l=curr(),kind=name==='cloth'||name==='ribbon'?'cloth':'hair',before={...l};Object.assign(l,P.PRESETS[name]);l.kind=kind;
 const errors=P.validateDocument(doc);if(errors.length){Object.assign(l,before);updateFields();inform('Preset không hợp lệ: '+errors.slice(0,3).join('; '),true);return;}
 reset();refresh();inform('Đã áp preset '+name+'.');}
function rebuildImage(id){const l=st(id);if(!l||!l.imageData){images.delete(id);return;}const img=new Image();img.onload=()=>{images.set(id,img);draw();};img.onerror=()=>{images.delete(id);inform('Không thể đọc ảnh: '+id,true);};img.src=l.imageData;}
function loadImages(){for(const l of doc.layers)rebuildImage(l.id);for(const id of [...images.keys()])if(!st(id))images.delete(id);}
function download(name,content){const blob=new Blob([content],{type:'application/json;charset=utf-8'}),url=URL.createObjectURL(blob),a=document.createElement('a');a.href=url;a.download=name;document.body.append(a);a.click();setTimeout(()=>{URL.revokeObjectURL(url);a.remove();},1000);}
function uploadFile(input){input.addEventListener('change',async()=>{const file=input.files&&input.files[0];input.value='';if(!file)return;const lId=selected;
  if(file.size>5*1024*1024||!['image/png','image/jpeg','image/webp'].includes(file.type)){inform('PNG/JPEG/WebP tối đa 5 MB.',true);return;}
  try{const text=await new Promise((ok,fail)=>{const reader=new FileReader();reader.onload=()=>ok(reader.result);reader.onerror=fail;reader.readAsDataURL(file);});const im=new Image();await new Promise((ok,fail)=>{im.onload=ok;im.onerror=fail;im.src=text;});if(im.naturalWidth>2048||im.naturalHeight>2048){inform('Ảnh vượt 2048 × 2048.',true);return;}const l=st(lId);if(!l)return;
  const old=l.imageData;l.imageData=text;const errs=P.validateDocument(doc);if(errs.length){l.imageData=old;inform('Ảnh vượt giới hạn dữ liệu JSON: '+errs[0],true);return;}
  images.set(lId,im);draw();inform('Đã liên kết ảnh vào '+lId+' để biến dạng theo mesh.');}catch(e){inform('Ảnh lỗi/không giải mã được.',true);}
 });}
function px(pt){return {x:center.x+pt.x/doc.world.metersPerPixel,y:center.y+pt.y/doc.world.metersPerPixel};}
function fromCanvas(clientX,clientY){const r=canvas.getBoundingClientRect();return {x:(clientX-r.left)*WIDTH/r.width,y:(clientY-r.top)*HEIGHT/r.height};}
function toMeters(p){return {x:(p.x-center.x)*doc.world.metersPerPixel,y:(p.y-center.y)*doc.world.metersPerPixel};}
function segment(ctx,a,b){ctx.moveTo(a.x,a.y);ctx.lineTo(b.x,b.y);}
function drawGrid(){ctx.fillStyle='#101d2d';ctx.fillRect(0,0,WIDTH,HEIGHT);ctx.strokeStyle='#1c344a';ctx.lineWidth=1;for(let x=(center.x%40);x<WIDTH;x+=40){ctx.beginPath();ctx.moveTo(x,0);ctx.lineTo(x,HEIGHT);ctx.stroke();}for(let y=(center.y%40);y<HEIGHT;y+=40){ctx.beginPath();ctx.moveTo(0,y);ctx.lineTo(WIDTH,y);ctx.stroke();}ctx.strokeStyle='#375166';ctx.setLineDash([4,7]);ctx.beginPath();ctx.moveTo(center.x,0);ctx.lineTo(center.x,HEIGHT);ctx.stroke();ctx.setLineDash([]);ctx.fillStyle='#8099ae';ctx.font='12px system-ui';ctx.fillText('XPBD 2D · layer-local authoring',17,24);ctx.fillText('1 grid ≈ '+(40*doc.world.metersPerPixel).toFixed(2)+' m',17,43);}
function drawStatic(l,s){const p=px(s.anchor);ctx.save();ctx.translate(p.x,p.y);ctx.globalAlpha=l.enabled?1:.5;ctx.lineWidth=2;
 const img=images.get(l.id);if(img&&img.complete){const width=img.naturalWidth*l.imageScale,height=img.naturalHeight*l.imageScale;ctx.drawImage(img,l.imageOffsetX,l.imageOffsetY,width,height);}
 else if(l.id==='body'){ctx.fillStyle='#42688a';ctx.strokeStyle='#8dbeda';ctx.beginPath();ctx.moveTo(-48,-67);ctx.lineTo(48,-67);ctx.lineTo(60,118);ctx.lineTo(-57,118);ctx.closePath();ctx.fill();ctx.stroke();ctx.fillStyle='#344d78';ctx.fillRect(-47,26,96,7);}
 else if(l.id==='head'){ctx.fillStyle='#ebbcaa';ctx.strokeStyle='#cc9282';ctx.beginPath();ctx.ellipse(0,-10,54,57,0,0,Math.PI*2);ctx.fill();ctx.stroke();ctx.fillStyle='#3d4c65';for(const x of [-18,18]){ctx.beginPath();ctx.arc(x,-11,4,0,Math.PI*2);ctx.fill();}ctx.strokeStyle='#ac6b6d';ctx.beginPath();ctx.arc(0,14,8,.2,Math.PI-.2);ctx.stroke();}
 else{ctx.fillStyle='#638aaa';ctx.beginPath();ctx.arc(0,0,13,0,Math.PI*2);ctx.fill();}
 ctx.restore();}
function affineTexture(p0,p1,p2,uv0,uv1,uv2,img){const dx1=uv1.x-uv0.x,dy1=uv1.y-uv0.y,dx2=uv2.x-uv0.x,dy2=uv2.y-uv0.y,D=dx1*dy2-dx2*dy1;if(Math.abs(D)<1e-5)return;
 const sx1=p1.x-p0.x,sy1=p1.y-p0.y,sx2=p2.x-p0.x,sy2=p2.y-p0.y;
 const a=(sx1*dy2-sx2*dy1)/D,c=(dx1*sx2-dx2*sx1)/D,b=(sy1*dy2-sy2*dy1)/D,d=(dx1*sy2-dx2*sy1)/D;
 const e=p0.x-a*uv0.x-c*uv0.y,f=p0.y-b*uv0.x-d*uv0.y;
 ctx.save();ctx.beginPath();ctx.moveTo(p0.x,p0.y);ctx.lineTo(p1.x,p1.y);ctx.lineTo(p2.x,p2.y);ctx.closePath();ctx.clip();ctx.transform(a,b,c,d,e,f);ctx.drawImage(img,0,0);ctx.restore();}
function drawCloth(l,s){const w=l.columns,h=l.rows,points=s.positions.map(px),img=images.get(l.id);ctx.save();ctx.globalAlpha=l.enabled?1:.47;
 for(let y=0;y<h-1;y++)for(let x=0;x<w-1;x++){
 const i=y*w+x,a=points[i],b=points[i+1],c=points[i+w],d=points[i+w+1],index=x+y;
 if(img&&img.complete&&img.naturalWidth){const iw=img.naturalWidth,ih=img.naturalHeight,uv=(u,v)=>({x:u/(w-1)*iw,y:v/(h-1)*ih});affineTexture(a,b,c,uv(x,y),uv(x+1,y),uv(x,y+1),img);affineTexture(b,d,c,uv(x+1,y),uv(x+1,y+1),uv(x,y+1),img);}
 else{ctx.fillStyle=(index%2===0)?'#ce8768':'#edaf84';for(const tris of [[a,b,c],[b,d,c]]){ctx.beginPath();ctx.moveTo(tris[0].x,tris[0].y);ctx.lineTo(tris[1].x,tris[1].y);ctx.lineTo(tris[2].x,tris[2].y);ctx.closePath();ctx.fill();}}
 }ctx.restore();}
function hairGeometry(s,width){const pos=s.positions.map(px),left=[],right=[];for(let i=0;i<pos.length;i++){const prev=pos[Math.max(0,i-1)],next=pos[Math.min(pos.length-1,i+1)],dx=next.x-prev.x,dy=next.y-prev.y,len=Math.hypot(dx,dy)||1,amount=width*(1-.76*i/(pos.length-1))*.5;const nx=-dy/len*amount,ny=dx/len*amount;left.push({x:pos[i].x+nx,y:pos[i].y+ny});right.push({x:pos[i].x-nx,y:pos[i].y-ny});}return {left,right};}
function drawHair(l,s){const g=hairGeometry(s,Math.max(14,Math.min(45,l.widthPx*.38))),img=images.get(l.id),n=s.positions.length;ctx.save();ctx.globalAlpha=l.enabled?1:.46;
 for(let i=0;i<n-1;i++){const a=g.left[i],b=g.right[i],c=g.left[i+1],d=g.right[i+1];
 if(img&&img.complete&&img.naturalWidth){const iw=img.naturalWidth,ih=img.naturalHeight,uv=(u,k)=>({x:u*iw,y:k/(n-1)*ih});affineTexture(a,b,c,uv(0,i),uv(1,i),uv(0,i+1),img);affineTexture(b,d,c,uv(1,i),uv(1,i+1),uv(0,i+1),img);}
 else{ctx.fillStyle=l.id==='hair-right'?'#987acc':'#7665ba';ctx.beginPath();ctx.moveTo(a.x,a.y);ctx.lineTo(b.x,b.y);ctx.lineTo(d.x,d.y);ctx.lineTo(c.x,c.y);ctx.closePath();ctx.fill();ctx.strokeStyle='#afa5f2';ctx.lineWidth=.9;ctx.stroke();}}
 ctx.restore();}
function drawLinks(l,s){const isSelected=l.id===selected,showOther=$('showBends').checked;
 if(!$('showLinks').checked)return;ctx.save();ctx.lineWidth=isSelected?1.5:1;for(const c of s.constraints){if(c.type!=='structural'&&!showOther)continue;const a=px(s.positions[c.a]),b=px(s.positions[c.b]);if(c.type==='structural')ctx.strokeStyle=isSelected?'#e4e7edc0':'#c0dee45c';else ctx.strokeStyle=c.type==='shear'?'#70dca27d':'#d78dff72';ctx.beginPath();segment(ctx,a,b);ctx.stroke();}ctx.restore();}
function drawParticles(l,s){if(!$('showPoints').checked)return;const pins=new Set(s.pins),isSelected=l.id===selected;ctx.save();for(let i=0;i<s.positions.length;i++){const p=px(s.positions[i]);ctx.beginPath();ctx.arc(p.x,p.y,isSelected?3.8:2.6,0,2*Math.PI);ctx.fillStyle=pins.has(i)?'#f6dd8e':isSelected?'#f5f8ff':'#8dd7d2';ctx.fill();if(isSelected&&pins.has(i)){ctx.strokeStyle='#6f5831';ctx.stroke();}}
 ctx.restore();}
function draw(){if(!ctx)return;drawGrid();pointsOnScreen.clear();const order=P.topoOrder(doc);
 for(const l of order){const s=state[l.id];if(l.kind==='static')drawStatic(l,s);else if(l.kind==='cloth')drawCloth(l,s);else drawHair(l,s);
 for(let i=0;i<s.positions.length;i++)pointsOnScreen.set(l.id+':'+i,{...px(s.positions[i]),layerId:l.id,index:i,pinned:s.pins.includes(i)});
 drawLinks(l,s);drawParticles(l,s);
 if($('showLabels').checked){const p=px(s.anchor);ctx.fillStyle=l.id===selected?'#f4e09a':'#9fb3c6';ctx.font=l.id===selected?'bold 12px system-ui':'11px system-ui';ctx.fillText(l.name,p.x+9,p.y-10);}
 if(l.id===selected){const p=px(s.anchor);ctx.strokeStyle='#f6da91';ctx.lineWidth=2;ctx.beginPath();ctx.arc(p.x,p.y,8,0,Math.PI*2);ctx.stroke();}}
 if($('showJoints').checked){ctx.save();ctx.lineWidth=2.5;ctx.strokeStyle='#75f8b1';ctx.setLineDash([7,4]);for(const j of doc.joints){if(!j.enabled)continue;const a=state[j.a.layerId],b=state[j.b.layerId];if(!a||!b||!a.positions[j.a.index]||!b.positions[j.b.index])continue;const x=px(a.positions[j.a.index]),y=px(b.positions[j.b.index]);ctx.beginPath();segment(ctx,x,y);ctx.stroke();}ctx.restore();}
 $('frameStatus').textContent=(playing?'RUN':'PAUSE')+' · '+steps+' ticks · '+doc.joints.length+' cross-joints';}
function tick(){const opts=drag?{drag:{layerId:drag.layerId,index:drag.index,target:toMeters(drag.screen)}}:{};const next=P.stepState(doc,state,P.FIXED_DT,opts);state=next;steps++;}
function animate(time){if(!lastTime)lastTime=time;const elapsed=Math.max(0,Math.min(.1,(time-lastTime)/1000));lastTime=time;if(playing){accum+=elapsed;let count=0;while(accum>=P.FIXED_DT&&count<8){try{tick();}catch(err){playing=false;inform('Simulation stopped (candidate rejected): '+err.message,true);break;}accum-=P.FIXED_DT;count++;}if(count>=8&&accum>=P.FIXED_DT)accum=0;}
 if(drag&&!playing){try{tick();}catch(err){inform(err.message,true);drag=null;}}
 if(steps%15===0)updateSelectedError();draw();requestAnimationFrame(animate);}
function pointerDown(e){const pt=fromCanvas(e.clientX,e.clientY);let best=null,dist2=20*20;
 for(const p of pointsOnScreen.values()){if(p.pinned)continue;const d=(pt.x-p.x)**2+(pt.y-p.y)**2;if(d<dist2){dist2=d;best=p;}}
 if(best&&creatingJoint){if(!jointStart){jointStart=best;inform('Chọn hạt thứ hai thuộc layer khác để nối với '+best.layerId+':'+best.index);return;}const begin=jointStart;jointStart=null;creatingJoint=false;$('jointMode').textContent='＋ Nối 2 particle';if(begin.layerId===best.layerId){inform('Joint phải nối hai layer khác nhau.',true);return;}const a=state[begin.layerId],b=state[best.layerId],pa=a.positions[begin.index],pb=b.positions[best.index];if(!pa||!pb){inform('Particle đã thay đổi, hãy thử lại.',true);return;}const rest=Math.hypot(pa.x-pb.x,pa.y-pb.y)/doc.world.metersPerPixel;if(rest>500){inform('Khoảng cách lớn hơn 500px, không tạo joint.',true);return;}if(doc.joints.length>=P.MAX_JOINTS){inform('Joint budget reached.',true);return;}const id=uniqueJointId();doc.joints.push({id,a:{layerId:begin.layerId,index:begin.index},b:{layerId:best.layerId,index:best.index},restLengthPx:Math.max(0,rest),stiffness:18000,enabled:true});if(P.validateDocument(doc).length){doc.joints.pop();inform('Joint bị từ chối do giới hạn.',true);return;}rebuildJoints();draw();inform('Đã tạo joint hai chiều '+id+'.');return;}
 if(best){selected=best.layerId;selectLayer(selected);drag={layerId:best.layerId,index:best.index,screen:pt,pointerId:e.pointerId};canvas.setPointerCapture(e.pointerId);$('stageHelp').textContent='Đang kéo particle #'+best.index+' của '+best.layerId;e.preventDefault();return;}
 let nearest=null,mind=26*26;for(const l of doc.layers){const anchor=px(state[l.id].anchor),d=(anchor.x-pt.x)**2+(anchor.y-pt.y)**2;if(d<mind){mind=d;nearest=l.id;}}
 if(nearest)selectLayer(nearest);}
function pointerMove(e){if(!drag||drag.pointerId!==e.pointerId)return;const pt=fromCanvas(e.clientX,e.clientY);drag.screen={x:Math.max(0,Math.min(WIDTH,pt.x)),y:Math.max(0,Math.min(HEIGHT,pt.y))};}
function pointerEnd(e){if(drag&&drag.pointerId===e.pointerId){drag=null;$('stageHelp').textContent='Kéo trực tiếp các hạt ○ để kiểm thử biến dạng';}}
$('layerName').addEventListener('input',()=>{const l=curr();l.name=$('layerName').value.slice(0,100)||'Layer';rebuildTree();});
function uniqueJointId(){let i=1;while(doc.joints.some(j=>j.id==='joint-'+i))i++;return 'joint-'+i;}
function rebuildJoints(){const list=$('jointList');if(!list)return;list.replaceChildren();for(const j of doc.joints){const row=document.createElement('div');row.className='joint-row';const desc=document.createElement('span');desc.textContent=j.id+' · '+j.a.layerId+'['+j.a.index+'] ↔ '+j.b.layerId+'['+j.b.index+']';desc.title='Độ cứng '+j.stiffness+' N/m';const del=document.createElement('button');del.textContent='Xóa';del.type='button';del.onclick=()=>{doc.joints=doc.joints.filter(x=>x.id!==j.id);rebuildJoints();draw();inform('Đã xóa '+j.id);};row.append(desc,del);list.append(row);}}
$('jointMode').onclick=()=>{creatingJoint=!creatingJoint;jointStart=null;$('jointMode').textContent=creatingJoint?'Chọn 2 particle...':'＋ Nối 2 particle';inform(creatingJoint?'Click hai hạt thuộc hai layer khác nhau để tạo joint hai chiều.':'Đã hủy chế độ tạo joint.');};
$('collisionLayer').onchange=()=>{const l=curr();l.collisionEnabled=$('collisionLayer').checked;draw();};
$('collisionWorld').onchange=()=>{doc.world.collisionEnabled=$('collisionWorld').checked;draw();};
$('kind').onchange=()=>changeFieldSelect('kind',$('kind').value);$('attachment').onchange=()=>changeFieldSelect('attachment',$('attachment').value);$('pinMode').onchange=()=>changeFieldSelect('pinMode',$('pinMode').value);
$('enabled').onchange=()=>{curr().enabled=$('enabled').checked;draw();};
$('pause').onclick=()=>{playing=!playing;$('pause').textContent=playing?'Ⅱ Tạm dừng':'▶ Tiếp tục';if(!playing)accum=0;draw();};
$('step').onclick=()=>{playing=false;accum=0;$('pause').textContent='▶ Tiếp tục';try{tick();updateSelectedError();draw();}catch(err){inform(err.message,true);}};
$('reset').onclick=()=>{reset();inform('Đã reset trạng thái các particle, giữ nguyên thông số layer.');};
$('shake').onclick=()=>{const ids=new Set([selected]);let n=-1;while(ids.size!==n){n=ids.size;for(const l of doc.layers)if(ids.has(l.parentId))ids.add(l.id);}for(const id of ids)P.impulse(state,id,id===selected?8:4);inform('Đã thêm vận tốc ngang cho nhánh '+selected+'.');};
$('windPulse').onclick=()=>{const old=doc.world.windAcceleration;doc.world.windAcceleration=20;let applied=0;for(let i=0;i<6;i++)try{state=P.stepState(doc,state);applied++;}catch(e){inform(e.message,true);break;}doc.world.windAcceleration=old;steps+=applied;draw();inform('Đã mô phỏng 6 tick gió mạnh và trả lại lực gió cũ.');};
for(const key of ['gravity','windAcceleration','solverIterations'])$(key).addEventListener('input',()=>{const v=Number($(key).value),old=doc.world[key];doc.world[key]=v;const errs=P.validateDocument(doc);if(errs.length){doc.world[key]=old;$(key).value=old;inform(errs[0],true);return;}$('out-'+key).textContent=labelValue(v,key==='solverIterations'?'':'m/s²');});
for(const b of document.querySelectorAll('[data-preset]'))b.onclick=()=>updatePreset(b.dataset.preset);
$('addChild').onclick=addChild;$('duplicate').onclick=duplicate;$('remove').onclick=remove;
$('restoreSample').onclick=()=>{if(!window.confirm('Khôi phục toàn bộ scene mẫu (mất thay đổi chưa xuất JSON)?'))return;doc=P.createDefaultDocument();selected='hair-left';reset();refresh();inform('Đã khôi phục scene mặc định v0.2.');};
$('clearImage').onclick=()=>{curr().imageData=null;images.delete(selected);draw();inform('Đã xóa texture layer đang chọn.');};
uploadFile($('upload'));
$('export').onclick=()=>{const errors=P.validateDocument(doc);if(errors.length){inform('Không thể xuất: '+errors.slice(0,3).join(' / '),true);return;}download('layer_physics_authoring_v0.3.json',JSON.stringify(doc,null,2));inform('Đã xuất authoring document v0.3 (chưa phải định dạng runtime).');};
$('import').addEventListener('change',async e=>{const file=e.target.files&&e.target.files[0];e.target.value='';if(!file)return;if(file.size>25*1024*1024){inform('Tệp JSON vượt giới hạn 25 MB.',true);return;}
 try{let d=JSON.parse(await file.text()),migrated=false;if(d.version==='0.1.0'){d=P.migrateV01(d);migrated=true;}else if(d.version==='0.2.0'){d=P.migrateV02(d);migrated=true;}const errors=P.validateDocument(d);if(errors.length){inform('Import rejected: '+errors.slice(0,3).join('; '),true);return;}
 const candidate=P.resetState(d);doc=d;state=candidate;selected=d.layers.find(x=>x.parentId===null).id;steps=0;accum=0;drag=null;refresh();inform(migrated?'Đã chuyển cấu hình cũ lên v0.3.':'Đã nhập cấu hình v0.3 hợp lệ.');}catch(err){inform('Không thể nhập JSON: '+err.message,true);}});
canvas.addEventListener('pointerdown',pointerDown);canvas.addEventListener('pointermove',pointerMove);canvas.addEventListener('pointerup',pointerEnd);canvas.addEventListener('pointercancel',pointerEnd);canvas.addEventListener('lostpointercapture',()=>{drag=null;});
for(const id of ['showPoints','showLinks','showBends','showLabels','showJoints'])$(id).addEventListener('change',draw);
window.addEventListener('keydown',e=>{if(e.target.closest('input,select,button,textarea'))return;if(e.code==='Space'){e.preventDefault();$('pause').click();}else if(e.key.toLowerCase()==='r')$('reset').click();else if(e.key.toLowerCase()==='s')$('step').click();});
drawInputs();refresh();inform('XPBD v0.3 sẵn sàng: collision & cross-layer joints. Chọn Tóc hoặc Vải, chỉnh particle/joints, kéo một hạt trên canvas hoặc nhấn Rung layer.');requestAnimationFrame(animate);
})();
