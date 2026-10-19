"""Independent bounded Physics3 adapter; no Cubism-equivalence claim.

Consumes immutable manifest-linked bytes and the actual evaluator catalog.
Prepare/commit makes clock and particle state transactional with downstream work.
"""
from __future__ import annotations
from dataclasses import dataclass
import json
import math
import threading

STEP_NS = 16_666_667

class PhysicsError(ValueError):
    pass

def require(ok, code):
    if not ok:
        raise PhysicsError(code)

def number(value, low=-1e6, high=1e6):
    require(type(value) in (int, float) and low <= value <= high and math.isfinite(value),
            'INVALID_NUMBER')
    return float(value)

def integer(value, low, high):
    require(type(value) is int and low <= value <= high, 'INVALID_INTEGER')
    return value

def obj(value, keys, optional=()):
    require(type(value) is dict and set(keys) <= value.keys() and
            not value.keys() - set(keys) - set(optional), 'INVALID_OBJECT')
    return value

def vector(value):
    obj(value, ('X', 'Y'))
    return (number(value['X'], -1e4, 1e4), number(value['Y'], -1e4, 1e4))

def sequence(value, low, high):
    require(type(value) is list and low <= len(value) <= high, 'INVALID_ARRAY')
    return value

def unique(pairs):
    result = {}
    for key, value in pairs:
        require(key not in result, 'DUPLICATE_KEY')
        result[key] = value
    return result

@dataclass(frozen=True)
class Binding:
    key: str
    axis: str
    weight: float
    reflect: bool
    vertex: int = 0
    scale: float = 1

@dataclass(frozen=True)
class Particle:
    position: tuple[float, float]
    mobility: float
    delay: float
    acceleration: float
    radius: float

@dataclass(frozen=True)
class Chain:
    inputs: tuple[Binding, ...]
    outputs: tuple[Binding, ...]
    particles: tuple[Particle, ...]
    position_range: tuple[float, float, float]
    angle_range: tuple[float, float, float]

@dataclass(frozen=True)
class Candidate:
    owner: object
    generation: int
    timestamp: int
    remainder: int
    state: tuple
    parameters: tuple[tuple[str, float], ...]
    steps: int

