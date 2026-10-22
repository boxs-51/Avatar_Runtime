/* Layer Physics Studio PoC v0.1 - self-contained, no network or dependencies.
 * 1-DOF rotational spring preview only. NOT an Avatar_Runtime runtime solver.
 */
(function (root, factory) {
  const api = factory();
  if (typeof module === 'object' && module.exports) module.exports = api;
  if (root) root.LayerPhysicsCore = api;
})(typeof globalThis !== 'undefined' ? globalThis : this, function () {
  'use strict';
  const PI = Math.PI;
  const RAD = PI / 180;
  const MAX_LAYERS = 64;
  const MAX_DEPTH = 12;
  const FIXED_DT = 1 / 120;
  const LIMITS = Object.freeze({
    mass: [0.01, 10], lengthPx: [10, 350], stiffness: [0, 20], damping: [0, 5],
    gravityScale: [0, 3], windScale: [0, 3], restAngle: [-180, 180],
    limitMin: [-180, 180], limitMax: [-180, 180],
    anchorX: [-800, 800], anchorY: [-800, 800], imageScale: [0.02, 3],
    imageOffsetX: [-800, 800], imageOffsetY: [-800, 800]
  });
  const BASE = Object.freeze({mass:0.15, lengthPx:90, stiffness:0.8, damping:0.12,
    gravityScale:1, windScale:1, restAngle:0, limitMin:-70, limitMax:70,
    anchorX:0, anchorY:0, imageScale:1, imageOffsetX:-25, imageOffsetY:0});
  const WORLD = Object.freeze({gravity:9.81, windAcceleration:0, metersPerPixel:0.01});
  const PRESETS = Object.freeze({
    hair:{mass:0.11, stiffness:0.85, damping:0.09, gravityScale:1, windScale:1.7, lengthPx:92},
    cloth:{mass:0.26, stiffness:1.4, damping:0.18, gravityScale:1, windScale:1.2, lengthPx:90},
    accessory:{mass:0.35, stiffness:0.28, damping:0.11, gravityScale:1, windScale:0.5, lengthPx:66},
    rubber:{mass:0.12, stiffness:2.8, damping:0.18, gravityScale:0.6, windScale:1.0, lengthPx:72}
  });
  function layer(id, name, parentId, kind, restAngle, anchorX, anchorY, overrides={}) {
    return {id,name,parentId,kind,enabled:true, ...BASE, restAngle,anchorX,anchorY,
      imageData:null, ...overrides};
  }
  function createDefaultDocument(){
    return {format:'ars.layer-physics-authoring',version:'0.1.0',world:{...WORLD},layers:[
      layer('body','Thân',null,'static',0,0,0,{lengthPx:125}),
      layer('head','Đầu','body','static',0,0,-116,{lengthPx:80}),
      layer('hair-left','Tóc trái','head','pendulum',-19,-36,-36,{...PRESETS.hair,limitMin:-72,limitMax:60}),
      layer('hair-right','Tóc phải','head','pendulum',19,38,-36,{...PRESETS.hair,limitMin:-60,limitMax:72}),
      layer('scarf','Khăn áo','body','pendulum',28,38,-39,{...PRESETS.cloth,lengthPx:105,limitMin:-55,limitMax:95}),
      layer('pendant','Phụ kiện','body','pendulum',0,-4,-40,{...PRESETS.accessory,lengthPx:65,limitMin:-80,limitMax:80})
    ]};
  }
  function finiteInRange(value,range){return typeof value==='number' && Number.isFinite(value) && value>=range[0] && value<=range[1];}
  function validateDocument(doc){
    const errors=[];
    if(!doc||typeof doc!=='object'||Array.isArray(doc))return ['Document must be an object'];
    if(doc.format!=='ars.layer-physics-authoring'||doc.version!=='0.1.0')errors.push('Unknown format/version');
    if(!doc.world||!finiteInRange(doc.world.gravity,[0,30])||!finiteInRange(doc.world.windAcceleration,[-30,30])||
       !finiteInRange(doc.world.metersPerPixel,[0.001,0.1]))errors.push('Invalid world or unit settings');
    if(!Array.isArray(doc.layers)||doc.layers.length<1||doc.layers.length>MAX_LAYERS)return [...errors,'Invalid layer count'];
    let ids=new Set(),imageTotal=0;
    for(const l of doc.layers){
      if(!l||typeof l!=='object'){errors.push('Invalid layer item');continue;}
      if(typeof l.id!=='string'||!/^[a-zA-Z][a-zA-Z0-9_-]{0,63}$/.test(l.id)||ids.has(l.id))errors.push('Invalid or duplicate layer id');
      else ids.add(l.id);
      if(typeof l.name!=='string'||l.name.length<1||l.name.length>100)errors.push('Invalid name: '+l.id);
      if(!['static','pendulum'].includes(l.kind)||typeof l.enabled!=='boolean')errors.push('Invalid kind/enabled: '+l.id);
      for(const [k,r] of Object.entries(LIMITS))if(!finiteInRange(l[k],r))errors.push('Invalid '+k+': '+l.id);
      if(typeof l.limitMin==='number'&&typeof l.limitMax==='number'&&l.limitMin>l.limitMax)errors.push('Inverted limits: '+l.id);
      if(l.imageData!==null && l.imageData!==undefined){
        if(typeof l.imageData!=='string'||!/^data:image\/(png|jpeg|webp);base64,[A-Za-z0-9+/=]+$/.test(l.imageData)||l.imageData.length>8e6)errors.push('Invalid image: '+l.id);
        else imageTotal+=l.imageData.length;
      }
    }
    if(imageTotal>24e6)errors.push('Embedded image budget exceeded');
    let roots=0;
    for(const l of doc.layers){
      if(l.parentId===null){roots++;continue;}
      if(typeof l.parentId!=='string'||!ids.has(l.parentId))errors.push('Missing parent: '+l.id);
    }
    if(roots!==1)errors.push('Exactly one root required');
    const lookup=new Map(doc.layers.filter(x=>x&&typeof x.id==='string').map(x=>[x.id,x]));
    for(const l of doc.layers){
      if(!l||typeof l.id!=='string')continue;
      const seen=new Set();let curr=l;
      while(curr&&curr.parentId!==null){
        if(seen.has(curr.id)||seen.size>=MAX_DEPTH){errors.push('Cycle/depth exceeded: '+l.id);break;}
        seen.add(curr.id);curr=lookup.get(curr.parentId);
        if(!curr)break;
      }
    }
    return [...new Set(errors)];
  }
  function resetState(doc){
    const state={};for(const l of doc.layers)state[l.id]={theta:l.restAngle*RAD,omega:0};return state;
  }
  function stepLayer(l,pose,world,dt){
    if(l.kind!=='pendulum'||!l.enabled)return {theta:l.restAngle*RAD,omega:0};
    const theta=pose.theta,omega=pose.omega;
    const L=Math.max(0.01,l.lengthPx*world.metersPerPixel),mass=l.mass;
    const I= Math.max(1e-5,mass*L*L/3);
    // Angle is measured clockwise from vertical-down. Negative gravity torque restores vertical.
    const torque=-l.stiffness*(theta-l.restAngle*RAD)-l.damping*omega
       -mass*world.gravity*l.gravityScale*(L/2)*Math.sin(theta)
       +mass*world.windAcceleration*l.windScale*(L/2)*Math.cos(theta);
    let w=Math.max(-24,Math.min(24,omega+(torque/I)*dt));
    let a=theta+w*dt;
    const min=l.limitMin*RAD,max=l.limitMax*RAD;
    if(a<min){a=min;if(w<0)w=0;}
    if(a>max){a=max;if(w>0)w=0;}
    if(!Number.isFinite(a)||!Number.isFinite(w))return {theta:l.restAngle*RAD,omega:0};
    return {theta:a,omega:w};
  }
  function stepState(doc,state,dt=FIXED_DT){
    if(!Number.isFinite(dt)||dt<=0||dt>FIXED_DT*1.001)throw Error('Invalid dt');
    const next={};for(const l of doc.layers)next[l.id]=stepLayer(l,state[l.id]||{theta:l.restAngle*RAD,omega:0},doc.world,dt);
    return next;
  }
  function impulse(state,id,radiansPerSecond){
    if(!state[id]||!Number.isFinite(radiansPerSecond))return;
    state[id].omega=Math.max(-24,Math.min(24,state[id].omega+radiansPerSecond));
  }
  return Object.freeze({RAD,FIXED_DT,MAX_LAYERS,BASE,WORLD,PRESETS,LIMITS,createDefaultDocument,validateDocument,resetState,stepLayer,stepState,impulse});
});
