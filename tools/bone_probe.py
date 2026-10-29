"""Capture validated reference-bone names/parents from live character assets, read-only."""
from functools import lru_cache
import json
import struct
from game_probe import Game

g=Game()
@lru_cache(maxsize=None)
def layout(cls):
    chain, fields=[],{}
    while cls:
        chain.append(g.object_name(cls))
        for key,value in g.properties(cls).items():
            fields.setdefault(key,value)
        cls=g.ptr(cls+0x40)
    return chain,fields

def pointers(address):
    ptr,count,cap=struct.unpack('<Qii',g.read(address,16))
    if not 0<=count<=cap<=100000:
        raise ValueError('Invalid array')
    return [p[0] for p in struct.iter_unpack('<Q',g.read(ptr,count*8)) if p[0]] if count else []

def reference_bones(obj):
    memory=g.read(obj,0x300)
    for off in range(0x28,0x2f0,8):
        ptr,count,cap=struct.unpack_from('<Qii',memory,off)
        if not 20<=count<=1024 or not count<=cap<=4096 or not 0x10000<ptr<0x7fffffffffff:
            continue
        for stride in [12,16]:
            try:
                data=g.read(ptr,count*stride)
                bones=[]
                for i in range(count):
                    idx,num,parent=struct.unpack_from('<IIi',data,i*stride)
                    if num>100 or parent>=i or parent<(-1):
                        raise ValueError('Invalid reference hierarchy')
                    name=g.name(idx)
                    if i==0 and name.lower() not in ['root','hips','pelvis','bip01']:
                        raise ValueError('Not reference bone info')
                    bones.append(dict(name=name+(f'_{num-1}' if num else ''),parent=parent))
                return bones
            except (ValueError,OverflowError,OSError):
                pass
    return None

try:
    world=g.ptr(g.base+g.off['GWorld']); _,wp=layout(g.ptr(world+16))
    assets={}
    for level in pointers(world+wp['Levels']):
        for actor in pointers(level+0x98):
            try:
                chain,props=layout(g.ptr(actor+16))
                if 'ReadyOrNotCharacter' not in chain:
                    continue
                mesh=g.ptr(actor+props['Mesh'])
                if not mesh:
                    continue
                _,mp=layout(g.ptr(mesh+16)); asset=g.ptr(mesh+mp['SkinnedAsset'])
                if not asset or asset in assets:
                    continue
                _,ap=layout(g.ptr(asset+16)); skeleton=g.ptr(asset+ap['Skeleton'])
                bones=reference_bones(asset) or (reference_bones(skeleton) if skeleton else None)
                if bones:
                    assets[asset]=dict(asset=g.object_name(asset),bones=bones)
            except (ValueError,KeyError,OverflowError,OSError):
                continue
    print(json.dumps(dict(world=g.object_name(world),assets=list(assets.values())),indent=2))
finally:
    g.close()