class Physics:
    def __init__(self, linked_bytes: bytes, catalog: dict):
        require(type(linked_bytes) is bytes and 0 < len(linked_bytes) <= 1024*1024,
                'LINKED_PHYSICS_BYTES_REQUIRED')
        self.owner_thread = threading.get_ident()
        self.identity = object()
        require(type(catalog) is dict and 0 < len(catalog) <= 4096, 'INVALID_CATALOG')
        self.catalog = {}
        for key, bounds in catalog.items():
            require(isinstance(key, str) and key.isascii() and 0 < len(key) <= 255 and
                    not any(c.isspace() for c in key), 'INVALID_PARAMETER_ID')
            require(type(bounds) in (list,tuple) and len(bounds) == 3, 'INVALID_CATALOG')
            a,b,c = map(number, bounds)
            require(a <= b <= c, 'INVALID_CATALOG')
            self.catalog[key] = (a,b,c)
        try:
            data = json.loads(linked_bytes.decode('utf-8'), object_pairs_hook=unique,
                              parse_constant=lambda _: (_ for _ in ()).throw(PhysicsError('NONFINITE_JSON')))
        except (ValueError, UnicodeError, RecursionError) as error:
            raise PhysicsError('INVALID_PHYSICS_JSON') from error
        obj(data, ('Version','Meta','PhysicsSettings'))
        require(type(data['Version']) is int and data['Version'] == 3, 'UNSUPPORTED_PHYSICS_VERSION')
        meta = obj(data['Meta'], ('PhysicsSettingCount','TotalInputCount','TotalOutputCount',
                  'VertexCount','EffectiveForces'), ('Fps','PhysicsDictionary'))
        forces = obj(meta['EffectiveForces'], ('Gravity','Wind'))
        gravity = vector(forces['Gravity'])
        length = math.hypot(*gravity)
        require(length > 0, 'ZERO_GRAVITY')
        self.gravity = (gravity[0]/length, -gravity[1]/length)
        self.wind = vector(forces['Wind'])
        if 'Fps' in meta:
            number(meta['Fps'], 1, 240)
        groups = sequence(data['PhysicsSettings'], 1, 128)
        chains = []; ids = set(); ni=no=nv=0
        for group in groups:
            obj(group, ('Id','Input','Output','Vertices','Normalization'))
            require(type(group['Id']) is str and 0 < len(group['Id']) <= 255 and
                    group['Id'] not in ids, 'INVALID_CHAIN_ID');ids.add(group['Id'])
            norm = obj(group['Normalization'], ('Position','Angle'))
            def limits(value):
                obj(value, ('Minimum','Default','Maximum'))
                a,b,c = (number(value[k], -1e4, 1e4) for k in ('Minimum','Default','Maximum'))
                require(a <= b <= c, 'INVALID_NORMALIZATION')
                return a,b,c
            position, angle = limits(norm['Position']), limits(norm['Angle'])
            particles = []
            for i,p in enumerate(sequence(group['Vertices'], 2, 2048)):
                obj(p, ('Position','Mobility','Delay','Acceleration','Radius'))
                particles.append(Particle(vector(p['Position']), number(p['Mobility'],0,1),
                    number(p['Delay'],0,1), number(p['Acceleration'],0,100), number(p['Radius'],0,1e4)))
                require(i == 0 or particles[-1].radius > 0, 'ZERO_PARTICLE_RADIUS')
            def bindings(entries, output):
                result=[];seen=set()
                for entry in sequence(entries, 1, 8192):
                    target = 'Destination' if output else 'Source'
                    fields = (target,'Weight','Type','Reflect') + (('VertexIndex','Scale') if output else ())
                    obj(entry, fields);ref=obj(entry[target], ('Target','Id'))
                    require(ref['Target'] == 'Parameter' and type(ref['Id']) is str and
                            ref['Id'] in self.catalog, 'UNKNOWN_PARAMETER_BINDING')
                    require(entry['Type'] in ('X','Y','Angle') and type(entry['Reflect']) is bool,
                            'UNSUPPORTED_BINDING')
                    key=(ref['Id'],entry['Type']) if not output else ref['Id']
                    require(key not in seen, 'DUPLICATE_BINDING');seen.add(key)
                    result.append(Binding(ref['Id'],entry['Type'],number(entry['Weight'],0,100)/100,
                        entry['Reflect'],integer(entry['VertexIndex'],1,len(particles)-1) if output else 0,
                        number(entry['Scale']) if output else 1))
                return tuple(result)
            inputs, outputs = bindings(group['Input'],False), bindings(group['Output'],True)
            ni+=len(inputs);no+=len(outputs);nv+=len(particles)
            require(ni+no <= 8192 and nv <= 2048, 'PHYSICS_BUDGET')
            chains.append(Chain(inputs,outputs,tuple(particles),position,angle))
        for key,total in (('PhysicsSettingCount',len(chains)),('TotalInputCount',ni),
                          ('TotalOutputCount',no),('VertexCount',nv)):
            require(integer(meta[key],0,8192) == total, 'META_COUNT_MISMATCH')
        self.chains = tuple(chains)
        self.timestamp = 0;self.remainder = 0;self.generation = 0
        self.state = self.rest()

    def rest(self):
        return tuple(tuple((p.position, (0.,0.)) for p in c.particles) for c in self.chains)

    def _owner(self):
        require(threading.get_ident() == self.owner_thread, 'WRONG_THREAD')

    def normalized(self, key, value, bounds):
        low,mid,high = self.catalog[key];a,b,c = bounds
        if value < mid and mid > low:
            return b+(value-mid)*(b-a)/(mid-low)
        if value > mid and high > mid:
            return b+(value-mid)*(c-b)/(high-mid)
        return b

    def _integrate(self, chain, state, base):
        x=y=angle=0.
        for binding in chain.inputs:
            bounds = chain.angle_range if binding.axis == 'Angle' else chain.position_range
            value = self.normalized(binding.key, base[binding.key], bounds)*binding.weight
            if binding.reflect: value=-value
            if binding.axis == 'X': x+=value
            elif binding.axis == 'Y': y+=value
            else: angle+=value
        radians = math.radians(-angle)
        gx=self.gravity[0]*math.cos(radians)-self.gravity[1]*math.sin(radians)
        gy=self.gravity[0]*math.sin(radians)+self.gravity[1]*math.cos(radians)
        root=chain.particles[0].position
        result=[((root[0]+x,root[1]+y),(0.,0.))]
        dt=STEP_NS/1e9
        for i,p in enumerate(chain.particles[1:],1):
            old,velocity=state[i];parent=result[i-1][0]
            delay=dt*p.delay*30
            nx=old[0]+velocity[0]*delay+(gx*p.acceleration+self.wind[0])*delay*delay
            ny=old[1]+velocity[1]*delay+(gy*p.acceleration+self.wind[1])*delay*delay
            dx,dy=nx-parent[0],ny-parent[1];length=math.hypot(dx,dy)
            if length < 1e-12: dx,dy,length=gx,gy,1.
            pos=(parent[0]+dx/length*p.radius,parent[1]+dy/length*p.radius)
            vel=tuple((pos[j]-old[j])/delay*p.mobility if delay else 0. for j in (0,1))
            require(all(math.isfinite(v) and abs(v) <= 1e6 for v in (*pos,*vel)), 'UNSTABLE_PHYSICS')
            result.append((pos,vel))
        return tuple(result)

    def prepare(self, timestamp: int, manual: dict | None = None, *, enabled=True, reset=False):
        self._owner()
        integer(timestamp,0,2**63-1)
        require(type(enabled) is bool and type(reset) is bool, 'INVALID_TOGGLE')
        require(reset or timestamp >= self.timestamp, 'CLOCK_REWIND')
        require(manual is None or type(manual) is dict, 'INVALID_MANUAL_INPUT')
        base={key:bounds[1] for key,bounds in self.catalog.items()}
        for key,value in (manual or {}).items():
            require(key in self.catalog, 'UNKNOWN_MANUAL_PARAMETER')
            base[key]=number(value,self.catalog[key][0],self.catalog[key][2])
        elapsed=0 if reset else timestamp-self.timestamp
        remainder=0 if reset else self.remainder
        require(elapsed+remainder <= 8*STEP_NS, 'CLOCK_BACKLOG')
        state=self.rest() if reset or not enabled else self.state
        steps=0
        if enabled:
            steps,remainder=divmod(elapsed+remainder,STEP_NS)
            for _ in range(steps):
                state=tuple(self._integrate(c,s,base) for c,s in zip(self.chains,state))
            for chain,particles in zip(self.chains,state):
                for binding in chain.outputs:
                    p=particles[binding.vertex][0];parent=particles[binding.vertex-1][0]
                    dx,dy=p[0]-parent[0],p[1]-parent[1]
                    value=math.degrees(math.atan2(dx,dy)) if binding.axis=='Angle' else dx if binding.axis=='X' else dy
                    value*=binding.scale*(-1 if binding.reflect else 1)
                    value=base[binding.key]*(1-binding.weight)+value*binding.weight
                    lo,_,hi=self.catalog[binding.key]
                    base[binding.key]=min(hi,max(lo,number(value)))
        else:
            remainder=0
        return Candidate(self.identity,self.generation+1,timestamp,remainder,state,tuple(base.items()),steps)

    def commit(self, candidate: Candidate):
        self._owner()
        require(type(candidate) is Candidate and candidate.owner is self.identity and
                candidate.generation == self.generation+1, 'STALE_PHYSICS_CANDIDATE')
        self.state=candidate.state;self.timestamp=candidate.timestamp
        self.remainder=candidate.remainder;self.generation=candidate.generation
