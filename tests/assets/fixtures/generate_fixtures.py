import argparse,copy,hashlib,io,json,pathlib,struct,zipfile,zlib
parser=argparse.ArgumentParser()
parser.add_argument('output',nargs='?',default='build/c1-corpus')
parser.add_argument('reference_manifest',nargs='?')
parser.add_argument('--reference-schema',action='store_true',help='Run separately installed Draft2020-12 reference checks (required in CI)')
args=parser.parse_args()
root=pathlib.Path(args.output);root.mkdir(parents=True,exist_ok=True)
def js(x):return json.dumps(x,ensure_ascii=False,separators=(',',':'),allow_nan=False).encode()
def chunk(t,data):return struct.pack('>I',len(data))+t+data+struct.pack('>I',zlib.crc32(t+data))
def png(w=1,h=1):
 raw=b'\0\xff\0\0\xff'
 stream=b'\x78\x01\x01'+struct.pack('<HH',len(raw),65535-len(raw))+raw+struct.pack('>I',zlib.adler32(raw))
 return b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',w,h,8,6,0,0,0))+chunk(b'IDAT',stream)+chunk(b'IEND',b'')
def archive(items):
 data=io.BytesIO()
 with zipfile.ZipFile(data,'w') as z:
  for name,payload in items:
   info=zipfile.ZipInfo(name,(1980,1,1,0,0,0));info.filename=name;info.create_system=3;info.external_attr=0o100644<<16;info.compress_type=zipfile.ZIP_STORED
   z.writestr(info,payload)
 return data.getvalue()
m=json.loads(pathlib.Path('examples/character_dual_mode.json').read_text());m['representations'].pop('3d');m['license']='CC0-1.0 generated test content';m['rig_semantics']=m['rig_semantics'][1:]
r=json.loads(pathlib.Path('examples/character2d.ars2d.json').read_text());r['canvas']={'width':1,'height':1}
for part in r['parts']:
 part['atlas_rect_px']=[0,0,1,1];part['vertices_px']=[[0,0],[1,0],[0,1]];part['uv']=[[0,0],[1,0],[0,1]];part['triangles']=[[0,1,2]]
 for target in part.get('deform_targets',[]):target['vertex_deltas_px']=[[0,0],[0,0],[0,0]]
def bundle(manifest=m,rig=r,image=None,extra=()):
 return archive([('character.json',js(manifest)),('assets/character2d.ars2d.json',js(rig)),('assets/atlas.png',png() if image is None else image),*extra])
records=[]
def save(name,data,status,group,stage='C1'):
 (root/name).write_bytes(data);records.append({'id':name.removesuffix('.arschar'),'file':name,'sha256':hashlib.sha256(data).hexdigest(),'bytes':len(data),'expected':status,'family':group,'stage':stage})
save('positive-2d.arschar',bundle(),'OK','complete generated 2D')
for name in ['../escape','/absolute','C:/drive','//host/share','folder/../alias','CON','file:stream','folder\\escape']:
 save('path-'+str(len(records))+'.arschar',bundle(extra=[(name,b'x')]),'BAD_ASSET','archive names')
for names in [('alias','ALIAS'),('\u00e9','e\u0301'),('same','same')]:
 save('alias-'+str(len(records))+'.arschar',bundle(extra=[(n,b'x') for n in names]),'BAD_ASSET','canonical collision')
save('zip-truncated.arschar',bundle()[:-8],'BAD_ASSET','ZIP envelope')
save('zip-tail.arschar',bundle()+b'trailing','BAD_ASSET','ZIP envelope')
bad=bytearray(bundle());bad[14]^=1;save('zip-crc-conflict.arschar',bytes(bad),'BAD_ASSET','ZIP local/central conflict')
save('duplicate-json.arschar',archive([('character.json',b'{"schema_version":"0.1.0","schema_version":"0.1.0"}')]),'BAD_ASSET','strict JSON')
save('json-invalid-utf8.arschar',archive([('character.json',b'{"name":"\xc0\xaf"}')]),'BAD_ASSET','strict JSON')
save('json-exponent-overflow.arschar',archive([('character.json',b'{"value":1e400}')]),'BAD_ASSET','strict JSON')
save('json-depth.arschar',archive([('character.json',b'['*65+b'0'+b']'*65)]),'OUT_OF_BUDGET','JSON depth')
for label,change in [
 ('duplicate-semantic',lambda a:a['rig_semantics'].append(copy.deepcopy(a['rig_semantics'][0]))),
 ('bool-numeric',lambda a:a['rig_semantics'][0].update(default=True)),
 ('bad-default',lambda a:a['rig_semantics'][0].update(default=2)),
 ('missing-layer-binding',lambda a:a['representations']['2d']['layer_bindings'].pop()),
 ('bad-mapping',lambda a:a['representations']['2d']['mappings'][0].update(target='missing')),
 ('dangling-mask',lambda a:a['visual_layers'][0].update(mask='missing')),
 ('self-mask',lambda a:a['visual_layers'][0].update(mask=a['visual_layers'][0]['id'])),
 ('valid-disabled-mask',lambda a:a['visual_layers'][0].update(mask=a['visual_layers'][1]['id']))]:
 a=copy.deepcopy(m);change(a);save(label+'.arschar',bundle(manifest=a),'UNSUPPORTED' if label=='valid-disabled-mask' else 'BAD_ASSET','manifest semantics')
