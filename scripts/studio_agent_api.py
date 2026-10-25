"""Local-only agent command API for Native Studio. No network or model export."""
from __future__ import annotations
import math
import os
from pathlib import Path
import secrets
import re
import time
VERSION="1.0"
def operation_fields()->dict[str,set[str]]:
    """Existing V1 operation contract for local Agent tool discovery."""
    return {
        "studio.status":{"op"}, "studio.metrics":{"op"},
        "studio.epoch":{"op"},
        "studio.g5.status":{"op"},
        "studio.g5.legacy_frame.status":{"op"},
        "g4.mesh.meta":{"op","generation","drawable"},
        "g4.mesh.vertex_count":{"op","generation","drawable"},
        "g4.mesh.vertex":{"op","generation","drawable","index"},
        "g4.mesh.triangle":{"op","generation","drawable","index"},
        "g4.rig.meta":{"op","generation","drawable"},
        "g4.rig.revision":{"op","generation","drawable"},
        "g4.rig.entry":{"op","generation","drawable","kind","index"},
        "param.id.at":{"op","generation","index"},
        "part.id.at":{"op","generation","index"},
        "param.find.at":{"op","generation","id"},
        "part.find.at":{"op","generation","id"},
        "param.inspect.at":{"op","generation","index"}, "part.inspect.at":{"op","generation","index"},
        "studio.tab":{"op","index"}, "studio.background":{"op","index"},
        "param.inspect":{"op","index"}, "part.inspect":{"op","index"},
        "xpbd.read":{"op","name"},
        "studio.play":{"op","enabled"}, "studio.step":{"op"},
        "studio.expression":{"op","index"},
        "studio.toggle":{"op","feature","enabled"},
        "studio.setting":{"op","name","value"},
        "studio.camera":{"op","mode"}, "studio.zoom":{"op","value"},
        "studio.pan":{"op","x","y"}, "studio.reset":{"op"},
        "studio.preset":{"op","action"}, "studio.capture":{"op","kind"},
        "param.set":{"op","index","value"}, "param.clear":{"op","index"},
        "param.reset":{"op"}, "part.opacity":{"op","index","value"},
        "rig.vertex":{"op","index"}, "rig.store":{"op","action"},
        "xpbd.set":{"op","name","value"},
        "xpbd.select":{"op","first","second","joint"},
        "xpbd.apply":{"op"}, "xpbd.disable":{"op"}, "xpbd.inspect":{"op"},
        "layer.topology":{"op"}, "layer.topology.scan":{"op"},
        "layer.component.inspect":{"op","component"},
        "layer.component.adopt":{"op","component"},
        "rig.select":{"op","drawable"},
        "rig.selection":{"op","generation"},
        "rig.select.at":{"op","generation","revision","drawable"},
        "layer.select":{"op","generation","revision","drawable"},
        "layer.inspect":{"op","generation"},
        "layer.physics.status":{"op","generation"},
        "layer.physics.point":{"op","generation","vertex"},
        "layer.physics.bind":{"op","generation","revision","layer"},
        "layer.physics.stop":{"op","generation","revision"},
        "layer.model.compile":{"op","generation","revision"},
        "layer.model.open":{"op","generation","revision"},
        "layer.project.status":{"op","generation"},
        "layer.project.by_id":{"op","generation","revision","id"},
        "layer.project.vertex_by_id":{"op","generation","revision","id","vertex_id"},
        "layer.active.id":{"op","generation","revision","id"},
        "layer.project.layer":{"op","generation","layer"},
        "layer.project.point":{"op","generation","layer","vertex"},
        "layer.project.face":{"op","generation","layer","triangle"},
        "layer.project.save":{"op","generation","revision"},
        "layer.project.load":{"op","generation","revision"},
        "layer.create":{"op","generation","revision","id"},
        "layer.copy.drawable":{"op","generation","revision","drawable","id"},
        "layer.delete":{"op","generation","revision","layer"},
        "layer.duplicate":{"op","generation","revision","layer","id"},
        "layer.reorder":{"op","generation","revision","layer","target"},
        "layer.active":{"op","generation","layer"},
        "layer.point.add":{"op","generation","revision","layer","x","y"},
        "layer.uv.set":{"op","generation","revision","layer","vertex","u","v"},
        "layer.material.set":{"op","generation","revision","layer","r","g","b","opacity"},
        "layer.rig.pin":{"op","generation","revision","layer","vertex"},
        "layer.rig.weight":{"op","generation","revision","layer","vertex","weight"},
        "layer.rig.parent":{"op","generation","revision","layer","vertex","parent","stiffness"},
        "layer.rig.links":{"op","generation"},
        "layer.rig.link.inspect":{"op","generation","revision","index"},
        "layer.physics.point_by_id":{"op","generation","id","vertex_id"},
        "layer.rig.link":{"op","generation","revision","parent_id","parent_vertex_id","child_id","child_vertex_id","stiffness"},
        "layer.rig.unlink":{"op","generation","revision","parent_id","parent_vertex_id","child_id","child_vertex_id"},
        "layer.point.move":{"op","generation","revision","layer","vertex","x","y"},
        "layer.point.delete":{"op","generation","revision","layer","vertex"},
        "layer.face.add":{"op","generation","revision","layer","a","b","c"},
        "layer.face.delete":{"op","generation","revision","layer","triangle"},
        "layer.edit.undo":{"op","generation","revision"},
        "layer.edit.redo":{"op","generation","revision"},
        "layer.cursor":{"op","generation"},
        "layer.vertex.select":{"op","generation","revision","index"},
        "layer.triangle.select":{"op","generation","revision","index"},
        "layer.overlay":{"op","generation","revision","enabled"},
        "layer.isolate":{"op","generation","revision","enabled"},
        "rig.history":{"op","generation"},
        "rig.undo":{"op","generation","revision"},
        "rig.redo":{"op","generation","revision"},
        "rig.anchor":{"op","vertex","enabled"},
        "rig.beam":{"op","a","b","enabled"},
        "rig.parent":{"op","parent","child","stiffness","enabled"},
        "rig.parent.inspect":{"op","child"},
        "rig.cross":{"op","target","parent","child","stiffness","enabled"},
        "rig.cross.inspect":{"op"},
        "rig.brush":{"op","radius","weight"},
        "rig.weight":{"op","vertex","weight"},
        "rig.weight.inspect":{"op","vertex"},
        "layer.component.relation":{"op","first","second"},
        "rig.apply":{"op"},
        "rig.inspect":{"op"},
        "layer.scan":{"op"},
        "layer.suggest":{"op"},
        "layer.adopt":{"op"},
        "layer.label":{"op","kind"},
        "physics.set":{"op","gravity","wind","damping"},
        "action.trigger":{"op","kind","strength"},
    }

