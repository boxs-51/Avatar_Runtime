"""Bounded private frame transport; payload must never be printed to logs."""
import base64
import json
import struct
import zlib

MAX_LINE=3*1024*1024
MAX_RAW=2*1024*1024
PREFIX='F4_PACKET '

def validate(meta,rgba,ppm):
    required={'width','height','frame_generation','snapshot_generation','target_epoch',
              'device_generation','core_generation','backend','behavior'}
    if type(meta) is not dict or set(meta)!=required:
        raise ValueError('invalid frame fields')
    for key in required-{'backend','behavior'}:
        if type(meta[key]) is not int or not 1<=meta[key]<=10001:
            raise ValueError('invalid frame identity')
    if meta['width']!=512 or meta['height']!=512 or meta['backend']!='D3D11_WARP':
        raise ValueError('invalid frame target')
    if not meta['frame_generation']==meta['target_epoch']==meta['core_generation'] or \
            meta['snapshot_generation']!=meta['frame_generation']+1:
        raise ValueError('inconsistent frame identity')
    if meta['behavior'] not in ('CORE_PHYSICS','CORE_RELEASE','CORE_REPLAY','CORE_DISABLED','CORE_REENABLED'):
        raise ValueError('invalid frame behavior')
    if len(rgba)!=512*512*4 or len(ppm)!=512*512*3+15 or not ppm.startswith(b'P6\n512 512\n255\n'):
        raise ValueError('invalid frame bytes')

def encode(meta,rgba,ppm):
    validate(meta,rgba,ppm)
    header=json.dumps(meta,separators=(',',':'),allow_nan=False).encode('ascii')
    raw=struct.pack('<III',len(header),len(rgba),len(ppm))+header+rgba+ppm
    value=PREFIX+base64.b64encode(zlib.compress(raw)).decode('ascii')
    if len(value)>=MAX_LINE:raise ValueError('frame transport budget')
    return value

def decode(line):
    if not isinstance(line,str) or not line.startswith(PREFIX) or len(line)>=MAX_LINE:
        raise ValueError('invalid frame transport')
    compressed=base64.b64decode(line[len(PREFIX):],validate=True)
    decoder=zlib.decompressobj()
    try:raw=decoder.decompress(compressed,MAX_RAW+1)
    except zlib.error as error:raise ValueError('invalid compressed frame') from error
    if len(raw)>MAX_RAW or not decoder.eof or decoder.unused_data or decoder.unconsumed_tail or len(raw)<12:
        raise ValueError('frame decompression budget')
    n,r,p=struct.unpack_from('<III',raw)
    if n>16384 or 12+n+r+p!=len(raw):raise ValueError('invalid frame sizes')
    def unique(pairs):
        result={}
        for key,value in pairs:
            if key in result:raise ValueError('duplicate frame field')
            result[key]=value
        return result
    try:
        meta=json.loads(raw[12:12+n],object_pairs_hook=unique,
                        parse_constant=lambda _: (_ for _ in ()).throw(ValueError('invalid constant')))
    except (ValueError,UnicodeError,RecursionError) as error:
        raise ValueError('invalid frame metadata') from error
    rgba=raw[12+n:12+n+r];ppm=raw[12+n+r:]
    validate(meta,rgba,ppm)
    return meta,rgba,ppm