for label,change in [
 ('triangle-oob',lambda a:a['parts'][0].update(triangles=[[0,1,3]])),
 ('uv-length',lambda a:a['parts'][0]['uv'].pop()),
 ('atlas-oob',lambda a:a['parts'][0].update(atlas_rect_px=[0,0,2,1])),
 ('delta-length',lambda a:a['parts'][1]['deform_targets'][0]['vertex_deltas_px'].pop())]:
 a=copy.deepcopy(r);change(a);save(label+'.arschar',bundle(rig=a),'BAD_ASSET','2D geometry')
save('png-truncated.arschar',bundle(image=png()[:-5]),'BAD_ASSET','PNG decode')
bad=bytearray(png());bad[-1]^=1;save('png-crc.arschar',bundle(image=bytes(bad)),'BAD_ASSET','PNG CRC')
save('png-dimension.arschar',bundle(image=png(8193,1)),'OUT_OF_BUDGET','PNG preflight')
g={'asset':{'version':'2.0'},'buffers':[{'byteLength':44}],'bufferViews':[{'buffer':0,'byteOffset':0,'byteLength':36},{'buffer':0,'byteOffset':36,'byteLength':6}],'accessors':[{'bufferView':0,'componentType':5126,'count':3,'type':'VEC3','min':[0,0,0],'max':[1,1,0]},{'bufferView':1,'componentType':5123,'count':3,'type':'SCALAR'}],'meshes':[{'primitives':[{'attributes':{'POSITION':0},'indices':1}]}],'nodes':[{'mesh':0}],'scenes':[{'nodes':[0]}],'scene':0}
def glb(desc=g,binary=None):
 j=js(desc);j+=b' '*((-len(j))%4);b=(struct.pack('<9f3H',0,0,0,1,0,0,0,1,0,0,1,2)+b'\0\0') if binary is None else binary+b'\0'*((-len(binary))%4)
 return struct.pack('<III',0x46546c67,2,12+8+len(j)+8+len(b))+struct.pack('<II',len(j),0x4e4f534a)+j+struct.pack('<II',len(b),0x004e4942)+b
save('positive-triangle.glb',glb(),'OK','embedded GLB','C2')
save('glb-truncated.glb',glb()[:-1],'BAD_ASSET','GLB bounds','C2')
for label,change in [('glb-accessor-overflow',lambda a:a['accessors'][0].update(byteOffset=4294967295)),('glb-external-uri',lambda a:a['buffers'][0].update(uri='https://example.invalid/blocked')),('glb-required-extension',lambda a:a.update(extensionsRequired=['ARS_disabled']))]:
 a=copy.deepcopy(g);change(a);save(label+'.glb',glb(a),'BAD_ASSET','GLB bounds/URI/extensions','C2')
dual=copy.deepcopy(m);dual['representations']['3d']={'format':'glb-gltf2','asset':'assets/triangle.glb','mappings':[]}
save('positive-dual.arschar',bundle(manifest=dual,extra=[('assets/triangle.glb',glb())]),'OK','complete generated dual CPU bundle','C2')
# Native C1 extensions: exercise actual compressed/described envelopes without
# depending on the generator host's zlib compressor version.
def rawzip(items,descriptor=False,symlink=False):
 local=bytearray();central=bytearray()
 for name,data in items:
  name=name.encode();crc=zlib.crc32(data);packed=b'\x01'+struct.pack('<HH',len(data),65535-len(data))+data
  flags=0x800|(8 if descriptor else 0);offset=len(local)
  local.extend(struct.pack('<IHHHHHIIIHH',0x04034b50,20,flags,8,0,33,0 if descriptor else crc,0 if descriptor else len(packed),0 if descriptor else len(data),len(name),0)+name+packed)
  if descriptor:local.extend(struct.pack('<IIII',0x08074b50,crc,len(packed),len(data)))
  central.extend(struct.pack('<IHHHHHHIIIHHHHHII',0x02014b50,0x314,20,flags,8,0,33,crc,len(packed),len(data),len(name),0,0,0,0,(0o120777 if symlink else 0o100644)<<16,offset)+name)
 return bytes(local+central+struct.pack('<IHHHHIIH',0x06054b50,0,0,len(items),len(items),len(central),len(local),0))
items=[('character.json',js(m)),('assets/character2d.ars2d.json',js(r)),('assets/atlas.png',png())]
save('positive-deflate.arschar',rawzip(items),'OK','real raw DEFLATE')
save('positive-descriptor.arschar',rawzip(items,True),'OK','real data descriptor')
save('zip-symlink.arschar',rawzip(items,symlink=True),'BAD_ASSET','symlink')
save('zip-nested.arschar',bundle(extra=[('nested.zip',b'PK')]),'BAD_ASSET','nested archive')
def envelope_change(label,change,expected='BAD_ASSET'):
 data=bytearray(bundle());end=len(data)-22;central=struct.unpack_from('<I',data,end+16)[0];change(data,end,central);save(label+'.arschar',bytes(data),expected,'ZIP structural/budget')
