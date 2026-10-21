"""Full native2D/GLB/dual typed transfer qualification; whole #60 remains open."""
import argparse
import hashlib
import io
import json
from pathlib import Path
import shutil
import struct
import subprocess
import uuid
import zipfile
import zlib
from verify_cpu_admission_probe import request, STATUS
from moc3_public_preflight import profile_absent

HEADER = 96
CAP = 16*1024*1024


def check(ok, name):
    if not ok:
        raise RuntimeError(name)


def fix_digest(data):
    data = bytearray(data)
    struct.pack_into('<I', data, 16, len(data))
    data[64:96] = hashlib.sha256(data[96:]).digest()
    return bytes(data)


def offsets(data):
    """Scan our known-good generated packet to target independent hostile mutations."""
    pos = 132
    nl, np, nt, ns = struct.unpack_from('<IIII', data, 112)
    def text():
        nonlocal pos
        start = pos
        size = struct.unpack_from('<I', data, pos)[0]
        pos += 4 + size
        return start
    layers, params, semantics, parts = [], [], [], []
    for _ in range(nl):
        at = text(); layers.append({'id': at, 'z': pos, 'visible': pos+4, 'opacity': pos+8}); pos += 12
    for _ in range(np):
        at = text(); params.append({'id': at, 'min': pos, 'max': pos+4, 'initial': pos+8}); pos += 12
    for _ in range(ns):
        at = {name: text() for name in ('id','type','units','target')}
        at.update(flags=pos, initial=pos+4, minimum=pos+36, maximum=pos+44)
        semantics.append(at); pos += 52
    for _ in range(nt):
        at = {'id': text(), 'rect': pos}
        # Dict value evaluation follows insertion order; pos is after text.
        at.update(pivot=pos+16,z=pos+24,layer=pos+28,nv=pos+32,triangles=pos+36,deforms=pos+40)
        nv,triangles,deforms = struct.unpack_from('<III',data,pos+32)
        pos += 44
        at.update(vertices=pos,uv=pos+nv*8,indices=pos+nv*16)
        pos += nv*16 + triangles*12
        at['deform_records'] = []
        for _ in range(deforms):
            at['deform_records'].append({'parameter':pos,'deltas':pos+4});pos += 4+nv*8
        parts.append(at)
    return layers, params, semantics, parts


