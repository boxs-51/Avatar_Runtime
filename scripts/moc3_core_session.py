"""Private persistent trusted-local core/physics/host owner; no model metadata exports."""
import json
import math
import threading

from moc3_local_integration import ProductBridge
from moc3_local_physics import Bridge
from moc3_local_pixels import PixelError,require
from moc3_physics import Physics,STEP_NS


def validate_control(value):
    if type(value) is not dict or set(value)!={'sequence','drive','physics','reset','lease_ms'}:
        raise ValueError('INVALID_CONTROL')
    if type(value['sequence']) is not int or not 1<=value['sequence']<=10000:
        raise ValueError('INVALID_SEQUENCE')
    drive=value['drive']
    if drive is not None and (type(drive) not in (float,int) or not 0<=drive<=1 or not math.isfinite(drive)):
        raise ValueError('INVALID_DRIVE')
    if type(value['physics']) is not bool or type(value['reset']) is not bool:
        raise ValueError('INVALID_TOGGLE')
    if type(value['lease_ms']) is not int or not 100<=value['lease_ms']<=2000:
        raise ValueError('INVALID_LEASE')
    return dict(value)


class CoreSession:
    def __init__(self,package,folder,hardware=False,*,binary_dir=None):
        self.owner=threading.get_ident();self.bridge=None;self.closed=False;self.faulted=False
        self.folder=folder;self.sequence=0;self.tick=0
        binary=binary_dir/'ars_moc3_product_bridge.exe' if binary_dir is not None else 'ars_moc3_product_bridge.exe'
        bootstrap=Bridge(package,folder/'catalog',hardware,binary=binary)
        try:
            physics=Physics(package.linked_physics_bytes,bootstrap.catalog)
            self.drive=next((b.key for c in physics.chains for b in c.inputs
                             if b.axis=='Angle' and b.weight>0 and
                             bootstrap.catalog[b.key][2]>bootstrap.catalog[b.key][1]),None)
            require(self.drive is not None,'F4B_NO_LINKED_DRIVE')
            bootstrap.finish()
        finally:bootstrap.close()
        try:
            self.bridge=ProductBridge(package,folder,hardware,self.drive,binary=binary)
            self.physics=Physics(package.linked_physics_bytes,self.bridge.catalog)
        except BaseException:
            if self.bridge:self.bridge.close()
            raise

    def _owner(self):
        require(threading.get_ident()==self.owner,'F4B_WRONG_OWNER')
        require(not self.closed and not self.faulted,'F4B_SESSION_CLOSED')

    def update(self,drive=None,*,enabled=True,reset=False):
        self._owner()
        validate_control(dict(sequence=1,drive=drive,physics=enabled,reset=reset,lease_ms=100))
        require(self.sequence<9500,'F4B_FRAME_LIMIT')
        low,default,high=self.bridge.catalog[self.drive]
        value=default+(high-default)*drive if drive is not None else None
        sequence=self.sequence+1
        try:
            core,generation=self.bridge.prepare_core(sequence,[(self.drive,value)] if value is not None else [])
            require(core is not None and generation==sequence,'F4B_CORE_GENERATION')
            require(core[self.drive]==float(f'{(value if value is not None else default):.9g}'),'F4B_CORE_VALUE')
            tick=self.tick+1
            candidate=self.physics.prepare(tick*6*STEP_NS,core,enabled=enabled,reset=reset)
            rgba,geometry=self.bridge.frame(dict(candidate.parameters))
            require(self.bridge.command('CORE_COMMIT')==['CORE_CURRENT',str(generation)],'F4B_FINAL_CORE_FENCE')
            self.physics.commit(candidate)
            metadata=json.loads((self.folder/'latest.json').read_text(encoding='utf-8'))
            require(metadata['frame_generation']==generation,'F4B_HOST_GENERATION')
            meta={k:metadata[k] for k in ('width','height','frame_generation','snapshot_generation',
                                        'target_epoch','device_generation','backend')}
            meta.update(core_generation=generation,behavior='CORE_DISABLED' if not enabled else
                        'CORE_REPLAY' if reset else 'CORE_RELEASE' if drive is None else 'CORE_PHYSICS')
            ppm=(self.folder/metadata['image']).read_bytes()
            self.sequence=sequence;self.tick=tick
            return meta,rgba,ppm,geometry
        except BaseException:
            self.faulted=True
            raise

    def finish(self):
        self._owner()
        try:return self.bridge.finish()
        finally:self.closed=True;self.bridge.close()

    def close(self):
        require(threading.get_ident()==self.owner,'F4B_WRONG_OWNER')
        if self.bridge:self.bridge.close()
        self.closed=True