envelope_change('zip-encrypted',lambda d,e,c:struct.pack_into('<H',d,c+8,1))
envelope_change('zip-offset',lambda d,e,c:struct.pack_into('<I',d,c+42,1))
envelope_change('zip-disk',lambda d,e,c:struct.pack_into('<H',d,c+34,1))
envelope_change('zip-count-budget',lambda d,e,c:(struct.pack_into('<H',d,e+8,4097),struct.pack_into('<H',d,e+10,4097)),'OUT_OF_BUDGET')
envelope_change('zip-expanded-budget',lambda d,e,c:struct.pack_into('<I',d,c+24,0xffffffff),'OUT_OF_BUDGET')
envelope_change('zip-ratio-budget',lambda d,e,c:struct.pack_into('<I',d,c+24,100000000),'OUT_OF_BUDGET')
data=bytearray(rawzip(items,True));pos=data.index(b'PK\x07\x08');data[pos+4]^=1;save('zip-descriptor-crc.arschar',bytes(data),'BAD_ASSET','data descriptor')
for label,change in [
 ('mixed-mask-triangle',lambda a:a['visual_layers'][0].update(mask=a['visual_layers'][1]['id'])),
 ('parent-mask-cycle',lambda a:(a['visual_layers'][0].update(parent=a['visual_layers'][1]['id']),a['visual_layers'][1].update(mask=a['visual_layers'][0]['id']))),
 ('duplicate-binding',lambda a:a['representations']['2d']['layer_bindings'].append(copy.deepcopy(a['representations']['2d']['layer_bindings'][0]))),
 ('uppercase-semantic',lambda a:a['rig_semantics'][0].update(id='Eye.Blink.Left')),
 ('unicode-semantic',lambda a:a['rig_semantics'][0].update(id='eye.\u00e9')),
 ('duplicate-layer',lambda a:a['visual_layers'].append(copy.deepcopy(a['visual_layers'][0]))),
 ('absent-default-mode',lambda a:a.update(default_mode='3d'))]:
 a=copy.deepcopy(m);change(a);rig=copy.deepcopy(r)
 if label=='mixed-mask-triangle':rig['parts'][0]['triangles']=[[0,1,3]]
 save(label+'.arschar',bundle(manifest=a,rig=rig),'BAD_ASSET','manifest/geometry precedence')
for label,change in [
 ('duplicate-part',lambda a:a['parts'].append(copy.deepcopy(a['parts'][0]))),
 ('duplicate-parameter',lambda a:a['parameters'].append(copy.deepcopy(a['parameters'][0]))),
 ('bad-pivot',lambda a:a['parts'][0].update(pivot_normalized=[2,0])),
 ('bad-parameter-default',lambda a:a['parameters'][0].update(default=2)),
 ('parameter-float-overflow',lambda a:a['parameters'][0].update(max=1e300)),
 ('unknown-deform-parameter',lambda a:a['parts'][1]['deform_targets'][0].update(parameter='missing'))]:
 a=copy.deepcopy(r);change(a);save(label+'.arschar',bundle(rig=a),'BAD_ASSET','2D semantic bounds')
image=png();image=image[:33]+chunk(b'tEXt',b'x'*65537)+image[33:];save('png-chunk-budget.arschar',bundle(image=image),'OUT_OF_BUDGET','PNG chunk allocation cap')
image=png();image=image[:33]+chunk(b'tEXt',b'')*4097+image[33:];save('png-chunk-count.arschar',bundle(image=image),'OUT_OF_BUDGET','PNG work bound')
def typed_png(depth,color,row,interlace=0,palette=b'',alpha=b''):
 stream=b'\x78\x01\x01'+struct.pack('<HH',len(row),65535-len(row))+row+struct.pack('>I',zlib.adler32(row))
 return b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',1,1,depth,color,0,0,interlace))+(chunk(b'PLTE',palette) if palette else b'')+(chunk(b'tRNS',alpha) if alpha else b'')+chunk(b'IDAT',stream)+chunk(b'IEND',b'')
for label,image in [
 ('positive-png-16bit',typed_png(16,6,b'\0\xff\xff\0\0\0\0\xff\xff')),
 ('positive-png-interlaced',typed_png(8,6,b'\0\xff\0\0\xff',1)),
 ('positive-png-gray',typed_png(1,0,b'\0\x80')),
 ('positive-png-rgb',typed_png(8,2,b'\0\xff\0\0')),
 ('positive-png-palette',typed_png(1,3,b'\0\0',palette=b'\xff\0\0',alpha=b'\xff'))]:
 save(label+'.arschar',bundle(image=image),'OK','PNG actual pixel format decode')