def validate_command(cmd:dict)->str:
    if not isinstance(cmd,dict) or not isinstance(cmd.get("op"),str):
        raise ValueError("invalid operation")
    op=cmd["op"]
    fields=operation_fields()
    if op not in fields or set(cmd)!=fields[op]:raise ValueError("invalid fields")
    def integer(k):
        v=cmd[k]
        if type(v) is not int or not 0<=v<=2047:raise ValueError("index out of bounds")
        return v
    def finite(k,low,high):
        v=cmd[k]
        if type(v) not in (float,int) or not math.isfinite(v) or not low<=v<=high:
            raise ValueError("invalid scalar")
        return format(v,".9g")
    def boolean(k):
        if type(cmd[k]) is not bool:raise ValueError("expected bool")
        return int(cmd[k])
    singles={"studio.g5.legacy_frame.status":"G5_LEGACY_FRAME_STATUS","studio.g5.status":"G5_LOCAL_STATUS","studio.epoch":"STUDIO_EPOCH","studio.status":"STUDIO_STATUS","studio.step":"STUDIO_STEP",
             "studio.reset":"STUDIO_RESET","studio.metrics":"STUDIO_METRICS",
             "param.reset":"PARAM_RESET","xpbd.apply":"XPBD_APPLY",
             "xpbd.disable":"XPBD_DISABLE","xpbd.inspect":"XPBD_INSPECT"}
    if op.startswith("g4."):
        gen=cmd["generation"]
        if type(gen) is not int or not 0<gen<2**63:raise ValueError("generation")
        drawable=integer("drawable")
        if op=="g4.mesh.meta":return f"G4_MESH_META {gen} {drawable}"
        if op=="g4.mesh.vertex_count":return f"G4_MESH_VERTEX_COUNT {gen} {drawable}"
        if op=="g4.rig.meta":return f"G4_RIG_META {gen} {drawable}"
        if op=="g4.rig.revision":return f"G4_RIG_REV {gen} {drawable}"
        index=integer("index")
        if op=="g4.mesh.vertex":return f"G4_MESH_VERTEX {gen} {drawable} {index}"
        if op=="g4.mesh.triangle":return f"G4_MESH_TRI {gen} {drawable} {index}"
        kind=cmd["kind"]
        if type(kind) is not str or kind not in ("anchor","beam","parent","weight","cross"):
            raise ValueError("kind")
        return f"G4_RIG_ENTRY {gen} {drawable} {kind} {index}"
    if op in singles:return singles[op]
    if op in ("param.id.at","part.id.at","param.find.at","part.find.at"):
        gen=cmd["generation"]
        if type(gen) is not int or not 0<gen<2**63:
            raise ValueError("invalid generation")
        prefix="PARAM_" if op.startswith("param.") else "PART_"
        if op.endswith("id.at"):
            return f"{prefix}ID_AT {gen} {integer('index')}"
        value=cmd["id"]
        if type(value) is not str or "/" in value or "\\" in value or any(
            ord(ch)<32 or 127<=ord(ch)<=159 for ch in value
        ):
            raise ValueError("invalid model ID")
        try:
            encoded=value.encode("utf-8",errors="strict")
        except UnicodeError as exc:
            raise ValueError("invalid UTF-8 model ID") from exc
        if not 1<=len(encoded)<=96:
            raise ValueError("invalid model ID size")
        return f"{prefix}FIND_AT {gen} {encoded.hex()}"
    if op in ("param.inspect.at","part.inspect.at"):
        generation=cmd["generation"]
        if type(generation) is not int or not 0<generation<2**63:
            raise ValueError("invalid model generation")
        index=integer("index")
        return ("PARAM_INSPECT_AT" if op=="param.inspect.at"
                else "PART_INSPECT_AT")+f" {generation} {index}"
    if op in ("studio.tab","studio.background"):
        index=integer("index")
        if index>3:raise ValueError("invalid UI index")
        return ("STUDIO_TAB" if op=="studio.tab" else "STUDIO_BACKGROUND")+f" {index}"
    if op in ("param.inspect","part.inspect"):
        return ("PARAM_INSPECT" if op=="param.inspect" else "PART_INSPECT")+f" {integer('index')}"
    if op=="studio.play":return f"STUDIO_PLAY {boolean('enabled')}"
    if op=="studio.expression":
        i=integer("index")
        if i>8:raise ValueError("invalid expression")
        return f"STUDIO_EXPRESSION {i}"
    if op=="studio.toggle":
        name=cmd["feature"]
        if type(name) is not str or name not in ("physics","blink","breath","gaze",
             "speech","cycle","grid","particles"):raise ValueError("toggle")
        return f"STUDIO_TOGGLE {name} {boolean('enabled')}"
    settings={"motion_speed":(.1,2.5),"motion_amount":(0,1.5),
              "wind_drive":(0,1),"blink_interval":(1,8),
              "cycle_interval":(3,20)}
    if op=="studio.setting":
        name=cmd["name"]
        if type(name) is not str or name not in settings:raise ValueError("setting")
        return f"STUDIO_SETTING {name} {finite('value',*settings[name])}"
    if op=="studio.camera":
        if cmd["mode"] not in ("fit","face"):raise ValueError("camera")
        return "STUDIO_CAMERA "+cmd["mode"]
    if op=="studio.zoom":return "STUDIO_ZOOM "+finite("value",.3,3)
    if op=="studio.pan":
        return "STUDIO_PAN "+finite("x",-10000,10000)+" "+finite("y",-10000,10000)
    if op in ("studio.preset","rig.store"):
        if cmd["action"] not in ("save","load"):raise ValueError("action")
        return ("STUDIO_PRESET" if op=="studio.preset" else "RIG_STORE")+" "+cmd["action"]
    if op=="studio.capture":
        if cmd["kind"]!="viewport":raise ValueError("capture")
        return "STUDIO_CAPTURE viewport"
    if op=="param.set":return f"PARAM_SET {integer('index')} {finite('value',-1000,1000)}"
    if op=="param.clear":return f"PARAM_CLEAR {integer('index')}"
    if op=="part.opacity":return f"PART_OPACITY {integer('index')} {finite('value',0,1)}"
    if op=="rig.vertex":return f"RIG_VERTEX {integer('index')}"
    xpbd={"gravity":(-80,80),"wind":(-60,60),"damping":(0,1),
          "edge_compliance":(0,.0002),"collision":(0,.15),"mass":(.1,10),
          "contact_compliance":(0,.001),"iterations":(1,24),
          "step":(1/240,1/60),"pin":(0,100),"substeps":(1,8),
          "pose":(0,.02),"transfer":(0,1),"bending":(0,.002),
          "surface":(0,10),"anchors":(1,8)}
    if op=="xpbd.read":
        name=cmd["name"]
        if type(name) is not str or name not in xpbd:raise ValueError("unknown XPBD field")
        return "XPBD_READ "+name
    if op=="xpbd.set":
        name=cmd["name"]
        if type(name) is not str or name not in xpbd:raise ValueError("xpbd name")
        value=cmd["value"]
        if name in ("iterations","pin","substeps","anchors"):
            if type(value) is not int or not xpbd[name][0]<=value<=xpbd[name][1]:
                raise ValueError("xpbd integer")
            return f"XPBD_SET {name} {value}"
        return f"XPBD_SET {name} {finite('value',*xpbd[name])}"
    if op=="xpbd.select":
        a,b=integer("first"),cmd["second"]
        if type(b) is not int or not -1<=b<=2047 or b==a:
            raise ValueError("second mesh")
        joint=boolean("joint")
        if joint and b<0:raise ValueError("joint needs second mesh")
        return f"XPBD_SELECT {a} {b} {joint}"
    if op in ("layer.component.inspect","layer.component.adopt"):
        return ("LAYER_COMPONENT_INSPECT" if op.endswith("inspect") else "LAYER_COMPONENT_ADOPT")+f" {integer('component')}"
    if op=="layer.topology":return "LAYER_TOPOLOGY"
    if op=="layer.topology.scan":return "LAYER_TOPOLOGY_SCAN"
    if op in ("rig.history","rig.undo","rig.redo"):
        gen=cmd["generation"]
        if type(gen) is not int or not 0<gen<2**63:
            raise ValueError("invalid generation")
        if op=="rig.history":
            return f"RIG_HISTORY {gen}"
        revision=cmd["revision"]
        if type(revision) is not int or not 0<=revision<2**63:
            raise ValueError("invalid revision")
        return f"RIG_{'UNDO' if op=='rig.undo' else 'REDO'} {gen} {revision}"
    if op in ("rig.selection","layer.inspect","rig.select.at","layer.select"):
        generation=cmd["generation"]
        if type(generation) is not int or not 0<generation<2**63:
            raise ValueError("invalid model generation")
        if op=="rig.selection":return f"RIG_SELECTION {generation}"
        if op=="layer.inspect":return f"LAYER_INSPECT {generation}"
        revision=cmd["revision"]
        if type(revision) is not int or not 0<=revision<2**63:
            raise ValueError("invalid Rig revision")
        drawable=integer("drawable")
        return ("RIG_SELECT_AT" if op=="rig.select.at" else "LAYER_SELECT")+f" {generation} {revision} {drawable}"
    if op in ("layer.cursor","layer.vertex.select","layer.triangle.select",
              "layer.overlay","layer.isolate"):
        generation=cmd["generation"]
        if type(generation) is not int or not 0<generation<2**63:
            raise ValueError("invalid layer generation")
        if op=="layer.cursor":return f"LAYER_CURSOR {generation}"
        revision=cmd["revision"]
        if type(revision) is not int or not 0<=revision<2**63:
            raise ValueError("invalid layer revision")
        if op in ("layer.vertex.select","layer.triangle.select"):
            index=integer("index")
            return ("LAYER_VERTEX_SELECT" if op=="layer.vertex.select"
                    else "LAYER_TRIANGLE_SELECT")+f" {generation} {revision} {index}"
        if type(cmd["enabled"]) is not bool:raise ValueError("invalid layer view flag")
        return ("LAYER_OVERLAY" if op=="layer.overlay"
                else "LAYER_ISOLATE")+f" {generation} {revision} {int(cmd['enabled'])}"
    if op=="layer.physics.point_by_id":
        generation=cmd["generation"]
        if type(generation) is not int or not 0<generation<2**63:
            raise ValueError("invalid generation")
        id=integer("id")
        vertex_id=integer("vertex_id")
        if not 1<=id<=9999 or not 1<=vertex_id<2**32:
            raise ValueError("invalid physics identity")
        return f"GEOM_PHYSICS_POINT_BY_ID {generation} {id} {vertex_id}"
    if op=="layer.rig.link.inspect":
        generation=cmd["generation"]
        revision=cmd["revision"]
        index=integer("index")
        if type(generation) is not int or not 0<generation<2**63 or (
            type(revision) is not int or not 0<=revision<2**63) or index<0 or index>=16:
            raise ValueError("invalid Rig link inspect")
        return f"GEOM_RIG_LINK_INSPECT {generation} {revision} {index}"
    if op in ("layer.rig.links","layer.rig.link","layer.rig.unlink"):
        generation=cmd["generation"]
        if type(generation) is not int or not 0<generation<2**63:
            raise ValueError("invalid generation")
        if op=="layer.rig.links":return f"GEOM_RIG_LINKS {generation}"
        revision=cmd["revision"]
        if type(revision) is not int or not 0<=revision<2**63:
            raise ValueError("invalid revision")
        keys=("parent_id","parent_vertex_id","child_id","child_vertex_id")
        values=[]
        for k in keys:
            n=integer(k)
            if not 1<=n<2**32:raise ValueError("invalid link key")
            values.append(n)
        if values[0]>9999 or values[2]>9999 or values[0]==values[2]:
            raise ValueError("invalid cross-layer identity")
        base=f"{'GEOM_RIG_LINK' if op=='layer.rig.link' else 'GEOM_RIG_UNLINK'} {generation} {revision} "+' '.join(map(str,values))
        return base+(f" {finite('stiffness',0,1)}" if op=="layer.rig.link" else "")
    if op in ("layer.project.by_id","layer.active.id","layer.project.vertex_by_id"):
        generation=cmd["generation"]
        revision=cmd["revision"]
        if type(generation) is not int or not 0<generation<2**63:
            raise ValueError("invalid model generation")
        if type(revision) is not int or not 0<=revision<2**63:
            raise ValueError("invalid geometry revision")
        id=integer("id")
        if not 1<=id<=9999:raise ValueError("invalid layer id")
        if op=="layer.project.vertex_by_id":
            vertex_id=integer("vertex_id")
            if not 1<=vertex_id<2**32:raise ValueError("invalid vertex id")
            return f"GEOM_VERTEX_BY_ID {generation} {revision} {id} {vertex_id}"
        name="GEOM_LAYER_BY_ID" if op=="layer.project.by_id" else "GEOM_ACTIVE_ID"
        return f"{name} {generation} {revision} {id}"
    if op in ("layer.project.layer","layer.project.point","layer.project.face"):
        generation=cmd["generation"]
        if type(generation) is not int or not 0<generation<2**63:
            raise ValueError("invalid model generation")
        layer=integer("layer")
        if op=="layer.project.layer":return f"GEOM_LAYER {generation} {layer}"
        if op=="layer.project.point":
            return f"GEOM_POINT {generation} {layer} {integer('vertex')}"
        return f"GEOM_FACE {generation} {layer} {integer('triangle')}"
    if op in ("layer.model.compile","layer.model.open"):
        epoch=cmd["generation"]
        rev=cmd["revision"]
        if type(epoch) is not int or not 0<epoch<2**63:
            raise ValueError("invalid generation")
        if type(rev) is not int or not 0<=rev<2**63:
            raise ValueError("invalid revision")
        return f"{'GEOM_COMPILE' if op=='layer.model.compile' else 'GEOM_OPEN_COMPILED'} {epoch} {rev}"
    if op in ("layer.physics.status","layer.physics.point",
              "layer.physics.bind","layer.physics.stop"):
        gen=cmd["generation"]
        if type(gen) is not int or not 0<gen<2**63:raise ValueError("invalid generation")
        if op=="layer.physics.status":return f"GEOM_PHYSICS_STATUS {gen}"
        if op=="layer.physics.point":return f"GEOM_PHYSICS_POINT {gen} {integer('vertex')}"
        rev=cmd["revision"]
        if type(rev) is not int or not 0<=rev<2**63:
            raise ValueError("invalid revision")
        if op=="layer.physics.stop":return f"GEOM_PHYSICS_STOP {gen} {rev}"
        return f"GEOM_PHYSICS_BIND {gen} {rev} {integer('layer')}"
    if op.startswith("layer.") and op in (
        "layer.project.status","layer.project.save","layer.project.load",
        "layer.create","layer.copy.drawable","layer.delete","layer.duplicate","layer.reorder","layer.active","layer.point.add",
        "layer.point.move","layer.point.delete","layer.uv.set","layer.material.set","layer.rig.pin","layer.rig.weight","layer.rig.parent","layer.face.add",
        "layer.face.delete","layer.edit.undo","layer.edit.redo"):
        gen=cmd["generation"]
        if type(gen) is not int or not 0<gen<2**63:raise ValueError("invalid generation")
        prefix={
            "layer.project.status":"GEOM_STATUS","layer.project.save":"GEOM_SAVE",
            "layer.project.load":"GEOM_LOAD","layer.create":"GEOM_CREATE",
            "layer.delete":"GEOM_DELETE","layer.duplicate":"GEOM_DUPLICATE","layer.reorder":"GEOM_REORDER","layer.copy.drawable":"GEOM_COPY_DRAWABLE","layer.active":"GEOM_ACTIVE",
            "layer.point.add":"GEOM_VERTEX_ADD","layer.point.move":"GEOM_VERTEX_MOVE",
            "layer.point.delete":"GEOM_VERTEX_DELETE","layer.uv.set":"GEOM_UV_SET","layer.material.set":"GEOM_MATERIAL_SET","layer.rig.pin":"GEOM_PIN_SET","layer.rig.weight":"GEOM_RIG_WEIGHT","layer.rig.parent":"GEOM_RIG_PARENT","layer.face.add":"GEOM_TRIANGLE_ADD",
            "layer.face.delete":"GEOM_TRIANGLE_DELETE","layer.edit.undo":"GEOM_UNDO",
            "layer.edit.redo":"GEOM_REDO"}[op]
        if op=="layer.project.status":return f"{prefix} {gen}"
        rev=cmd.get("revision")
        if op=="layer.active":return f"{prefix} {gen} {integer('layer')}"
        if type(rev) is not int or not 0<=rev<2**63:raise ValueError("invalid geometry revision")
        if op in ("layer.uv.set","layer.material.set","layer.rig.pin",
                   "layer.rig.weight","layer.rig.parent"):
            start=f"{prefix} {gen} {rev} {integer('layer')}"
            if op=="layer.uv.set":
                return start+f" {integer('vertex')} {finite('u',-8,8)} {finite('v',-8,8)}"
            if op=="layer.rig.weight":
                return start+f" {integer('vertex')} {finite('weight',0,1)}"
            if op=="layer.rig.parent":
                parent=cmd["parent"]
                if type(parent) is not int or parent< -1 or parent>4096:
                    raise ValueError("invalid parent")
                return start+f" {integer('vertex')} {parent} {finite('stiffness',0,1)}"
            if op=="layer.rig.pin":
                vertex=cmd["vertex"]
                if type(vertex) is not int or vertex< -1 or vertex>4096:
                    raise ValueError("invalid anchor")
                return start+f" {vertex}"
            return start+" "+" ".join(str(finite(k,0,1)) for k in ("r","g","b","opacity"))
        params=[]
        for k in (("layer","id") if op=="layer.duplicate" else
                  ("layer","target") if op=="layer.reorder" else
                  ("drawable","id","layer","vertex","triangle","a","b","c")):
            if k in cmd:params.append(str(integer(k)))
        for k in ("x","y"):
            if k in cmd:params.append(str(finite(k,-10000,10000)))
        return f"{prefix} {gen} {rev}"+(" "+" ".join(params) if params else "")
    if op=="rig.select":return f"RIG_SELECT {integer('drawable')}"
    if op=="rig.anchor":
        v=integer("vertex")
        if type(cmd["enabled"]) is not bool:raise ValueError("enabled must be bool")
        return f"RIG_ANCHOR {v} {int(cmd['enabled'])}"
    if op=="rig.beam":
        a,b=integer("a"),integer("b")
        if a==b or type(cmd["enabled"]) is not bool:raise ValueError("invalid beam")
        return f"RIG_BEAM {a} {b} {int(cmd['enabled'])}"
    if op=="rig.cross.inspect":return "RIG_CROSS_INSPECT"
    if op=="rig.cross":
        parent,child,target=integer("parent"),integer("child"),integer("target")
        if type(cmd["enabled"]) is not bool:raise ValueError("cross enabled bool")
        return f"RIG_CROSS {target} {parent} {child} {finite('stiffness',0,1)} {int(cmd['enabled'])}"
    if op=="rig.brush":
        return "RIG_BRUSH "+finite("radius",.00001,1000)+" "+finite("weight",0,1)
    if op=="rig.parent.inspect":
        return f"RIG_PARENT_INSPECT {integer('child')}"
    if op=="rig.parent":
        parent,child=integer("parent"),integer("child")
        if parent==child or type(cmd["enabled"]) is not bool:
            raise ValueError("parent edge invalid")
        return f"RIG_PARENT {parent} {child} {finite('stiffness',0,1)} {int(cmd['enabled'])}"
    if op=="rig.weight":
        return f"RIG_WEIGHT {integer('vertex')} {finite('weight',0,1)}"
    if op=="rig.weight.inspect":
        return f"RIG_WEIGHT_INSPECT {integer('vertex')}"
    if op=="layer.component.relation":
        first,second=integer("first"),integer("second")
        if first==second:raise ValueError("identical components")
        return f"LAYER_COMPONENT_RELATION {first} {second}"
    if op=="rig.apply":return "RIG_APPLY"
    if op=="rig.inspect":return "RIG_INSPECT"
    if op=="layer.scan":return "LAYER_SCAN"
    if op=="layer.suggest":return "LAYER_SUGGEST"
    if op=="layer.adopt":return "LAYER_ADOPT"
    if op=="layer.label":
        if type(cmd["kind"]) is not str or cmd["kind"] not in ("unknown","hair","skirt","strap"):
            raise ValueError("invalid manually confirmed layer role")
        return "LAYER_LABEL "+cmd["kind"]
    if op=="physics.set":
        return "PHYSICS_SET "+" ".join((finite("gravity",-80,80),finite("wind",-60,60),finite("damping",0,1)))
    if cmd["kind"] not in ("pull","release","shake","settle"):raise ValueError("invalid action")
    return f"ACTION {cmd['kind']} {finite('strength',0,1)}"
