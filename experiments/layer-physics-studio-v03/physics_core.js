/* Layer Physics Studio v0.3 — deterministic, bounded, non-production 2D XPBD authoring core.
 * Classic-script UMD for file:// usage; Node CommonJS export for regression tests.
 * Units: positions/rest lengths meters, mass kg, accelerations m/s^2,
 * stretch/shear/bend stiffness N/m, compliance m/N (=1/stiffness).
 * dt is fixed at 1/120 s; solver lambdas reset per time step, not per iteration.
 * These mesh-distance bending links approximate bending: not a triangle/FEM fabric model.
 */
(function(root,factory){'use strict';const api=factory();if(typeof module==='object'&&module.exports)module.exports=api;if(root)root.LayerPhysicsCore=api;})(typeof globalThis!=='undefined'?globalThis:this,function(){
'use strict';
const FIXED_DT=1/120, RAD=Math.PI/180, MAX_LAYERS=64,MAX_DEPTH=12,MAX_PARTICLES=2048,MAX_CONSTRAINTS=18000,MAX_WORK_PER_STEP=120000;
const LIMITS=Object.freeze({colliderRadiusPx:[0,25],mass:[0.01,10],stretchStiffness:[1,100000],shearStiffness:[1,100000],bendStiffness:[1,100000],damping:[0,12],gravityScale:[0,3],windScale:[0,3],lengthPx:[10,350],widthPx:[6,350],segments:[2,22],columns:[2,18],rows:[2,18],restAngle:[-180,180],anchorX:[-800,800],anchorY:[-800,800],imageScale:[0.02,3],imageOffsetX:[-800,800],imageOffsetY:[-800,800]});
const BASE=Object.freeze({colliderRadiusPx:6,collisionEnabled:true,mass:0.18,stretchStiffness:12000,shearStiffness:6000,bendStiffness:900,damping:2.2,gravityScale:1,windScale:1,lengthPx:105,widthPx:90,segments:11,columns:9,rows:7,restAngle:0,anchorX:0,anchorY:0,imageScale:1,imageOffsetX:-22,imageOffsetY:0,pinMode:'corners',attachment:'origin'});
const WORLD=Object.freeze({gravity:9.81,windAcceleration:2,metersPerPixel:0.01,solverIterations:10,collisionEnabled:true});
const MAX_JOINTS=128,MAX_CONTACT_CHECKS=40000;
const PRESETS=Object.freeze({
 hair:{mass:0.12,lengthPx:108,segments:12,stretchStiffness:15000,bendStiffness:850,shearStiffness:6000,damping:2.3,gravityScale:1,windScale:1.6},
 cloth:{mass:0.48,lengthPx:130,widthPx:108,columns:9,rows:8,stretchStiffness:20000,shearStiffness:7000,bendStiffness:550,damping:2.8,gravityScale:1,windScale:1.2,pinMode:'corners'},
 ribbon:{mass:0.11,lengthPx:95,widthPx:43,columns:4,rows:10,stretchStiffness:9000,shearStiffness:2400,bendStiffness:260,damping:1.7,gravityScale:1,windScale:1.8,pinMode:'edge'},
 accessory:{mass:0.23,lengthPx:70,segments:5,stretchStiffness:45000,bendStiffness:6500,damping:3,gravityScale:1,windScale:0.5}
});
function layer(id,name,parentId,kind,anchorX,anchorY,overrides={}){return {id,name,parentId,kind,enabled:true,...BASE,anchorX,anchorY,imageData:null,...overrides};}
function createDefaultDocument(){return {format:'ars.layer-physics-authoring',version:'0.3.0',world:{...WORLD},joints:[],layers:[
 layer('body','Thân',null,'static',0,0),
 layer('head','Đầu','body','static',0,-118),
 layer('hair-left','Tóc trái','head','hair',-42,-34,{...PRESETS.hair,restAngle:-14}),
 layer('hair-right','Tóc phải','head','hair',43,-34,{...PRESETS.hair,restAngle:14}),
 layer('scarf','Khăn vải','body','cloth',27,-30,{...PRESETS.cloth}),
 layer('pendant','Phụ kiện','body','hair',-21,-56,{...PRESETS.accessory})
]};}
const finite=(n,min,max)=>typeof n==='number'&&Number.isFinite(n)&&n>=min&&n<=max;
const validId=s=>typeof s==='string'&&/^[A-Za-z][A-Za-z0-9_-]{0,63}$/.test(s);
function estimateBudget(doc){let particles=0,constraints=0;for(const l of doc.layers||[]){if(!l||!Number.isInteger(l.segments)||!Number.isInteger(l.rows)||!Number.isInteger(l.columns))continue;
 if(l.kind==='hair'){particles+=l.segments+1;constraints+=l.segments+Math.max(0,l.segments-1);}
 if(l.kind==='cloth'){const c=l.columns,r=l.rows;particles+=c*r;constraints+=(c-1)*r+c*(r-1)+2*(c-1)*(r-1)+Math.max(0,c-2)*r+c*Math.max(0,r-2);}}
 return {particles,constraints};}
function validateDocument(doc){const errors=[];
 if(!doc||typeof doc!=='object'||Array.isArray(doc))return ['Document must be an object'];
 if(doc.format!=='ars.layer-physics-authoring'||doc.version!=='0.3.0')errors.push('Unknown format/version');
 const w=doc.world;if(!w||typeof w.collisionEnabled!=='boolean'||!finite(w.gravity,0,30)||!finite(w.windAcceleration,-30,30)||!finite(w.metersPerPixel,0.001,0.1)||!Number.isInteger(w.solverIterations)||w.solverIterations<2||w.solverIterations>24)errors.push('Invalid world parameters');
 if(!Array.isArray(doc.layers)||doc.layers.length<1||doc.layers.length>MAX_LAYERS)return [...errors,'Invalid layer count'];
 const ids=new Set();let imageSize=0,roots=0;
 for(const l of doc.layers){if(!l||typeof l!=='object'||Array.isArray(l)){errors.push('Invalid layer entry');continue;}
  if(!validId(l.id)||ids.has(l.id))errors.push('Invalid or duplicate layer id');else ids.add(l.id);
  if(typeof l.name!=='string'||l.name.length<1||l.name.length>100)errors.push('Invalid name: '+l.id);
  if(!['static','hair','cloth'].includes(l.kind)||typeof l.enabled!=='boolean')errors.push('Invalid kind/enabled: '+l.id);
  if(typeof l.collisionEnabled!=='boolean')errors.push('Invalid collisionEnabled: '+l.id);
  if(!['origin','tip'].includes(l.attachment))errors.push('Invalid attachment: '+l.id);
  if(!['corners','edge','center'].includes(l.pinMode))errors.push('Invalid pinMode: '+l.id);
  for(const [k,r]of Object.entries(LIMITS))if(!finite(l[k],r[0],r[1])||(['segments','columns','rows'].includes(k)&&!Number.isInteger(l[k])))errors.push('Invalid '+k+': '+l.id);
  if(l.parentId===null)roots++;
  const data=l.imageData;if(data!==null&&data!==undefined){if(typeof data!=='string'||data.length>8e6||!/^data:image\/(png|jpeg|webp);base64,[A-Za-z0-9+/=]+$/.test(data))errors.push('Invalid image: '+l.id);else imageSize+=data.length;}
 }
 if(imageSize>24e6)errors.push('Embedded image budget exceeded');
 if(roots!==1)errors.push('Exactly one root required');
 const map=new Map(doc.layers.filter(x=>x&&validId(x.id)).map(x=>[x.id,x]));
 for(const l of doc.layers){if(!l||!validId(l.id))continue;if(l.parentId!==null&&!map.has(l.parentId))errors.push('Missing parent: '+l.id);
  let curr=l,seen=new Set();while(curr&&curr.parentId!==null){if(seen.has(curr.id)||seen.size>=MAX_DEPTH){errors.push('Cycle/depth exceeded: '+l.id);break;}seen.add(curr.id);curr=map.get(curr.parentId);}
 }
 if(doc.layers.some(x=>x&&x.parentId===null&&x.kind!=='static'))errors.push('Root must be static');
 const b=estimateBudget(doc);if(b.particles>MAX_PARTICLES||b.constraints>MAX_CONSTRAINTS)errors.push('Particle/constraint budget exceeded ('+b.particles+'/'+b.constraints+')');
 if(!Array.isArray(doc.joints)||doc.joints.length>MAX_JOINTS)errors.push('Invalid joint count');
 else {const seenJ=new Set();const end=e=>{if(!e||typeof e!=='object'||!validId(e.layerId)||!Number.isInteger(e.index)||e.index<0)return false;const l=map.get(e.layerId);return !!l&&l.kind!=='static'&&e.index<(l.kind==='hair'?l.segments+1:l.columns*l.rows);};
  for(const j of doc.joints){if(!j||!validId(j.id)||seenJ.has(j.id)||!end(j.a)||!end(j.b)||j.a.layerId===j.b.layerId||!finite(j.restLengthPx,0,500)||!finite(j.stiffness,1,100000)||typeof j.enabled!=='boolean'){errors.push('Invalid cross-layer joint');continue;}seenJ.add(j.id);}}
 if(w&&Number.isInteger(w.solverIterations)&&(b.constraints+(Array.isArray(doc.joints)?doc.joints.length:0))*w.solverIterations>MAX_WORK_PER_STEP)errors.push('XPBD iteration work budget exceeded');
 return [...new Set(errors)];}
function topoOrder(doc){const map=new Map(doc.layers.map(x=>[x.id,x])),out=[],seen=new Set();function visit(l){if(seen.has(l.id))return;seen.add(l.id);if(l.parentId!==null)visit(map.get(l.parentId));out.push(l);}for(const l of doc.layers)visit(l);return out;}
function topologyKey(l){return [l.kind,l.segments,l.columns,l.rows,l.lengthPx,l.widthPx,l.restAngle,l.pinMode].join('|');}
const v=(x,y)=>({x,y});
const copyP=p=>v(p.x,p.y);
function rotate(x,y,a){return v(Math.cos(a)*x+Math.sin(a)*y,-Math.sin(a)*x+Math.cos(a)*y);}
function attachmentPoint(l,s){if(l.attachment==='tip'&&s.positions.length)return s.positions[s.positions.length-1];return s.anchor;}
function anchorOf(l,results,byId,mpp){if(l.parentId===null)return v(l.anchorX*mpp,l.anchorY*mpp);const ps=results[l.parentId],p=attachmentPoint(l,ps);return v(p.x+l.anchorX*mpp,p.y+l.anchorY*mpp);}
function coordinates(l,anchor,mpp){const points=[],ang=l.restAngle*RAD;
 if(l.kind==='hair'){for(let i=0;i<=l.segments;i++){const p=rotate(0,l.lengthPx*i/l.segments*mpp,ang);points.push(v(anchor.x+p.x,anchor.y+p.y));}}
 if(l.kind==='cloth'){for(let y=0;y<l.rows;y++)for(let x=0;x<l.columns;x++){const p=rotate((x/(l.columns-1)-0.5)*l.widthPx*mpp,(y/(l.rows-1))*l.lengthPx*mpp,ang);points.push(v(anchor.x+p.x,anchor.y+p.y));}}
 return points;}
function createConstraints(l,points){const arr=[];
 function add(a,b,type){arr.push({a,b,rest:Math.hypot(points[a].x-points[b].x,points[a].y-points[b].y),type});}
 if(l.kind==='hair')for(let i=0;i<l.segments;i++){add(i,i+1,'structural');if(i+2<=l.segments)add(i,i+2,'bend');}
 if(l.kind==='cloth'){const w=l.columns,h=l.rows,idx=(x,y)=>y*w+x;
  for(let y=0;y<h;y++)for(let x=0;x<w;x++){
   if(x+1<w)add(idx(x,y),idx(x+1,y),'structural');if(y+1<h)add(idx(x,y),idx(x,y+1),'structural');
   if(x+1<w&&y+1<h){add(idx(x,y),idx(x+1,y+1),'shear');add(idx(x+1,y),idx(x,y+1),'shear');}
   if(x+2<w)add(idx(x,y),idx(x+2,y),'bend');if(y+2<h)add(idx(x,y),idx(x,y+2),'bend');
  }
 }
 return arr;}
function pinIndices(l){if(l.kind==='hair')return [0];if(l.kind!=='cloth')return [];
 if(l.pinMode==='edge')return Array.from({length:l.columns},(_,i)=>i);
 if(l.pinMode==='center')return [Math.floor((l.columns-1)/2)];
 return [0,l.columns-1];}
function resetState(doc){const issues=validateDocument(doc);if(issues.length)throw Error('Invalid document: '+issues.slice(0,4).join('; '));const results={},mpp=doc.world.metersPerPixel;
 for(const l of topoOrder(doc)){const anchor=anchorOf(l,results,null,mpp),positions=coordinates(l,anchor,mpp),previous=positions.map(copyP),constraints=createConstraints(l,positions),pins=pinIndices(l);
 results[l.id]={anchor,positions,previous,constraints,pins,topology:topologyKey(l)};
 }return results;}
function stiffness(l,type){return type==='bend'?l.bendStiffness:type==='shear'?l.shearStiffness:l.stretchStiffness;}
function solveConstraint(pa,pb,invA,invB,c,alpha,lambda){const dx=pb.x-pa.x,dy=pb.y-pa.y,d=Math.hypot(dx,dy);if(d<1e-12)return lambda;const C=d-c.rest,dl=-(C+alpha*lambda)/(invA+invB+alpha),ux=dx/d,uy=dy/d;
 if(invA){pa.x-=invA*dl*ux;pa.y-=invA*dl*uy;}if(invB){pb.x+=invB*dl*ux;pb.y+=invB*dl*uy;}return lambda+dl;}
function stepState(doc,accepted,dt=FIXED_DT,options={}){
 if(!Number.isFinite(dt)||dt<=0||Math.abs(dt-FIXED_DT)>1e-8)throw Error('Invalid dt: fixed 120 Hz required');
 const errors=validateDocument(doc);if(errors.length)throw Error('Invalid document: '+errors.slice(0,3).join('; '));
 const order=topoOrder(doc),world=doc.world,mpp=world.metersPerPixel,result={},drag=options&&options.drag;
 if(drag&&(!doc.layers.some(l=>l.id===drag.layerId)||!Number.isInteger(drag.index)||drag.index<0||!drag.target||!finite(drag.target.x,-100,100)||!finite(drag.target.y,-100,100)))throw Error('Invalid drag target');
 const layers=new Map(order.map(l=>[l.id,l]));
 for(const l of order){const old=accepted&&accepted[l.id];if(!old||old.topology!==topologyKey(l)||!Array.isArray(old.positions)||!Array.isArray(old.previous)||old.positions.length!==old.previous.length||(l.kind==='hair'&&old.positions.length!==l.segments+1)||(l.kind==='cloth'&&old.positions.length!==l.columns*l.rows)||(l.kind==='static'&&old.positions.length!==0)||!Array.isArray(old.constraints)||!Array.isArray(old.pins))throw Error('Stale physics state or topology mismatch: '+l.id);
  const parent=l.parentId===null?null:result[l.parentId];const anchor=l.parentId===null?v(l.anchorX*mpp,l.anchorY*mpp):v(attachmentPoint(l,parent).x+l.anchorX*mpp,attachmentPoint(l,parent).y+l.anchorY*mpp);
  const positions=old.positions.map(p=>{if(!p||!Number.isFinite(p.x)||!Number.isFinite(p.y))throw Error('Non-finite accepted position');return copyP(p);});
  const previous=old.previous.map(p=>{if(!p||!Number.isFinite(p.x)||!Number.isFinite(p.y))throw Error('Non-finite accepted velocity');return copyP(p);});
  const pins=old.pins.slice(),constraints=old.constraints,topology=old.topology,fixed=new Set(pins);
  if(drag&&drag.layerId===l.id){if(drag.index>=positions.length||fixed.has(drag.index))throw Error('Invalid drag index (pinned or OOB)');fixed.add(drag.index);}
  const entry={anchor,positions,previous,pins,constraints,topology,_fixed:fixed,_inverse:l.kind==='static'?0:positions.length/l.mass};result[l.id]=entry;
  if(l.kind==='static')continue;
  const damping=Math.exp(-l.damping*dt),gx=world.windAcceleration*l.windScale,gy=world.gravity*l.gravityScale;
  if(!l.enabled){const rest=coordinates(l,anchor,mpp);for(let i=0;i<positions.length;i++){positions[i]=copyP(rest[i]);previous[i]=copyP(rest[i]);}for(let i=0;i<positions.length;i++)fixed.add(i);continue;}
  for(let i=0;i<positions.length;i++){if(fixed.has(i))continue;const p=positions[i],prev=previous[i],before=copyP(p);p.x+=(p.x-prev.x)*damping+gx*dt*dt;p.y+=(p.y-prev.y)*damping+gy*dt*dt;previous[i]=before;}
 }
 function applyPins(){for(const l of order){const s=result[l.id];s.anchor=anchorOf(l,result,null,mpp);if(l.kind==='static')continue;const rest=coordinates(l,s.anchor,mpp);for(const i of s.pins){s.positions[i]=copyP(rest[i]);s.previous[i]=copyP(rest[i]);}if(drag&&drag.layerId===l.id){s.positions[drag.index]=copyP(drag.target);s.previous[drag.index]=copyP(drag.target);}}}
 function inverse(s,index){return s._fixed.has(index)?0:s._inverse;}
 applyPins();
 const lambdas={};for(const l of order)lambdas[l.id]=new Float64Array(result[l.id].constraints.length);
 const jointLambdas=new Float64Array(doc.joints.length);
 const jointPairs=new Set(doc.joints.filter(j=>j.enabled).map(j=>[`${j.a.layerId}:${j.a.index}|${j.b.layerId}:${j.b.index}`,`${j.b.layerId}:${j.b.index}|${j.a.layerId}:${j.a.index}`]).flat());
 let contacts=0;
 function collide(){if(!world.collisionEnabled)return;const maxR=Math.max(...order.map(l=>l.collisionEnabled?l.colliderRadiusPx*mpp:0));if(maxR<=0)return;
  const cell=2*maxR,grid=new Map();let checks=0;contacts=0;
  for(const l of order){if(l.kind==='static'||!l.enabled||!l.collisionEnabled||l.colliderRadiusPx<=0)continue;const s=result[l.id],radius=l.colliderRadiusPx*mpp;
   for(let i=0;i<s.positions.length;i++){const p=s.positions[i],cx=Math.floor(p.x/cell),cy=Math.floor(p.y/cell);
    for(let dy=-1;dy<=1;dy++)for(let dx=-1;dx<=1;dx++){const arr=grid.get((cx+dx)+','+(cy+dy));if(!arr)continue;
      for(const other of arr){if(other.id===l.id||jointPairs.has(`${l.id}:${i}|${other.id}:${other.index}`))continue;
       if(++checks>MAX_CONTACT_CHECKS)throw Error('Collision pair budget exceeded');const rs=radius+other.radius,deltaX=p.x-other.point.x,deltaY=p.y-other.point.y,d2=deltaX*deltaX+deltaY*deltaY;if(d2>=rs*rs)continue;
       const so=result[other.id],wa=inverse(s,i),wb=inverse(so,other.index),mass=wa+wb;if(mass===0)continue;
       const d=Math.sqrt(d2),nx=d<1e-12?1:deltaX/d,ny=d<1e-12?0:deltaY/d,depth=rs-d;
       p.x+=nx*depth*wa/mass;p.y+=ny*depth*wa/mass;other.point.x-=nx*depth*wb/mass;other.point.y-=ny*depth*wb/mass;contacts++;
      }
    }
    const key=cx+','+cy;if(!grid.has(key))grid.set(key,[]);grid.get(key).push({id:l.id,index:i,point:p,radius});
   }
  }
 }
 for(let iter=0;iter<world.solverIterations;iter++){
  for(const l of order){const s=result[l.id];for(let j=0;j<s.constraints.length;j++){const c=s.constraints[j];if(!Number.isInteger(c.a)||!Number.isInteger(c.b)||c.a<0||c.b<0||c.a>=s.positions.length||c.b>=s.positions.length||!finite(c.rest,0,10)||!['structural','shear','bend'].includes(c.type))throw Error('Invalid accepted constraint');const wa=inverse(s,c.a),wb=inverse(s,c.b);if(wa+wb===0)continue;
   const alpha=1/stiffness(l,c.type)/(dt*dt);lambdas[l.id][j]=solveConstraint(s.positions[c.a],s.positions[c.b],wa,wb,c,alpha,lambdas[l.id][j]);}}
  for(let j=0;j<doc.joints.length;j++){const c=doc.joints[j];if(!c.enabled)continue;const a=result[c.a.layerId],b=result[c.b.layerId],wa=inverse(a,c.a.index),wb=inverse(b,c.b.index);if(wa+wb===0)continue;const link={rest:c.restLengthPx*mpp},alpha=1/c.stiffness/(dt*dt);jointLambdas[j]=solveConstraint(a.positions[c.a.index],b.positions[c.b.index],wa,wb,link,alpha,jointLambdas[j]);}
  collide();applyPins();
 }
 for(const s of Object.values(result)){for(const p of s.positions)if(!Number.isFinite(p.x)||!Number.isFinite(p.y)||Math.abs(p.x)>100||Math.abs(p.y)>100)throw Error('Divergent solver candidate');delete s._fixed;delete s._inverse;}
 return result;
}
// Controlled immediate velocity injection for preview interaction only. Mutates supplied mutable preview state.
function impulse(state,id,velocity){if(!finite(velocity,-25,25)||!state||!state[id])return false;const s=state[id],fixed=new Set(s.pins);for(let i=0;i<s.positions.length;i++)if(!fixed.has(i)){const f=(i+1)/s.positions.length;s.previous[i].x-=velocity*f*FIXED_DT;}return true;}
function migrateV01(old){if(!old||old.version!=='0.1.0'||old.format!=='ars.layer-physics-authoring')throw Error('Not a v0.1 document');
 // Validation is self-contained so the browser does not depend on v0.1 source files.
 if(!old.world||!Array.isArray(old.layers)||old.layers.length<1||old.layers.length>MAX_LAYERS)throw Error('Invalid v0.1 document');
 const ids=new Set(),map=new Map();for(const l of old.layers){if(!l||!validId(l.id)||ids.has(l.id)||!['static','pendulum'].includes(l.kind)||!finite(l.mass,0.01,10)||!finite(l.lengthPx,10,350)||!finite(l.restAngle,-180,180)||!finite(l.anchorX,-800,800)||!finite(l.anchorY,-800,800))throw Error('Invalid v0.1 layer');ids.add(l.id);map.set(l.id,l);}
 if(old.layers.filter(x=>x.parentId===null).length!==1)throw Error('Invalid v0.1 root');
 for(const l of old.layers){let p=l,n=0,seen=new Set();while(p&&p.parentId!==null){if(!ids.has(p.parentId)||seen.has(p.id)||++n>MAX_DEPTH)throw Error('Invalid v0.1 hierarchy');seen.add(p.id);p=map.get(p.parentId);}}
 const migrated={format:'ars.layer-physics-authoring',version:'0.3.0',joints:[],world:{...WORLD,gravity:old.world.gravity,windAcceleration:old.world.windAcceleration,metersPerPixel:old.world.metersPerPixel},layers:old.layers.map(l=>{
  let kind=l.kind==='static'?'static':/(scarf|cloth|vải|váy|áo)/i.test(l.name+' '+l.id)?'cloth':'hair',preset=kind==='cloth'?PRESETS.cloth:PRESETS.hair;
  const obj=layer(l.id,l.name,l.parentId,kind,l.anchorX,l.anchorY,{...preset,enabled:l.enabled,mass:l.mass,lengthPx:l.lengthPx,restAngle:l.restAngle,gravityScale:l.gravityScale,windScale:l.windScale,damping:Math.min(12,Math.max(0,l.damping*9)),imageScale:l.imageScale,imageOffsetX:l.imageOffsetX,imageOffsetY:l.imageOffsetY,imageData:l.imageData||null});
  return obj;
 })};const errors=validateDocument(migrated);if(errors.length)throw Error('Invalid migrated document: '+errors.join('; '));return migrated;}
function migrateV02(old){if(!old||old.format!=='ars.layer-physics-authoring'||old.version!=='0.2.0'||!Array.isArray(old.layers)||!old.world)throw Error('Not a v0.2 document');
 const migrated=JSON.parse(JSON.stringify(old));migrated.version='0.3.0';migrated.joints=[];migrated.world.collisionEnabled=true;
 for(const l of migrated.layers){l.colliderRadiusPx=l.kind==='static'?0:6;l.collisionEnabled=l.kind!=='static';}
 const e=validateDocument(migrated);if(e.length)throw Error('Invalid migrated v0.2 document: '+e.join('; '));return migrated;}
return Object.freeze({MAX_JOINTS,MAX_CONTACT_CHECKS,RAD,FIXED_DT,MAX_LAYERS,MAX_PARTICLES,MAX_CONSTRAINTS,MAX_WORK_PER_STEP,BASE,WORLD,PRESETS,LIMITS,layer,createDefaultDocument,validateDocument,estimateBudget,topoOrder,resetState,stepState,impulse,migrateV01,migrateV02});
});
