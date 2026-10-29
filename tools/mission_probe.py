"""Inspect active Ready or Not mission targets without changing game state."""
from functools import lru_cache
import json
import struct
from game_probe import Game

g = Game()


def array(address, item_size=8, maximum=100000):
    pointer, count, capacity = struct.unpack('<Qii', g.read(address, 16))
    if not (0 <= count <= capacity <= maximum):
        raise ValueError('Invalid mission array')
    return g.read(pointer, count * item_size) if count else b''


def pointers(address):
    data = array(address)
    return [v[0] for v in struct.iter_unpack('<Q', data) if v[0]]


def string(address):
    data = array(address, 2, 4096)
    return data.decode('utf-16-le', errors='replace').rstrip('\0')


def text(address):
    data = g.ptr(address)
    return string(data + 0x28) if data else ''


@lru_cache(maxsize=None)
def properties(cls):
    chain = []
    result = {}
    while cls:
        chain.append(g.object_name(cls))
        for key, value in g.properties(cls).items():
            result.setdefault(key, value)
        cls = g.ptr(cls + 0x40)
    return chain, result


def info(actor):
    chain, props = properties(g.ptr(actor + 16))
    record = {'address': hex(actor), 'object': g.object_name(actor), 'class': chain[0]}
    if 'Tags' in props:
        record['tags'] = [g.name(i) for i, n in struct.iter_unpack('<II', array(actor + props['Tags']))]
    for key in ['ObjectiveName', 'ObjectiveDescription', 'ReportableName']:
        if key in props:
            try:
                record[key] = text(actor + props[key])
            except ValueError:
                record[key] = '<unreadable text>'
    for key in ['SuspectTag', 'CivilianTag', 'EvidenceTag', 'ReportTag']:
        if key in props:
            record[key] = g.name(struct.unpack('<I', g.read(actor + props[key], 4))[0])
    for key in ['ObjectiveStatus', 'BombState', 'bRequireArrest', 'bHiddenObjective', 'bReportableEnabled', 'bHasBeenReported']:
        if key in props:
            record[key] = g.read(actor + props[key], 1)[0]
    for key in ['suspect', 'civilian', 'TiedToActor']:
        if key in props:
            pointer = g.ptr(actor + props[key])
            record[key] = hex(pointer) if pointer else None
    return record, chain, props


try:
    world = g.ptr(g.base + g.off['GWorld'])
    _, wp = properties(g.ptr(world + 16))
    state = g.ptr(world + wp['GameState'])
    _, sp = properties(g.ptr(state + 16))
    objectives = pointers(state + sp['MissionObjectives'])
    print(json.dumps({'world': g.object_name(world), 'objectives': len(objectives)}, ensure_ascii=True))
    for objective in objectives:
        print(json.dumps({'objective': info(objective)[0]}, ensure_ascii=True))
    for actor in pointers(state + sp['AllReportableActors']):
        print(json.dumps({'reportable': info(actor)[0]}, ensure_ascii=True))
    matched = 0
    for level in pointers(world + wp['Levels']):
        for actor in pointers(level + 0x98):
            try:
                chain, props = properties(g.ptr(actor + 16))
                tags = [g.name(i) for i, n in struct.iter_unpack('<II', array(actor + props['Tags']))] if 'Tags' in props else []
                if 'BombActor' in chain or ('ReadyOrNotCharacter' in chain and tags):
                    record = info(actor)[0]
                    if 'SpeechCharacterName' in props:
                        record['speechName'] = string(actor + props['SpeechCharacterName'])
                    print(json.dumps({'candidate': record}, ensure_ascii=True))
                    matched += 1
            except (ValueError, UnicodeError):
                continue
    print(json.dumps({'candidates': matched}))
finally:
    g.close()
