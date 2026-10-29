"""Read-only weapon discovery and evidence-state snapshot for ESP regression checks."""
import argparse
from functools import lru_cache
import json
import struct
from game_probe import Game


def snapshot():
    g = Game()
    try:
        def pointers(address):
            ptr, count, cap = struct.unpack('<Qii', g.read(address, 16))
            if not 0 <= count <= cap <= 100000:
                raise ValueError('Invalid actor array')
            return [p[0] for p in struct.iter_unpack('<Q', g.read(ptr, count*8)) if p[0]] if count else []

        @lru_cache(maxsize=None)
        def layout(cls):
            chain, fields = [], {}
            while cls:
                chain.append(g.object_name(cls))
                for name, off in g.properties(cls).items():
                    fields.setdefault(name, off)
                cls = g.ptr(cls + 0x40)
            return chain, fields

        world = g.ptr(g.base + g.off['GWorld'])
        _, wp = layout(g.ptr(world + 16))
        state = g.ptr(world + wp['GameState'])
        _, sp = layout(g.ptr(state + 16))
        registry = set(pointers(state + sp['AllItems']))
        actors = set(registry)
        for level in pointers(world + wp['Levels']):
            actors.update(pointers(level + 0x98))
        records = []
        for actor in actors:
            try:
                chain, props = layout(g.ptr(actor+16))
                if 'BaseItem' not in chain:
                    continue
                record = dict(actor=hex(actor), name=g.object_name(actor), classes=chain[:-2], inAllItems=actor in registry)
                for name in ['bInInventory', 'ItemCategory', 'ItemClass', 'bHidden']:
                    if name in props:
                        record[name] = g.read(actor + props[name], 1)[0]
                for name in ['PreviousOwner', 'Owner', 'EvidenceComponent', 'ItemMesh']:
                    if name not in props:
                        continue
                    ptr = g.ptr(actor + props[name])
                    record[name] = g.object_name(ptr) if ptr else None
                    if ptr and name in ['PreviousOwner','Owner']:
                        record[name+'Classes'] = layout(g.ptr(ptr+16))[0]
                    if ptr and name == 'EvidenceComponent':
                        _, ep = layout(g.ptr(ptr+16))
                        for flag in ['bIsBeingCollected','bEvidenceExtracted','bMarkAsEvidenceWhenNoOwner','EvidenceState']:
                            if flag in ep:
                                record[flag] = g.read(ptr+ep[flag],1)[0]
                record['legacyIncluded'] = (actor in registry and not record.get('bInInventory', True)
                    and 'CyberneticsSuspect_V2_C' in record.get('PreviousOwnerClasses', []))
                record['eligibleEvidenceWeapon'] = ('BaseWeapon' in chain and not record.get('bInInventory', True)
                    and record.get('ItemMesh') is not None and record.get('Owner') is None
                    and not (record.get('bEvidenceExtracted',0)&1) and record.get('EvidenceState',0) not in [1,2]
                    and 'CyberneticsSwat_V2_C' not in record.get('PreviousOwnerClasses',[])
                    and ('CyberneticsSuspect_V2_C' in record.get('PreviousOwnerClasses',[]) or record.get('bMarkAsEvidenceWhenNoOwner',False)))
                records.append(record)
            except (ValueError, UnicodeError):
                continue
        return dict(world=g.object_name(world), items=records)
    finally:
        g.close()


if __name__ == '__main__':
    args = argparse.ArgumentParser()
    args.add_argument('--assert-handguns',action='store_true')
    options=args.parse_args()
    data=snapshot()
    print(json.dumps(data,indent=2))
    if options.assert_handguns:
        handguns=[i for i in data['items'] if 'BaseWeapon' in i['classes']
            and i.get('ItemClass') == 4 and not i.get('bInInventory') and not (i.get('bEvidenceExtracted',0)&1)]
        assert handguns, 'No live dropped handgun to verify'
        assert all(i['eligibleEvidenceWeapon'] for i in handguns), 'Dropped handgun has an unexpected non-evidence/owned state'