def hostile(data):
    layers, params, semantics, parts = offsets(data)
    result = {}
    def number(name, pos, value, fmt='<I'):
        altered=bytearray(data);struct.pack_into(fmt,altered,pos,value);result[name]=fix_digest(altered)
    def text(name, pos, value):
        n=struct.unpack_from('<I',data,pos)[0]
        result[name]=fix_digest(data[:pos]+struct.pack('<I',len(value))+value+data[pos+4+n:])
    for name, pos, value in [('version',8,2),('profile',12,2),('status',20,999),('error_with_body',20,2),('layer_count',112,129),('parameter_count',116,513),('part_count',120,0),('semantic_count',124,513),('pixel_size',128,0xffffffff),('canvas_zero',96,0),('atlas_overflow',104,0xffffffff)]:
        number(name,pos,value)
    for name,pos in [('nonce',24),('request_digest',32),('payload_digest',64),('magic',0)]:
        altered=bytearray(data);altered[pos]^=1;result[name]=bytes(altered)
    result['trailing']=fix_digest(data+b'x')
    for label,at in [('layer',layers[0]),('part',parts[0])]:
        text(label+'_binding_syntax',at['id'],b'!')
        text(label+'_nul',at['id'],b'a\0b')
        text(label+'_utf8',at['id'],b'\xc0\xaf')
    text('duplicate_layer',layers[1]['id'],data[layers[0]['id']+4:layers[0]['id']+4+struct.unpack_from('<I',data,layers[0]['id'])[0]])
    text('duplicate_part',parts[1]['id'],data[parts[0]['id']+4:parts[0]['id']+4+struct.unpack_from('<I',data,parts[0]['id'])[0]])
    number('visibility',layers[0]['visible'],2)
    number('layer_z',layers[0]['z'],10001,'<i')
    for name,value in [('nan',float('nan')),('inf',float('inf')),('negative',-1.0),('high',2.0)]:
        number('opacity_'+name,layers[0]['opacity'],value,'<f')
    number('parameter_range',params[0]['min'],2.0,'<f')
    number('parameter_default',params[0]['initial'],2.0,'<f')
    text('parameter_long',params[0]['id'],b'x'*129)
    number('semantic_flags',semantics[0]['flags'],8)
    text('semantic_noncanonical',semantics[0]['id'],b'Upper')
    text('semantic_type',semantics[0]['type'],b'invalid')
    text('semantic_units',semantics[0]['units'],b'invalid')
    text('semantic_mapping',semantics[0]['target'],b'missing')
    number('semantic_projection_mismatch',semantics[0]['initial'],0.5,'<d')
    number('semantic_nan',semantics[0]['initial'],float('nan'),'<d')
    for name,pos,value,fmt in [
        ('part_layer',parts[0]['layer'],128,'<I'),('part_z',parts[0]['z'],10001,'<i'),
        ('vertex_count',parts[0]['nv'],0xffffffff,'<I'),('triangle_count',parts[0]['triangles'],0,'<I'),
        ('deform_count',parts[0]['deforms'],65,'<I'),('triangle_index',parts[0]['indices'],65535,'<I'),
        ('rect_overflow',parts[0]['rect'],0xffffffff,'<I'),('rect_zero',parts[0]['rect']+8,0,'<I'),
        ('pivot_high',parts[0]['pivot'],2.0,'<f'),('vertex_nan',parts[0]['vertices'],float('nan'),'<f'),
        ('uv_inf',parts[0]['uv'],float('inf'),'<f'),
        ('deform_parameter',parts[1]['deform_records'][0]['parameter'],512,'<I'),
        ('deform_inf',parts[1]['deform_records'][0]['deltas'],float('inf'),'<f')]:
        number(name,pos,value,fmt)
    return result


def extras(source):
    with zipfile.ZipFile(io.BytesIO(source)) as z:
        files={n:z.read(n) for n in z.namelist()}
    for variant in ('precision','unicode_parameter','rich_fields','empty_catalog','max_catalog','packet_budget'):
        m=json.loads(files['character.json']);rig=json.loads(files['assets/character2d.ars2d.json'])
        if variant=='precision':
            for p,s in zip(rig['parameters'],m['rig_semantics']):
                p.update(min=-0.3,max=0.7,default=0.1);s.update(min=-0.3,max=0.7,default=0.1)
        elif variant=='unicode_parameter':
            name='\U0001f600'*128
            rig['parameters'][1]['id']=name
            rig['parts'][1]['deform_targets'][0]['parameter']=name
            m['representations']['2d']['mappings'][1]['target']=name
        elif variant=='max_catalog':
            m['rig_semantics']=[{'id':'s'+str(i).zfill(3),'type':'trigger'} for i in range(512)]
            m['representations']['2d']['mappings']=[]
        elif variant=='empty_catalog':
            rig['parameters']=[];m['rig_semantics']=[];m['representations']['2d']['mappings']=[]
            rig['parts'][1].pop('deform_targets')
        elif variant=='rich_fields':
            m['visual_layers'][0]['opacity']=0.6
            m['visual_layers'][1].update(parent='base.body',opacity=0.5)
            rig['canvas']={'width':256,'height':128}
            rig['parts'][1]['deform_targets'][0]['vertex_deltas_px']=[[0.1,-0.2],[0.25,0.5],[-1,2]]
            rig['parts'][0]['pivot_normalized']=[0.2,0.9]
            m['rig_semantics'] += [{'id':'extra.bool','type':'bool','default':True},
                {'id':'extra.vec2','type':'vec2','default':[0.1,-0.2],'min':-1,'max':1},
                {'id':'extra.vec3','type':'vec3','default':[1,2,3]},
                {'id':'extra.quat','type':'quat','default':[0,0,0,1]},
                {'id':'extra.trigger','type':'trigger'}]
        case_files=dict(files)
        if variant=='packet_budget':
            def chunk(kind,data):
                return struct.pack('>I',len(data))+kind+data+struct.pack('>I',zlib.crc32(kind+data)&0xffffffff)
            raw=(b'\0'+b'\xff\0\0\xff'*2048)*2048
            case_files['assets/atlas.png']=b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',2048,2048,8,6,0,0,0))+chunk(b'IDAT',zlib.compress(raw))+chunk(b'IEND',b'')
        out=io.BytesIO()
        with zipfile.ZipFile(out,'w',compression=zipfile.ZIP_STORED) as z:
            for name,data in case_files.items():
                if name=='character.json':data=json.dumps(m,ensure_ascii=False).encode('utf-8')
                if name=='assets/character2d.ars2d.json':data=json.dumps(rig,ensure_ascii=False).encode('utf-8')
                z.writestr(name,data)
        yield variant,out.getvalue()