def _writer_claim(folder:Path)->int:
    """Exclusive cross-process byte-range lock, OS-released after a crash.

    The lock file is deliberately retained. Its presence is not ownership.
    """
    import sys
    path=folder/"writer.lock"
    try:
        fd=os.open(path,os.O_CREAT|os.O_RDWR,0o600)
        if os.fstat(fd).st_size==0:
            os.write(fd,bytes([0]))
        os.lseek(fd,0,os.SEEK_SET)
        if sys.platform=="win32":
            import msvcrt
            msvcrt.locking(fd,msvcrt.LK_NBLCK,1)
        else:
            import fcntl
            fcntl.flock(fd,fcntl.LOCK_EX|fcntl.LOCK_NB)
        return fd
    except OSError as exc:
        if "fd" in locals():os.close(fd)
        raise RuntimeError("Studio queue busy: writer claim unavailable") from exc

def _writer_release(fd:int)->None:
    import sys
    try:
        os.lseek(fd,0,os.SEEK_SET)
        if sys.platform=="win32":
            import msvcrt
            msvcrt.locking(fd,msvcrt.LK_UNLCK,1)
        else:
            import fcntl
            fcntl.flock(fd,fcntl.LOCK_UN)
    finally:
        os.close(fd)

class StudioUnknownOutcome(TimeoutError):
    """One V1 request outcome is uncertain: inspect by original local token."""
    def __init__(self, request_token: str):
        super().__init__("Studio request outcome unknown; use read-only recovery")
        self.request_token = request_token


