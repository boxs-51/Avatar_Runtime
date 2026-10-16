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
def glb(desc=g):
 j=js(desc);j+=b' '*((-len(j))%4);b=struct.pack('<9f3H',0,0,0,1,0,0,0,1,0,0,1,2)+b'\0\0'
 return struct.pack('<III',0x46546c67,2,12+8+len(j)+8+len(b))+struct.pack('<II',len(j),0x4e4f534a)+j+struct.pack('<II',len(b),0x004e4942)+b
save('positive-triangle.glb',glb(),'OK','embedded GLB','C2')
save('glb-truncated.glb',glb()[:-1],'BAD_ASSET','GLB bounds','C2')
for label,change in [('glb-accessor-overflow',lambda a:a['accessors'][0].update(byteOffset=4294967295)),('glb-external-uri',lambda a:a['buffers'][0].update(uri='https://example.invalid/blocked')),('glb-required-extension',lambda a:a.update(extensionsRequired=['ARS_disabled']))]:
 a=copy.deepcopy(g);change(a);save(label+'.glb',glb(a),'UNSUPPORTED' if label=='glb-required-extension' else 'BAD_ASSET','GLB bounds/URI/extensions','C2')
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
(root/'atlas.png').write_bytes(png());(root/'manifest.json').write_bytes((json.dumps(records,indent=2,ensure_ascii=False)+'\n').encode())
if args.reference_manifest:
 reference=json.loads(pathlib.Path(args.reference_manifest).read_text(encoding='utf-8'))
 # Compare the complete ordered oracle, including every binary hash and verdict.
 # Git may check text out as CRLF on Windows; binary fixtures stay byte exact.
 if js(reference)!=js(records):raise SystemExit('fixture manifest differs from committed oracle')
if args.reference_schema:
 from jsonschema import Draft202012Validator
 for schema_path,instance in [('schemas/character_manifest_v0.1.schema.json',m),('schemas/ars2d_rig_v0.1.schema.json',r),('schemas/character_manifest_v0.1.schema.json',dual)]:
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
print(json.dumps({'fixtures':len(records),'positive_manifests_schema_validated':3 if args.reference_schema else 0,'seed':'deterministic no RNG','native_parser_execution':False}))