def glb_extras(source):
    with zipfile.ZipFile(io.BytesIO(source)) as z:files={n:z.read(n) for n in z.namelist()}
    for variant in ('glb_unbound_utf8_layer','glb_many_members','glb_camera','glb_empty_catalog'):
        changed=dict(files);manifest=json.loads(files['character.json'])
        if variant=='glb_unbound_utf8_layer':
            manifest['visual_layers']=[{'id':'!','mode_scope':'2d','z_index':0,'visible':True,'opacity':1}]
        elif variant=='glb_empty_catalog':
            manifest['rig_semantics']=[];manifest['representations']['3d']['mappings']=[]
        else:
            raw=files['assets/triangle.glb'];size=struct.unpack_from('<I',raw,12)[0];doc=json.loads(raw[20:20+size])
            if variant=='glb_many_members':doc['extras']={f'key{i}':i for i in range(1025)}
            else:
                doc['cameras']=[{'type':'perspective','perspective':{'yfov':1,'znear':0.1,'zfar':100}}]
                doc['nodes'].append({'camera':0,'translation':[0,0,4]});doc['scenes'][0]['nodes'].append(len(doc['nodes'])-1)
            encoded=json.dumps(doc,separators=(',',':')).encode();encoded+=b' '*((-len(encoded))%4)
            raw=raw[:12]+struct.pack('<II',len(encoded),0x4e4f534a)+encoded+raw[20+size:]
            raw=raw[:8]+struct.pack('<I',len(raw))+raw[12:];changed['assets/triangle.glb']=raw
        changed['character.json']=json.dumps(manifest).encode()
        out=io.BytesIO()
        with zipfile.ZipFile(out,'w',compression=zipfile.ZIP_STORED) as z:
            for name,data in changed.items():z.writestr(name,data)
        yield variant,out.getvalue()


def full_offsets(data):
    pos=140;nl,np,nt,ns=struct.unpack_from('<IIII',data,120)
    def text():
        nonlocal pos
        start=pos; n=struct.unpack_from('<I',data,pos)[0];pos+=4+n;return start
    layers=[];semantics=[]
    for _ in range(nl):
        at=text();layers.append({'id':at,'visible':pos+4,'opacity':pos+8,'parent':pos+12,'local_visible':pos+16,'local_opacity':pos+20});pos+=24
    for _ in range(np):text();pos+=12
    for _ in range(ns):
        at={key:text() for key in ('id','type','units','target2d','target3d')};at['initial']=pos+4;semantics.append(at);pos+=52
    for _ in range(nt):
        text();nv,triangles,deforms=struct.unpack_from('<III',data,pos+32);pos+=44+nv*16+triangles*12+deforms*(4+nv*8)
    pixels=struct.unpack_from('<I',data,136)[0];pos+=pixels
    return layers,semantics,pos


