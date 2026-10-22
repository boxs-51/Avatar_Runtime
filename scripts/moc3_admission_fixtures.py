"""Generated packet/ZIP/PNG fixtures. No copyrighted model assets."""
import io
import json
import struct
import zipfile
import zlib


def archive(files):
    out=io.BytesIO()
    with zipfile.ZipFile(out,'w',zipfile.ZIP_DEFLATED) as bundle:
        for name,data in files.items():bundle.writestr(name,data)
    return out.getvalue()


def png(width=1,height=1,*,pixel=b'\xff\x00\x00\xff'):
    def chunk(kind,data):return struct.pack('>I',len(data))+kind+data+struct.pack('>I',zlib.crc32(kind+data))
    header=struct.pack('>IIBBBBB',width,height,8,6,0,0,0)
    rows=b''.join(b'\0'+pixel*width for _ in range(height))
    return b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',header)+chunk(b'IDAT',zlib.compress(rows))+chunk(b'IEND',b'')


def basic_package(texture=None):
    manifest={'Version':3,'FileReferences':{'Moc':'m.moc3','Textures':['t.png']}}
    return {'m.model3.json':json.dumps(manifest).encode(),'m.moc3':b'MOC3'+bytes(124),'t.png':png() if texture is None else texture}


def drawable_packet(nonce=42):
    header=b'ARSDRAW1'+struct.pack('<IQQQIII',1,0,1,nonce,0,1,1)
    atlas=struct.pack('<IIQ',1,1,4)+b'\xff\x00\x00\xff'
    # A full square, clockwise in texture space, double-sided, unmasked straight red texel.
    draw=struct.pack('<iiiiIf8fIII',0,0,0,0,3,1,1,1,1,1,0,0,0,1,4,6,0)
    vertices=struct.pack('<16f',-1,-1,0,0,1,-1,1,0,-1,1,0,1,1,1,1,1)
    data=bytearray(header+atlas+draw+vertices+struct.pack('<6H',0,1,2,2,1,3))
    struct.pack_into('<Q',data,12,len(data));return bytes(data)


def hostile_packets():
    valid=drawable_packet();cases={}
    def field(name,offset,value,fmt='<I'):
        data=bytearray(valid);struct.pack_into(fmt,data,offset,value);cases[name]=bytes(data)
    cases['truncated']=valid[:-1];cases['trailing']=valid+b'x';cases['bad_magic']=b'X'+valid[1:]
    field('version',8,2);field('total_length',12,2**64-1,'<Q');field('stale_generation',20,2,'<Q')
    field('stale_request',28,43,'<Q');field('unknown_features',36,128);field('physics_incomplete',36,1)
    field('atlas_count',40,17);field('drawable_count',44,4097);field('atlas_edge',48,8193);field('atlas_bytes',56,2**64-1,'<Q')
    draw=68;field('atlas_reference',draw,1);field('blend',draw+12,3);field('flags',draw+16,8)
    field('nan_opacity',draw+20,float('nan'),'<f');field('negative_color',draw+24,-.1,'<f')
    field('vertex_count',draw+56,65537);field('nontriangle_indices',draw+60,5)
    field('infinite_position',draw+68,float('inf'),'<f');field('unbounded_uv',draw+76,17.,'<f')
    field('index_reference',draw+68+64,4,'<H')
    # Self-reference with a correctly extended packet, to reach graph validation.
    bad=bytearray(valid)+struct.pack('<I',0);struct.pack_into('<I',bad,draw+64,1);struct.pack_into('<Q',bad,12,len(bad));cases['self_mask']=bytes(bad)
    return cases
