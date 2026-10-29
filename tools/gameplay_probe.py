"""Read-only snapshots of gameplay controls and nearby NPC/door state."""
from functools import lru_cache
import json,struct
from game_probe import Game

def snapshot():
    g=Game()
    try:
        @lru_cache(maxsize=None)
        def layout(cls):
            chain,fields=[],{}
            while cls:
                chain.append(g.object_name(cls))
                for k,v in g.properties(cls).items(): fields.setdefault(k,v)
                cls=g.ptr(cls+0x40)
            return chain,fields
        def array(addr):
            p,n,cap=struct.unpack('<Qii',g.read(addr,16))
            if not 0<=n<=cap<=100000: raise ValueError('Invalid array')
            return [v[0] for v in struct.iter_unpack('<Q',g.read(p,n*8)) if v[0]] if n else []
        def pointer(obj,field): return g.ptr(obj+layout(g.ptr(obj+16))[1][field])
        def fields(obj,names):
            props=layout(g.ptr(obj+16))[1]
            return {n:struct.unpack('<f',g.read(obj+props[n],4))[0] for n in names if n in props}
        w=g.ptr(g.base+g.off['GWorld']);gi=pointer(w,'OwningGameInstance')
        pc=pointer(array(gi+layout(g.ptr(gi+16))[1]['LocalPlayers'])[0],'PlayerController');p=pointer(pc,'Pawn')
        pp=layout(g.ptr(p+16))[1]
        player=fields(p,['RunSpeed','SpeedModifier_Crouch','SlowDownSpeedMultiplier','WalkSpeedMultiplier','MaxRunSpeedPercent'])
        player['GodMode']=bool(g.read(p+pp['bGodMode'],1)[0]&1)
        player['CanBeDamaged']=bool(g.read(p+pp['bCanBeDamaged'],1)[0]&4)
        hp=pointer(p,'CharacterHealth');hprops=layout(g.ptr(hp+16))[1]
        player['Health']=fields(hp,['Resource','MaxResource'])
        player['UnlimitedHealth']=bool(g.read(hp+hprops['bUnlimited'],1)[0]&1)
        player['LimbDamageMultipliers']=[struct.unpack('<f',g.read(hp+hprops[n]+24,4))[0] for n in ['RightLeg','LeftLeg','RightArm','LeftArm','Head']]
        movement=pointer(p,'CharacterMovement')
        player['Movement']=fields(movement,['MaxWalkSpeed','MaxWalkSpeedCrouched'])
        inv=pointer(p,'InventoryComp');weapons=[]
        for item in array(inv+layout(g.ptr(inv+16))[1]['InventoryItems']):
            chain,props=layout(g.ptr(item+16))
            if 'BaseWeapon' in chain:
                info=dict(name=g.object_name(item),**fields(item,['RecoilMultiplierPitch','RecoilMultiplierYaw','RecoilFireStrength']))
                if 'bInfiniteAmmo' in props:info['InfiniteAmmo']=bool(g.read(item+props['bInfiniteAmmo'],1)[0]&1)
                weapons.append(info)
        doors=[];npcs=[]
        for level in array(w+layout(g.ptr(w+16))[1]['Levels']):
            for a in array(level+0x98):
                try:
                    chain,props=layout(g.ptr(a+16))
                    if 'Door' in chain:doors.append(dict(id=hex(a),locked=bool(g.read(a+props['bLocked'],1)[0]&1)))
                    if 'CyberneticsSuspect_V2_C' in chain or 'CyberneticsSwat_V2_C' in chain:
                        npcs.append(dict(id=hex(a),kind='suspect' if 'CyberneticsSuspect_V2_C' in chain else 'ally',god=bool(g.read(a+props['bGodMode'],1)[0]&1),surrendered=bool(g.read(a+props['bSurrendered'],1)[0])))
                except (ValueError,KeyError):pass
        return dict(world=g.object_name(w),player=player,weapons=weapons,doors=doors,npcs=npcs)
    finally:g.close()

if __name__=='__main__':print(json.dumps(snapshot(),indent=2))