def full_hostile(data):
    layers,semantics,_=full_offsets(data);result={}
    def number(name,pos,value,fmt='<I'):
        altered=bytearray(data);struct.pack_into(fmt,altered,pos,value);result[name]=fix_digest(altered)
    def text(name,pos,value):
        n=struct.unpack_from('<I',data,pos)[0];result[name]=fix_digest(data[:pos]+struct.pack('<I',len(value))+value+data[pos+4+n:])
    for name,pos,value in [('profile',12,1),('mode',96,2),('absent_3d_default',96,1),('glb_flag',100,2),('parent_range',layers[0]['parent'],128),('parent_self',layers[0]['parent'],0),('local_visibility',layers[0]['local_visible'],2),('visibility_mismatch',layers[0]['visible'],0)]:number(name,pos,value)
    number('opacity_mismatch',layers[0]['opacity'],0.5,'<f');number('local_opacity_nan',layers[0]['local_opacity'],float('nan'),'<f')
    text('missing_glb_mapping',semantics[0]['target3d'],b'bone:missing.rotation')
    number('semantic_nan',semantics[0]['initial'],float('nan'),'<d')
    result['trailing']=fix_digest(data+b'x')
    # Redirect every part to layer0, leaving the second valid layer unbound.
    n=bytearray(data);pos=140
    def skip_text():
        nonlocal pos
        size=struct.unpack_from('<I',data,pos)[0];pos+=4+size
    nl,np,nt,ns=struct.unpack_from('<IIII',data,120)
    for _ in range(nl):skip_text();pos+=24
    for _ in range(np):skip_text();pos+=12
    for _ in range(ns):
        for _ in range(5):skip_text()
        pos+=52
    for _ in range(nt):
        skip_text();struct.pack_into('<I',n,pos+28,0);nv,tr,df=struct.unpack_from('<III',data,pos+32);pos+=44+nv*16+tr*12+df*(4+nv*8)
    result['unbound_layer']=fix_digest(n)
    return result