class AgentBridge:
    def __init__(self,folder:Path,timeout:float=5):
        self.folder=Path(folder);self.timeout=timeout
    def call(self,command:dict)->dict:
        wire=validate_command(command)
        self.folder.mkdir(parents=True,exist_ok=True)
        request=self.folder/"request.txt";response=self.folder/"response.txt"
        writer_fd=_writer_claim(self.folder)
        token=secrets.token_hex(12)
        temporary=self.folder/("request-"+token+".tmp")
        intent=self.folder/"uncertain_outcome"
        acknowledged=False
        try:
            if (request.exists() or response.exists() or intent.exists() or
                    (self.folder/"response.tmp").exists()):
                raise RuntimeError("Studio queue not empty; recovery inspection required")
            # Durable local intent before request publication: a timeout is UNKNOWN_OUTCOME.
            with intent.open("x",encoding="ascii") as marker:
                marker.write(token+"\n")
                marker.flush()
                os.fsync(marker.fileno())
            temporary.write_text(token+" "+wire+"\n",encoding="ascii")
            os.replace(temporary,request)
            deadline=time.monotonic()+self.timeout
            while time.monotonic()<deadline:
                if response.exists():
                    try:
                        with response.open("rb") as handle:
                            frame=handle.read(513)
                        if len(frame)>512:
                            raise RuntimeError("Studio receipt exceeds fixed bound")
                        line=frame.decode("ascii")
                    except (OSError,UnicodeError):
                        line=""
                    if line.startswith(token+" ") and line.endswith("\n") and line.count("\n")==1:
                        fields=line[:-1].removesuffix("\r").split(" ",2)
                        if len(fields)!=3 or fields[1] not in ("OK","ERR") or not fields[2]:
                            raise RuntimeError("Invalid Studio receipt frame")
                        acknowledged=True
                        return {"ok":fields[1]=="OK","receipt":fields[2]}
                time.sleep(.015)
            raise StudioUnknownOutcome(token)
        finally:
            temporary.unlink(missing_ok=True)
            if request.exists():
                try:
                    line=request.read_text(encoding="ascii")
                    if line.startswith(token+" "):
                        request.unlink(missing_ok=True)
                except (OSError,UnicodeError):
                    pass
            if response.exists():
                try:
                    line=response.read_text(encoding="ascii")
                    if line.startswith(token+" "):
                        response.unlink(missing_ok=True)
                except (OSError,UnicodeError):
                    pass
            # Keep the intent on crash/timeout or incomplete cleanup: no blind replay.
            if acknowledged and not request.exists() and not response.exists():
                intent.unlink(missing_ok=True)
            _writer_release(writer_fd)

    def recover(self, original_token: str) -> dict:
        """Read-only original-request metadata from real Studio IPC.

        UNKNOWN does not mean the operation was not applied. The volatile
        store is not crash-durable; no replay or queue-unquarantine here.
        """
        if (type(original_token) is not str or
            re.fullmatch(r"[0-9a-f]{24}", original_token, re.ASCII) is None):
            raise ValueError("invalid recovery token")
        recovery=self.folder/"recovery"
        recovery.mkdir(parents=True, exist_ok=True)
        if recovery.is_symlink():
            raise RuntimeError("Recovery directory cannot be a symlink")
        claim=_writer_claim(recovery)
        token=secrets.token_hex(12)
        request=recovery/"request.txt"
        response=recovery/"response.txt"
        temp=recovery/("request-"+token+".tmp")
        try:
            if request.exists() or response.exists() or (recovery/"response.tmp").exists():
                raise RuntimeError("Recovery lane busy")
            temp.write_text(token+" RECOVER "+original_token+"\n",encoding="ascii")
            os.replace(temp,request)
            deadline=time.monotonic()+self.timeout
            while time.monotonic()<deadline:
                if response.is_file():
                    with response.open("rb") as f:
                        raw=f.read(96)
                    if len(raw)>95:
                        raise RuntimeError("Recovery response over budget")
                    try:line=raw.decode("ascii")
                    except UnicodeError as exc:
                        raise RuntimeError("Malformed recovery frame") from exc
                    if line==token+" UNKNOWN\n":
                        return {"state":"UNKNOWN"}
                    if line==token+" FOUND OK\n":
                        return {"state":"FOUND","ok":True}
                    if line==token+" FOUND ERR\n":
                        return {"state":"FOUND","ok":False}
                    raise RuntimeError("Recovery response token or result mismatch")
                time.sleep(.015)
            raise TimeoutError("Read-only Studio recovery did not acknowledge")
        finally:
            temp.unlink(missing_ok=True)
            for path in (request,response):
                try:
                    if path.is_file() and path.stat().st_size<96:
                        if path.read_text(encoding="ascii").startswith(token+" "):
                            path.unlink()
                except (OSError, UnicodeError):
                    pass
            _writer_release(claim)

    def reconcile(self, original_token: str) -> dict:
        """Resolve local V1 ACK uncertainty only from a matching Native receipt.

        Does not retry the original command. UNKNOWN leaves quarantine intact.
        """
        if type(original_token) is not str or re.fullmatch(
                r"[0-9a-f]{24}",original_token,re.ASCII) is None:
            raise ValueError("invalid recovery token")
        claim=_writer_claim(self.folder)
        marker=self.folder/"uncertain_outcome"
        try:
            if any((self.folder/name).exists() for name in
                   ("request.txt","response.txt","response.tmp")):
                raise RuntimeError("Cannot reconcile while mutation ACK is pending")
            if not marker.is_file() or marker.is_symlink():
                raise RuntimeError("Missing original uncertain intent")
            with marker.open("rb") as f:
                raw=f.read(64)
            # Windows text-mode creation writes CRLF; accept only that or LF.
            expected=original_token.encode("ascii")
            if raw not in (expected+b"\n",expected+b"\r\n"):
                raise RuntimeError("Original request mismatch")
            result=self.recover(original_token)
            if (type(result) is not dict or result.get("state")!="FOUND" or
                    type(result.get("ok")) is not bool):
                raise RuntimeError("Outcome UNKNOWN; mutation lane stays quarantined")
            if any((self.folder/name).exists() for name in
                   ("request.txt","response.txt","response.tmp")):
                raise RuntimeError("Mutation response changed during reconciliation")
            if marker.read_bytes()!=raw:
                raise RuntimeError("Original uncertain intent changed")
            marker.unlink()
            return {"state":"RECONCILED","ok":result["ok"]}
        finally:
            _writer_release(claim)