save('png-16bit-truncated.arschar',bundle(image=typed_png(16,6,b'\0\xff')),'BAD_ASSET','PNG decoded row length')
save('png-bad-bit-depth.arschar',bundle(image=typed_png(7,6,b'\0\xff\0\0\xff')),'BAD_ASSET','PNG IHDR fields')
a=copy.deepcopy(r);a['canvas']={'width':1024,'height':1024};a['parts'][0]['atlas_rect_px']=[0,0,2,1];save('atlas-canvas-is-not-image.arschar',bundle(rig=a),'BAD_ASSET','actual PNG bounds vs canvas')
a=copy.deepcopy(r);a['canvas']={'width':1024,'height':1024};save('positive-canvas-independent.arschar',bundle(rig=a),'OK','canvas is coordinate space')
for label,name in [('superscript','COM\u00b2.txt'),('console','CONIN$'),('trailing-dot','safe.')]:
 save('path-'+label+'.arschar',bundle(extra=[(name,b'x')]),'BAD_ASSET','Windows device/alias policy')
save('alias-casefold.arschar',bundle(extra=[('strasse',b'x'),('stra\u00dfe',b'y')]),'BAD_ASSET','Unicode casefold collision')
for label,filename in [('unknown-method','safe'),('unknown-method-traversal','../escape')]:
 data=bytearray(archive([(filename,b'x')]));central=data.index(b'PK\x01\x02')
 struct.pack_into('<H',data,8,99);struct.pack_into('<H',data,central+10,99)
 save('zip-'+label+'.arschar',bytes(data),'BAD_ASSET','unknown compression is malformed')
# A redundant ZIP64 size field with ordinary bounded sizes is recognized but
# disabled; conflicting local metadata must still win as malformed envelope.
for malformed in (False,True):
 data=bytearray(archive([('safe',b'x')]));central=data.index(b'PK\x01\x02');end=len(data)-22
 extra=struct.pack('<HHQ',1,8,1)
 if malformed:data[14]^=1
 struct.pack_into('<H',data,central+30,len(extra));struct.pack_into('<I',data,end+12,end-central+len(extra))
 data[end:end]=extra
 save('zip-zip64-'+('conflict' if malformed else 'disabled')+'.arschar',bytes(data),'BAD_ASSET' if malformed else 'UNSUPPORTED','ZIP64 bounded envelope precedence')