def glb_hostile(data):
    import copy
    _,_,start=full_offsets(data);pos=start
    def read_node():
        nonlocal pos
        kind=struct.unpack_from('<I',data,pos)[0];pos+=4
        size=struct.unpack_from('<I',data,pos)[0];pos+=4;key=data[pos:pos+size];pos+=size
        node={'kind':kind,'key':key}
        if kind==1:node['value']=struct.unpack_from('<I',data,pos)[0];pos+=4
        elif kind==2:node['value']=struct.unpack_from('<d',data,pos)[0];pos+=8
        elif kind==3:
            size=struct.unpack_from('<I',data,pos)[0];pos+=4;node['value']=data[pos:pos+size];pos+=size
        elif kind in (4,5):
            size=struct.unpack_from('<I',data,pos)[0];pos+=4;node['children']=[read_node() for _ in range(size)]
        return node
    tree=read_node();end=pos
    def child(n,key):return next(c for c in n['children'] if c['key']==key.encode())
    def at(n,*path):
        for key in path:n=n['children'][key] if isinstance(key,int) else child(n,key)
        return n
    def write_node(n):
        out=struct.pack('<II',n['kind'],len(n['key']))+n['key'];kind=n['kind']
        if kind==1:out+=struct.pack('<I',n['value'])
        elif kind==2:out+=struct.pack('<d',n['value'])
        elif kind==3:out+=struct.pack('<I',len(n['value']))+n['value']
        elif kind in (4,5):out+=struct.pack('<I',len(n['children']))+b''.join(write_node(c) for c in n['children'])
        return out
    result={}
    def change(name,path,value,field='value'):
        n=copy.deepcopy(tree);at(n,*path)[field]=value;result[name]=fix_digest(data[:start]+write_node(n)+data[end:])
    for name,path,value in [
        ('buffer_uri',('buffers',0),{'kind':3,'key':b'uri','value':b'file:escape'}),
    ]:
        n=copy.deepcopy(tree);at(n,*path)['children'].append(value);result[name]=fix_digest(data[:start]+write_node(n)+data[end:])
    for name,path,value in [
        ('view_range',('bufferViews',0,'byteLength'),1e9),('view_alignment',('bufferViews',0,'byteOffset'),1),
        ('component_type',('accessors',0,'componentType'),999),('accessor_count',('accessors',0,'count'),4),
        ('accessor_shape',('accessors',0,'type'),b'VEC4'),('bounds_mismatch',('accessors',0,'min',0),-1),
        ('node_cycle',('nodes',0,'children',0),0),('mesh_reference',('nodes',0,'mesh'),999),
        ('joint_reference',('skins',0,'joints',0),999),('inverse_bind_shape',('skins',0,'inverseBindMatrices'),0),
        ('animation_time_shape',('animations',0,'samplers',0,'input'),0),
        ('animation_target',('animations',0,'channels',0,'target','node'),999),
        ('texture_reference',('textures',0,'source'),999),('image_jpeg',('images',0,'mimeType'),b'image/jpeg'),
        ('material_reference',('materials',0,'pbrMetallicRoughness','baseColorTexture','index'),999),
        ('invalid_utf8',('nodes',0,'name'),b'\xc0\xaf'),('root_kind',(),4),
    ]:change(name,path,value,'kind' if name=='root_kind' else 'value')
    n=copy.deepcopy(tree);n['children'].append({'kind':0,'key':b'x'*1025});result['long_object_key']=fix_digest(data[:start]+write_node(n)+data[end:])
    n=copy.deepcopy(tree);n['children'].append(copy.deepcopy(n['children'][0]));result['duplicate_key']=fix_digest(data[:start]+write_node(n)+data[end:])
    n=copy.deepcopy(tree);branch={'kind':4,'key':b'extras','children':[]};n['children'].append(branch)
    for _ in range(65):next_branch={'kind':4,'key':b'','children':[]};branch['children'].append(next_branch);branch=next_branch
    result['deep_tree']=fix_digest(data[:start]+write_node(n)+data[end:])
    # Mutate decoded numeric domains while keeping packet hash/identity valid.
    count=struct.unpack_from('<I',data,pos)[0];pos+=4;arrays=[]
    for _ in range(count):
        n=struct.unpack_from('<I',data,pos)[0];arrays.append((pos,pos+4,n));pos+=4+8*n
    def number(name,offset,value,fmt='<d'):
        n=bytearray(data);struct.pack_into(fmt,n,offset,value);result[name]=fix_digest(n)
    number('float64_not_f32',arrays[0][1],0.1);number('float_nan',arrays[0][1],float('nan'))
    number('integer_fraction',arrays[1][1],0.5);number('integer_negative',arrays[1][1],-1);number('integer_overflow',arrays[1][1],65536)
    number('index_range',arrays[1][1],65535);number('nonunit_weights',arrays[3][1],2)
    number('time_not_increasing',arrays[7][1]+8,0);number('quaternion_nonunit',arrays[9][1],2)
    number('decoded_accessor_count',end,0,'<I');number('decoded_values_count',arrays[0][0],0,'<I')
    number('decoded_image_count',pos,0,'<I');number('image_width',pos+4,0,'<I');number('image_length',pos+12,0,'<I')
    return result


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--bin',type=Path,required=True);parser.add_argument('--corpus',type=Path,required=True);parser.add_argument('--evidence',type=Path,required=True)
    parser.add_argument('--case',action='append',default=[]);args=parser.parse_args();args.bin=args.bin.resolve();root=args.evidence.resolve()/uuid.uuid4().hex
    root.mkdir(parents=True);(root/'packets').mkdir();(root/'inputs').mkdir()
    repo=Path(__file__).resolve().parents[1]
    sources=['CMakeLists.txt','src/assets/cpu_model_packet.hpp','src/assets/cpu_model_packet.cpp','src/assets/validated_cpu_model.hpp','src/assets/cpu_admission_worker.cpp','src/moc3/import_sandbox.cpp','tests/assets/cpu_model_packet_guard.cpp','tests/assets/cpu_model_packet_reference.cpp','scripts/verify_cpu_full_transfer.py','src/assets/typed_glb_packet.hpp','src/assets/typed_glb_packet.cpp','src/validation/strict_value.cpp','src/validation/strict_json.cpp','src/validation/glb_validator.cpp','src/validation/glb_validator.hpp','src/assets/glb_reader.hpp','src/assets/validated_cpu_model.cpp','scripts/verify_cpu_admission_probe.py','tests/assets/fixtures/manifest.json']
    report={'result':'RUNNING','full_issue60_complete':False,'public_activation':False,'cases':[],
        'selected_cases':args.case,'source_sha256':{n:hashlib.sha256((repo/n).read_bytes()).hexdigest() for n in sources},
        'binary_sha256':{n:hashlib.sha256((args.bin/n).read_bytes()).hexdigest() for n in ['ars_cpu_admission_worker.exe','ars_cpu_model_broker.exe','ars_cpu_model_guard.exe','ars_cpu_model_reference.exe','ars_schema_worker.exe']}}
    def save(): (root/'evidence.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
    def passed(name,**proof):
        report['cases'].append({'case':name,'result':'PASS',**proof});save();print('PASS',name,flush=True)
    def guard(data,nonce,digest,mode='--check',baseline=None):
        return subprocess.run([str(args.bin/'ars_cpu_model_guard.exe'),str(nonce),digest,mode,*([str(baseline)] if baseline else [])],input=data,capture_output=True,timeout=30)
    def trial(name,data,expected,nonce):
        folder=root/('trial-'+uuid.uuid4().hex);folder.mkdir()
        worker=folder/'worker.exe';shutil.copyfile(args.bin/'ars_cpu_admission_worker.exe',worker)
        (folder/'worker-data.txt').write_bytes(b'generated native2D worker sentinel')
        profile='AvatarMocImport-'+uuid.uuid4().hex
        run=subprocess.run([str(args.bin/'ars_cpu_model_broker.exe'),str(worker),profile,'--cpu-model-full'],input=request(data,nonce),capture_output=True,timeout=15,cwd=folder)
        state=json.loads((folder/'launcher-state.json').read_text());cleanup=json.loads((folder/'job-cleanup.json').read_text())
        proof={'launcher_exit':run.returncode,'state':state,'cleanup':cleanup,'profile_removed':profile_absent(profile)}
        (folder/'proof.json').write_text(json.dumps(proof,indent=2))
        check(run.returncode==0 and state['worker_exit']==0 and state['wait_result']==0 and not state['output_overflow'] and cleanup['query_error']==0 and cleanup['active_processes']==0 and proof['profile_removed'],name+' isolation/cleanup')
        check(96<=len(run.stdout)<=CAP and run.stdout[:8]==b'ARSCPUFN',name+' packet framing')
        path=root/'packets'/(name+'.bin');path.write_bytes(run.stdout)
        decoded=guard(run.stdout,nonce,hashlib.sha256(data).hexdigest())
        check(decoded.returncode==(0 if expected==0 else 3),name+' host exit '+repr(decoded.stdout+decoded.stderr))
        parsed=json.loads(decoded.stdout);check(parsed['status']==expected and parsed['balanced'],name+' independent host status')
        if expected==0 or name=='packet_budget':
            source=root/'inputs'/(name+'.arschar');source.write_bytes(data)
            oracle=subprocess.run([str(args.bin/'ars_cpu_model_reference.exe'),str(args.bin/'ars_schema_worker.exe'),str(source),str(path),str(nonce),*(['--expect-budget'] if name=='packet_budget' else [])],capture_output=True,timeout=15)
            marker=b'NATIVE_VALID_PACKET_BUDGET_ROLLBACK_PASS' if name=='packet_budget' else b'NATIVE_FIELD_AND_SCENE_PARITY_PASS'
            check(oracle.returncode==0 and marker in oracle.stdout,name+' native reference '+repr(oracle.stderr+oracle.stdout))
        passed(name,status=expected,**proof)
        return run.stdout
    try:
        rows=json.loads((args.corpus/'manifest.json').read_text())
        base_packet=None;base_data=None
        for index,row in enumerate(rows):
            if not row['file'].endswith('.arschar') or (args.case and row['id'] not in args.case):continue
            data=(args.corpus/row['file']).read_bytes();check(hashlib.sha256(data).hexdigest()==row['sha256'],'fixture digest')
            expected=STATUS[row['expected']]
            packet=trial(row['id'],data,expected,index+1)
            if row['id']=='positive-2d':base_packet,base_data=packet,data
        if not args.case:
            for name,data in glb_extras((args.corpus/'positive-3d.arschar').read_bytes()):trial(name,data,0,1<<62)
        for name,data in (extras(base_data) if base_data is not None else []):
            trial(name,data,STATUS['OUT_OF_BUDGET'] if name=='packet_budget' else 0,1<<63)
        for name,data in (full_hostile(base_packet).items() if base_packet is not None else []):
            result=guard(data,1,hashlib.sha256(base_data).hexdigest(),'--retain',root/'packets/positive-2d.bin')
            check(result.returncode==3 and json.loads(result.stdout)['status']!=0,'malicious packet accepted '+name)
            passed('hostile_'+name)
        if not args.case:
            full_packet=(root/'packets/positive-dual-full.bin').read_bytes()
            full_source=(root/'inputs/positive-dual-full.arschar').read_bytes()
            row_index=next(i+1 for i,row in enumerate(rows) if row['id']=='positive-dual-full')
            for name,data in glb_hostile(full_packet).items():
                result=guard(data,row_index,hashlib.sha256(full_source).hexdigest(),'--retain',root/'packets/positive-dual-full.bin')
                check(result.returncode==3 and json.loads(result.stdout)['status']!=0,'malicious GLB packet accepted '+name)
                passed('hostile_glb_'+name)
            result=guard(full_packet,row_index,hashlib.sha256(full_source).hexdigest(),'--stress')
            check(result.returncode==0 and b'STRESS_PASS' in result.stdout,'GLB allocation/truncation/lifetime '+repr(result.stdout+result.stderr))
            passed('glb_allocation_truncation_lifetime',output=result.stdout.decode().strip())
        if base_packet is not None:
            stressed=guard(base_packet,1,hashlib.sha256(base_data).hexdigest(),'--stress')
            check(stressed.returncode==0 and b'STRESS_PASS' in stressed.stdout,'native OOM/lifetime/truncation '+repr(stressed.stdout+stressed.stderr))
            passed('native_allocation_truncation_lifetime',output=stressed.stdout.decode().strip())
        for name,digest in report['source_sha256'].items():check(hashlib.sha256((repo/name).read_bytes()).hexdigest()==digest,'source changed '+name)
        for name,digest in report['binary_sha256'].items():check(hashlib.sha256((args.bin/name).read_bytes()).hexdigest()==digest,'binary changed '+name)
        report['result']='PASS'
    except Exception as error:
        report['result']='FAIL';report['error']=str(error);raise
    finally:save()
    print('FULL_TYPED_TRANSFER_PASS',len(report['cases']),str(root),flush=True)


if __name__=='__main__':main()