# C2: retain the original C1 byte oracles and extend real embedded GLB coverage.
class GlbFixture:
 def __init__(self):self.doc=copy.deepcopy(g);self.binary=bytearray(struct.pack('<9f3H',0,0,0,1,0,0,0,1,0,0,1,2)+b'\0\0')
 def view(self,payload,**fields):
  self.binary.extend(b'\0'*((-len(self.binary))%4));offset=len(self.binary);self.binary.extend(payload)
  self.doc['bufferViews'].append({'buffer':0,'byteOffset':offset,'byteLength':len(payload),**fields});return len(self.doc['bufferViews'])-1
 def accessor(self,values,kind=5126,shape='VEC3',**fields):
  components={'SCALAR':1,'VEC2':2,'VEC3':3,'VEC4':4,'MAT4':16}[shape];flat=[v for row in values for v in row] if values and isinstance(values[0],(list,tuple)) else values
  payload=struct.pack('<'+{5121:'B',5123:'H',5125:'I',5126:'f'}[kind]*len(flat),*flat)
  view=self.view(payload);index=len(self.doc['accessors']);self.doc['accessors'].append({'bufferView':view,'componentType':kind,'count':len(flat)//components,'type':shape,**fields});return index
 def bytes(self):self.doc['buffers'][0]['byteLength']=len(self.binary);return glb(self.doc,bytes(self.binary))
def glb_case(label,change,status='BAD_ASSET',base=None):
 fixture=copy.deepcopy(base) if base is not None else GlbFixture();change(fixture);save(label+'.glb',fixture.bytes(),status,'GLB checked CPU subset','C2')
for label,change in [
 ('glb-view-overflow',lambda x:x.doc['bufferViews'][0].update(byteLength=4294967295)),
 ('glb-view-stride-small',lambda x:x.doc['bufferViews'][0].update(byteStride=4)),
 ('glb-view-stride-alignment',lambda x:x.doc['bufferViews'][0].update(byteStride=14)),
 ('glb-view-target-zero',lambda x:x.doc['bufferViews'][0].update(target=0)),
 ('glb-accessor-alignment',lambda x:x.doc['accessors'][0].update(byteOffset=2)),
 ('glb-accessor-component',lambda x:x.doc['accessors'][0].update(componentType=5130)),
 ('glb-accessor-float-normalized',lambda x:x.doc['accessors'][0].update(normalized=True)),
 ('glb-position-min-conflict',lambda x:x.doc['accessors'][0].update(min=[-1,0,0])),
 ('glb-index-oob',lambda x:struct.pack_into('<H',x.binary,40,3)),
 ('glb-index-restart',lambda x:struct.pack_into('<H',x.binary,40,65535)),
 ('glb-binary-nan',lambda x:struct.pack_into('<I',x.binary,0,0x7fc00000)),
 ('glb-node-cycle',lambda x:x.doc['nodes'][0].update(children=[0])),
 ('glb-node-dangling',lambda x:x.doc['nodes'][0].update(children=[3])),
 ('glb-node-mesh-sentinel',lambda x:x.doc['nodes'][0].update(mesh=4294967295)),
 ('glb-node-skin-sentinel',lambda x:x.doc['nodes'][0].update(skin=4294967295)),
 ('glb-scene-sentinel',lambda x:x.doc.update(scene=4294967295)),
 ('glb-material-sentinel',lambda x:x.doc['meshes'][0]['primitives'][0].update(material=4294967295)),
 ('glb-quaternion-zero',lambda x:x.doc['nodes'][0].update(rotation=[0,0,0,0])),
 ('glb-matrix-trs',lambda x:x.doc['nodes'][0].update(matrix=[1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1],translation=[0,0,0])),
 ('glb-triangle-count',lambda x:x.doc['accessors'][1].update(count=2)),
 ('glb-extension-not-used',lambda x:x.doc.update(extensionsUsed=['OTHER'],extensionsRequired=['ARS_disabled']))]:glb_case(label,change)
glb_case('glb-valid-required-extension',lambda x:x.doc.update(extensionsUsed=['ARS_disabled'],extensionsRequired=['ARS_disabled']),'UNSUPPORTED')
glb_case('positive-glb-extras',lambda x:x.doc.update(extras={'extensions':'opaque metadata','number':1e200}),'OK')
glb_case('glb-count-budget',lambda x:x.doc['accessors'][0].update(count=4294967295),'OUT_OF_BUDGET')
for label,offset,value in [('glb-bad-magic',0,0),('glb-bad-version',4,1),('glb-length-conflict',8,20),('glb-json-alignment',12,3)]:
 payload=bytearray(glb());struct.pack_into('<I',payload,offset,value);save(label+'.glb',bytes(payload),'BAD_ASSET','GLB framing','C2')
payload=bytearray(glb());payload.extend(struct.pack('<II4s',4,0x12345678,b'test'));struct.pack_into('<I',payload,8,len(payload));save('positive-glb-unknown-chunk.glb',bytes(payload),'OK','GLB unknown chunk skip','C2')
interleaved=GlbFixture();interleaved.binary=bytearray(struct.pack('<12f3H',0,0,0,123,1,0,0,123,0,1,0,123,0,1,2));interleaved.doc['bufferViews']=[{'buffer':0,'byteLength':48,'byteStride':16},{'buffer':0,'byteOffset':48,'byteLength':6}];save('positive-glb-interleaved.glb',interleaved.bytes(),'OK','GLB actual stride','C2')
sparse=GlbFixture();indices=sparse.view(bytes([1,2]));values=sparse.view(struct.pack('<6f',1,0,0,0,1,0));sparse.doc['accessors'][0].pop('bufferView');sparse.doc['accessors'][0]['sparse']={'count':2,'indices':{'bufferView':indices,'componentType':5121},'values':{'bufferView':values}};save('positive-glb-sparse.glb',sparse.bytes(),'OK','GLB actual sparse overlay','C2')
for label,payload in [('duplicate',bytes([1,1])),('descending',bytes([2,1])),('oob',bytes([1,3]))]:
 def change(x,payload=payload):
  offset=x.doc['bufferViews'][indices]['byteOffset'];x.binary[offset:offset+2]=payload
 glb_case('glb-sparse-'+label,change,base=sparse)
glb_case('glb-sparse-count',lambda x:x.doc['accessors'][0]['sparse'].update(count=4),base=sparse)
matrix=GlbFixture();matrix_view=matrix.view(struct.pack('<3Hxx3Hxx3H',1,2,3,4,5,6,7,8,9));matrix.doc['accessors'].append({'bufferView':matrix_view,'componentType':5123,'count':1,'type':'MAT3'});save('positive-glb-matrix-padding.glb',matrix.bytes(),'OK','GLB matrix column padding','C2')
full=GlbFixture();primitive=full.doc['meshes'][0]['primitives'][0]
primitive['attributes']['JOINTS_0']=full.accessor([[0,0,0,0]]*3,5121,'VEC4')
primitive['attributes']['WEIGHTS_0']=full.accessor([[1,0,0,0]]*3,shape='VEC4')
primitive['attributes']['TEXCOORD_0']=full.accessor([[0,0],[1,0],[0,1]],shape='VEC2')
bind=full.accessor([[1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1]],shape='MAT4')
morph=full.accessor([[0.1,0,0]]*3,min=[0.1,0,0],max=[0.1,0,0]);primitive['targets']=[{'POSITION':morph}];full.doc['meshes'][0].update(weights=[0],extras={'targetNames':['MouthOpen']})
full.doc['nodes']=[{'name':'Body','mesh':0,'skin':0,'children':[1]},{'name':'Head'}];full.doc['skins']=[{'joints':[1],'skeleton':0,'inverseBindMatrices':bind}]
times=full.accessor([0,1],shape='SCALAR',min=[0],max=[1]);translations=full.accessor([[0,0,0],[0,1,0]]);rotations=full.accessor([[0,0,0,1]]*2,shape='VEC4');weights=full.accessor([0,1],shape='SCALAR')
full.doc['animations']=[{'samplers':[{'input':times,'output':translations},{'input':times,'output':rotations,'interpolation':'STEP'},{'input':times,'output':weights}],'channels':[{'sampler':0,'target':{'node':1,'path':'translation'}},{'sampler':1,'target':{'node':1,'path':'rotation'}},{'sampler':2,'target':{'node':0,'path':'weights'}}]}]
pngview=full.view(png());full.doc.update(images=[{'bufferView':pngview,'mimeType':'image/png'}],textures=[{'source':0}],materials=[{'pbrMetallicRoughness':{'baseColorTexture':{'index':0}}}]);primitive['material']=0
save('positive-glb-full.glb',full.bytes(),'OK','skin morph animation embedded PNG','C2')
for label,change in [
 ('glb-joint-index',lambda x: x.binary.__setitem__(x.doc['bufferViews'][x.doc['accessors'][2]['bufferView']]['byteOffset'],1)),
 ('glb-weight-zero',lambda x:struct.pack_into('<f',x.binary,x.doc['bufferViews'][x.doc['accessors'][3]['bufferView']]['byteOffset'],0)),
 ('glb-skin-duplicate-joint',lambda x:x.doc['skins'][0].update(joints=[1,1])),
 ('glb-skin-inverse-sentinel',lambda x:x.doc['skins'][0].update(inverseBindMatrices=4294967295)),
 ('glb-skin-skeleton-sentinel',lambda x:x.doc['skins'][0].update(skeleton=4294967295)),
 ('glb-skin-missing-weights',lambda x:x.doc['meshes'][0]['primitives'][0]['attributes'].pop('WEIGHTS_0')),
 ('glb-morph-count',lambda x:x.doc['accessors'][morph].update(count=2)),
 ('glb-morph-name-count',lambda x:x.doc['meshes'][0]['extras'].update(targetNames=['One','Two'])),
 ('glb-animation-output-count',lambda x:x.doc['accessors'][translations].update(count=1)),
 ('glb-animation-node-sentinel',lambda x:x.doc['animations'][0]['channels'][0]['target'].update(node=4294967295)),
 ('glb-animation-duplicate-target',lambda x:x.doc['animations'][0]['channels'].append(copy.deepcopy(x.doc['animations'][0]['channels'][0]))),
 ('glb-animation-zero-quaternion',lambda x:struct.pack_into('<f',x.binary,x.doc['bufferViews'][x.doc['accessors'][rotations]['bufferView']]['byteOffset']+12,0)),
 ('glb-image-uri',lambda x:x.doc['images'][0].update(uri='data:image/png;base64,AA==')),
 ('glb-image-truncated',lambda x:x.doc['bufferViews'][pngview].update(byteLength=20)),
 ('glb-texture-index',lambda x:x.doc['materials'][0]['pbrMetallicRoughness']['baseColorTexture'].update(index=7)),
 ('glb-texture-uv',lambda x:x.doc['materials'][0]['pbrMetallicRoughness']['baseColorTexture'].update(texCoord=1))]:glb_case(label,change,base=full)
duplicate=copy.deepcopy(full);offset=duplicate.doc['bufferViews'][duplicate.doc['accessors'][times]['bufferView']]['byteOffset'];struct.pack_into('<f',duplicate.binary,offset+4,0);duplicate.doc['accessors'][times]['max']=[0];save('glb-animation-time-order.glb',duplicate.bytes(),'BAD_ASSET','actual time ordering','C2')
cubic=copy.deepcopy(full);cubic_output=cubic.accessor([[0,0,0],[0,0,0],[0,1,0],[0,1,0],[0,1,0],[0,0,0]]);cubic.doc['animations'][0]['samplers'][0].update(output=cubic_output,interpolation='CUBICSPLINE');save('positive-glb-cubic.glb',cubic.bytes(),'OK','real cubic tangent/value cardinality','C2')
glb_case('glb-cubic-count',lambda x:x.doc['animations'][0]['samplers'][0].update(output=translations,interpolation='CUBICSPLINE'),base=full)
disabled=copy.deepcopy(full);disabled.doc.update(extensionsUsed=['ARS_disabled'],extensionsRequired=['ARS_disabled']);save('glb-disabled-bad-index.glb',glb(disabled.doc,bytes(disabled.binary[:40]+struct.pack('<H',3)+disabled.binary[42:])),'BAD_ASSET','malformed before disabled dispatch','C2')
head={'id':'head.yaw_rad','type':'float','units':'radians','min':-1,'max':1,'default':0}
full_dual=copy.deepcopy(dual);full_dual['rig_semantics'].insert(0,head);full_dual['representations']['3d']['mappings']=[{'semantic':'head.yaw_rad','target':'bone:Head.yaw'},{'semantic':'mouth.open','target':'morph:MouthOpen'}]
three=copy.deepcopy(full_dual);three['representations'].pop('2d');three['default_mode']='3d';three['visual_layers']=[]
extra_manifests=[three,full_dual]
for label,manifest in [('positive-3d',three),('positive-dual-full',full_dual)]:save(label+'.arschar',bundle(manifest=manifest,extra=[('assets/triangle.glb',full.bytes())]),'OK','complete owned 3D/dual CPU model','C2')
for label,change,status in [('valid-disabled-vrm',lambda a:a['representations']['3d'].update(format='vrm1'),'UNSUPPORTED'),('bad-bone-reference',lambda a:a['representations']['3d']['mappings'][0].update(target='bone:Missing.yaw'),'BAD_ASSET'),('bad-morph-reference',lambda a:a['representations']['3d']['mappings'][1].update(target='morph:Missing'),'BAD_ASSET'),('bad-bone-unit',lambda a:a['rig_semantics'][0].update(units='meters'),'BAD_ASSET'),('semantic-range-overflow',lambda a:a['rig_semantics'][0].update(max=1e300),'BAD_ASSET')]:
 a=copy.deepcopy(full_dual);change(a);save(label+'.arschar',bundle(manifest=a,extra=[('assets/triangle.glb',full.bytes())]),status,'3D bindings and capability precedence','C2')
bad=bytearray(full.bytes());bad[0]=0
for label,manifest in [('vrm',copy.deepcopy(full_dual)),('mask',copy.deepcopy(full_dual))]:
 if label=='vrm':manifest['representations']['3d']['format']='vrm1'
 else:manifest['visual_layers'][0]['mask']=manifest['visual_layers'][1]['id']
 save('mixed-'+label+'-bad-glb.arschar',bundle(manifest=manifest,extra=[('assets/triangle.glb',bytes(bad))]),'BAD_ASSET','complete validation before disabled','C2')
glb_case('glb-morph-missing-bounds',lambda x:x.doc['accessors'][morph].pop('min'),base=full)
glb_case('glb-attribute-set-gap',lambda x:x.doc['meshes'][0]['primitives'][0]['attributes'].update(TEXCOORD_2=x.doc['meshes'][0]['primitives'][0]['attributes']['TEXCOORD_0']),base=full)
glb_case('glb-view-short-actual-data',lambda x:x.doc['bufferViews'][0].update(byteLength=32))
glb_case('glb-two-node-cycle',lambda x:(x.doc.update(nodes=[{'mesh':0,'children':[1]},{'children':[0]}]),x.doc.pop('scenes'),x.doc.pop('scene')))
for uri in ['file:///C:/blocked.bin','data:application/octet-stream;base64,AA==']:
 glb_case('glb-uri-'+('file' if uri.startswith('file') else 'data'),lambda x,uri=uri:x.doc['buffers'][0].update(uri=uri))
glb_case('glb-image-second',lambda x:x.doc['images'].append(copy.deepcopy(x.doc['images'][0])),'OK',full)
jpeg=b'\xff\xd8\xff\xc0'+struct.pack('>H',11)+bytes([8,0,1,0,1,1,1,0x11,0])+b'\xff\xda'+struct.pack('>H',8)+bytes([1,1,0,0,63,0])+b'\x01\xff\xd9'
def jpeg_image(x,payload):
 x.doc['images']=[{'bufferView':x.view(payload),'mimeType':'image/jpeg'}]
glb_case('glb-jpeg-disabled',lambda x:jpeg_image(x,jpeg),'UNSUPPORTED',full)
glb_case('glb-jpeg-truncated',lambda x:jpeg_image(x,jpeg[:-2]),base=full)
glb_case('glb-jpeg-marker-overflow',lambda x:jpeg_image(x,b'\xff\xd8\xff\xc0\xff\xff'),'BAD_ASSET',full)
integer_weights=copy.deepcopy(full);old=integer_weights.doc['meshes'][0]['primitives'][0]['attributes']['WEIGHTS_0'];integer_weights.doc['meshes'][0]['primitives'][0]['attributes']['WEIGHTS_0']=integer_weights.accessor([[255,0,0,0]]*3,5121,'VEC4',normalized=True)
save('positive-glb-integer-weights.glb',integer_weights.bytes(),'OK','actual normalized skin weights','C2')
mask=copy.deepcopy(full_dual);mask['visual_layers'][0]['mask']=mask['visual_layers'][1]['id'];save('dual-mask-disabled.arschar',bundle(manifest=mask,extra=[('assets/triangle.glb',full.bytes())]),'UNSUPPORTED','valid disabled dual masking','C2')
conflict=copy.deepcopy(full_dual);conflict['rig_semantics'][0]['default']=0.5;conflict['representations']['3d']['mappings'][0]['target']='morph:MouthOpen'
save('dual-mapping-type-conflict.arschar',bundle(manifest=conflict,extra=[('assets/triangle.glb',full.bytes())]),'BAD_ASSET','binding type and units','C2')
vrm=copy.deepcopy(full_dual);vrm['representations']['3d']['format']='vrm1';vrm['representations']['3d']['mappings'][0]['target']='bone:Missing.yaw'
save('vrm-bad-binding.arschar',bundle(manifest=vrm,extra=[('assets/triangle.glb',full.bytes())]),'BAD_ASSET','cross-file validation before disabled','C2')
for count in [256,257]:
 skin=copy.deepcopy(full);skin.doc['nodes']=[{'name':'Body','mesh':0,'skin':0,'children':list(range(1,count+1))},*[{'name':'J'+str(i)} for i in range(count)]];skin.doc['skins']=[{'joints':list(range(1,count+1)),'skeleton':0}]
 save('glb-skin-joints-'+str(count)+'.glb',skin.bytes(),'OK' if count==256 else 'OUT_OF_BUDGET','cumulative skin joint boundary','C2')
skin=copy.deepcopy(full);skin.doc['skins']=[{'joints':[1]} for _ in range(257)];save('glb-skin-cumulative-limit.glb',skin.bytes(),'OUT_OF_BUDGET','cumulative skin joint count across skins','C2')
normal=copy.deepcopy(full);attrs=normal.doc['meshes'][0]['primitives'][0]['attributes'];attrs['NORMAL']=normal.accessor([[0,0,1]]*3);attrs['TANGENT']=normal.accessor([[1,0,0,1]]*3,shape='VEC4');delta=normal.accessor([[0,0,0.1]]*3);normal.doc['meshes'][0]['primitives'][0]['targets'][0].update(NORMAL=delta,TANGENT=delta);save('positive-glb-normal-tangent-morph.glb',normal.bytes(),'OK','actual normal and tangent morph deltas','C2')
glb_case('glb-morph-nan',lambda x:struct.pack_into('<I',x.binary,x.doc['bufferViews'][x.doc['accessors'][delta]['bufferView']]['byteOffset'],0x7fc00000),base=normal)
quaternion_cubic=copy.deepcopy(full);output=quaternion_cubic.accessor([[0,0,0,0],[0,0,0,1],[0,0,0,0]]*2,shape='VEC4');quaternion_cubic.doc['animations'][0]['samplers'][1].update(output=output,interpolation='CUBICSPLINE');save('positive-glb-cubic-quaternion.glb',quaternion_cubic.bytes(),'OK','cubic quaternion values unit and tangents unrestricted','C2')
glb_case('glb-cubic-zero-quaternion',lambda x:struct.pack_into('<f',x.binary,x.doc['bufferViews'][x.doc['accessors'][output]['bufferView']]['byteOffset']+28,0),base=quaternion_cubic)
(root/'atlas.png').write_bytes(png());(root/'manifest.json').write_bytes((json.dumps(records,indent=2,ensure_ascii=False)+'\n').encode())
if args.reference_manifest:
 reference=json.loads(pathlib.Path(args.reference_manifest).read_text(encoding='utf-8'))
 # Compare the complete ordered oracle, including every binary hash and verdict.
 # Git may check text out as CRLF on Windows; binary fixtures stay byte exact.
 if js(reference)!=js(records):raise SystemExit('fixture manifest differs from committed oracle')
if args.reference_schema:
 from jsonschema import Draft202012Validator
 for schema_path,instance in [('schemas/character_manifest_v0.1.schema.json',m),('schemas/ars2d_rig_v0.1.schema.json',r),('schemas/character_manifest_v0.1.schema.json',dual),*[('schemas/character_manifest_v0.1.schema.json',a) for a in extra_manifests]]:
  schema=json.loads(pathlib.Path(schema_path).read_text(encoding='utf-8'));Draft202012Validator.check_schema(schema);Draft202012Validator(schema).validate(instance)
parity=[]
for kind,base,schema_path in [('manifest',m,'schemas/character_manifest_v0.1.schema.json'),('rig',r,'schemas/ars2d_rig_v0.1.schema.json')]:
 validator=Draft202012Validator(json.loads(pathlib.Path(schema_path).read_text(encoding='utf-8'))) if args.reference_schema else None
 variants=[('valid',copy.deepcopy(base)),('missing-required',{}),('unknown-property',{**base,'not_in_schema':True})]
 if kind=='manifest':
  for label,change in [('version',lambda a:a.update(schema_version='9.0.0')),('units',lambda a:a.update(units='feet')),('format',lambda a:a['representations']['2d'].update(format='unknown')),('ref-shape',lambda a:a['representations']['2d']['layer_bindings'][0].update(part_ids=[]))]:
   a=copy.deepcopy(base);change(a);variants.append((label,a))
 else:
  for label,change in [('bool-integer',lambda a:a['canvas'].update(width=True)),('zero-dimension',lambda a:a['canvas'].update(width=0)),('tuple-shape',lambda a:a['parts'][0].update(atlas_rect_px=[0,0,1])),('unknown-weight',lambda a:a['parts'][0].update(weights=[]))]:
   a=copy.deepcopy(base);change(a);variants.append((label,a))
 for label,instance in variants:
  expected=label=='valid'
  if validator is not None and validator.is_valid(instance)!=expected:raise SystemExit('reference schema oracle drift: '+kind+'-'+label)
  parity.append({'id':kind+'-'+label,'schema':kind,'input':js(instance).decode('utf-8'),'valid':expected})
parity_bytes=js(parity);(root/'schema-parity.json').write_bytes(parity_bytes)
print(json.dumps({'schema_reference_cases':len(parity),'schema_parity_sha256':hashlib.sha256(parity_bytes).hexdigest(),'reference_checks_executed':args.reference_schema}))
print(json.dumps({'fixtures':len(records),'positive_manifests_schema_validated':3+len(extra_manifests) if args.reference_schema else 0,'seed':'deterministic no RNG','native_parser_execution':False}))
