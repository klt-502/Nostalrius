"""Strict readers for this branch's direct-ID items.srv, OTBM v1/v2 and 7.72 DAT.

No client binaries, downloaded maps, runtime server state or third-party dependencies.
"""
from dataclasses import dataclass
from pathlib import Path
import re
import struct


class FormatError(ValueError):
    pass


class Reader:
    def __init__(self, data):
        self.data, self.at = data, 0

    def take(self, n):
        if n < 0 or self.at + n > len(self.data):
            raise FormatError(f'truncated binary at byte {self.at}, need {n}')
        out = self.data[self.at:self.at+n]
        self.at += n
        return out

    def uint(self, size):
        return int.from_bytes(self.take(size), 'little')

    def string(self):
        return self.take(self.uint(2)).decode('utf-8')

    def end(self):
        if self.at != len(self.data):
            raise FormatError(f'unexpected trailing bytes at {self.at}')


@dataclass(frozen=True)
class ServerItem:
    name: str
    flags: frozenset
    speed: int
    client_id: int

    @property
    def layer(self):
        return next((i for i, name in enumerate(('Bank', 'Clip', 'Bottom', 'Top')) if name in self.flags), 4)


def read_srv(path):
    # Audit the format actually consumed by Items::loadItems; no OTB remapping exists here.
    text = re.sub(r'#[^\n]*', '', Path(path).read_text(encoding='latin1'))
    result = {}
    blocks = re.split(r'(?m)^TypeID\s*=\s*(\d+)\s*$', text)
    if blocks[0].strip() or len(blocks) < 3:
        raise FormatError('expected items.srv TypeID records')
    for i in range(1, len(blocks), 2):
        item_id, body = int(blocks[i]), blocks[i+1]
        if item_id in result or not 1 <= item_id <= 65535:
            raise FormatError(f'duplicate/invalid item ID {item_id}')
        name = re.search(r'(?m)^Name\s*=\s*"([^"\n]*)"', body)
        flags = re.search(r'(?m)^Flags\s*=\s*\{([^}]*)\}', body)
        flags = frozenset(s.strip() for s in flags[1].split(',') if s.strip()) if flags else frozenset()
        speed = re.search(r'\bWaypoints\s*=\s*(\d+)', body)
        disguise = re.search(r'\bDisguiseTarget\s*=\s*(\d+)', body, re.I)
        if 'Disguise' in flags and not disguise:
            raise FormatError(f'missing disguise target for {item_id}')
        result[item_id] = ServerItem(name[1] if name else '', flags, int(speed[1]) if speed else 0,
                                    int(disguise[1]) if 'Disguise' in flags else item_id)
    return result


@dataclass(frozen=True)
class ClientItem:
    flags: frozenset
    speed: int
    color: int


def read_dat(path):
    """7.55–7.72: attr23=floor change, patternZ present, 16-bit sprite indices."""
    r = Reader(Path(path).read_bytes())
    signature = r.uint(4)
    counts = [r.uint(2) for _ in range(4)]
    if counts[0] < 100:
        raise FormatError('DAT item limit below 100')
    items = {}
    payload_sizes = {0: 2, 8: 2, 9: 2, 21: 4, 24: 4, 25: 2, 28: 2, 29: 2, 32: 2}
    for category, count in enumerate(counts):
        for item_id in range(100 if category == 0 else 1, count+1):
            flags, speed, color = set(), 0, 0
            for _ in range(256):
                attr = r.uint(1)
                if attr == 255:
                    break
                if attr > 32 or attr in flags:
                    raise FormatError(f'unknown/duplicate DAT attribute {attr}, category {category}, ID {item_id}')
                flags.add(attr)
                value = r.uint(payload_sizes.get(attr, 0))
                if attr == 0: speed = value
                if attr == 28:
                    if value > 215: raise FormatError(f'unsupported minimap palette index {value}')
                    color = value
            else:
                raise FormatError('unterminated DAT attributes')
            width, height = r.uint(1), r.uint(1)
            if width > 1 or height > 1: r.take(1)  # exact render size
            dims = [width, height] + [r.uint(1) for _ in range(5)] # layers, patterns X/Y/Z, frames
            sprites = 1
            for dim in dims:
                if not 1 <= dim <= 64: raise FormatError(f'invalid DAT dimension {dim}')
                sprites *= dim
            if sprites > 4096: raise FormatError('DAT sprite count exceeds 4096')
            r.take(sprites*2)
            if category == 0: items[item_id] = ClientItem(frozenset(flags), speed, color)
    r.end()  # Parse ALL categories; wrong versions/layouts must not pass an item-only prefix check.
    return items, dict(signature=f'{signature:08x}', item_limit=counts[0], creature_count=counts[1],
                       effect_count=counts[2], missile_count=counts[3])


# Item::CreateItem(PropStream&) maps PvP field IDs to these persistent types.
FIELD_IDS = {2118: 2123, 2119: 2124, 2120: 2125, 2121: 2127, 2122: 2126}


def item_id(props):
    r = Reader(props)
    item = r.uint(2)
    sizes = {4:2, 5:2, 8:5, 10:2, 12:1, 14:1, 15:1, 16:4, 17:1, 18:4,
             20:4, 21:4, 22:2, 23:2, 24:2, 25:2, 26:2, 27:2, 28:2,
             33:4, 34:4, 35:4, 36:4, 37:1}
    while r.at < len(props):
        attr = r.uint(1)
        if attr == 0: r.end(); break
        if attr in (6, 7, 19, 30, 31, 32): r.take(r.uint(2))
        elif attr in sizes: r.take(sizes[attr])
        else: raise FormatError(f'unknown OTBM item attribute {attr}')
    return FIELD_IDS.get(item, item)


@dataclass
class Node:
    kind: int
    props: bytearray
    area: tuple | None
    items: list
    children_started: bool = False


@dataclass(frozen=True)
class Tile:
    x: int
    y: int
    z: int
    house_id: int
    flags: int
    items: tuple


def tiles(path, metadata):
    """Stream complete tiles. Nested container contents are validated, never drawn as ground items."""
    raw = Path(path).read_bytes()
    if raw[:4] not in (b'OTBM', b'\0\0\0\0'): raise FormatError('invalid OTBM signature')
    special, stack, at, roots = re.compile(b'[\xfd-\xff]'), [], 4, 0
    metadata.update(towns=[], waypoints=[])
    while at < len(raw):
        m = special.search(raw, at)
        if not m: raise FormatError('unterminated OTBM node')
        end = m.start()
        if stack:
            if stack[-1].children_started and end != at: raise FormatError('properties after child nodes')
            stack[-1].props.extend(raw[at:end])
        elif end != at: raise FormatError('bytes outside OTBM root')
        token, at = raw[end], end+1
        if token == 253:
            if not stack or at == len(raw) or stack[-1].children_started: raise FormatError('bad OTBM escape')
            stack[-1].props.append(raw[at]); at += 1
        elif token == 254:
            if at == len(raw) or len(stack) >= 64: raise FormatError('truncated/deep OTBM')
            kind, at = raw[at], at+1
            if not stack:
                roots += 1
                if roots != 1 or kind != 0: raise FormatError('expected one OTBM root')
                area = None
            else:
                parent = stack[-1]
                allowed = {0:{2}, 2:{4,12,15}, 4:{5,14}, 5:{6}, 14:{6}, 6:{6}, 12:{13}, 15:{16}}
                if kind not in allowed.get(parent.kind, set()): raise FormatError(f'unsupported node {kind} under {parent.kind}')
                parent.children_started = True
                if parent.kind == 4:
                    r = Reader(parent.props); area = (r.uint(2), r.uint(2), r.uint(1)); r.end()
                else: area = parent.area
            stack.append(Node(kind, bytearray(), area, []))
        else:
            if not stack: raise FormatError('unmatched OTBM end')
            n = stack.pop(); r = Reader(n.props)
            if n.kind == 0:
                version, w, h, major, minor = (r.uint(k) for k in (4,2,2,4,4)); r.end()
                if version not in (1,2): raise FormatError(f'unsupported OTBM version {version}')
                metadata.update(version=version, width=w, height=h, items_major=major, items_minor=minor)
            elif n.kind == 2:
                while r.at < len(r.data):
                    attr = r.uint(1)
                    if attr not in (1,11,13): raise FormatError(f'unknown map attribute {attr}')
                    value = r.string()
                    if attr in (11,13): metadata['spawn_file' if attr == 11 else 'house_file'] = value
            elif n.kind in (5,14):
                if not n.area: raise FormatError('tile without area')
                x, y = n.area[0]+r.uint(1), n.area[1]+r.uint(1)
                house = r.uint(4) if n.kind == 14 else 0
                flags, ids = 0, []
                while r.at < len(r.data):
                    attr = r.uint(1)
                    if attr == 3: flags = r.uint(4)
                    elif attr == 9:
                        value = r.uint(2); ids.append(FIELD_IDS.get(value, value))
                    else: raise FormatError(f'unknown tile attribute {attr}')
                if not (0 <= x <= 65535 and 0 <= y <= 65535 and 0 <= n.area[2] <= 15):
                    raise FormatError('tile coordinates outside protocol range')
                yield Tile(x, y, n.area[2], house, flags, tuple(ids+n.items))
            elif n.kind == 6:
                value = item_id(n.props)
                if stack and stack[-1].kind in (5,14): stack[-1].items.append(value)
            elif n.kind in (13,16):
                identity = r.uint(4) if n.kind == 13 else None
                name = r.string(); position = [r.uint(k) for k in (2,2,1)]; r.end()
                metadata['towns' if n.kind == 13 else 'waypoints'].append(dict(id=identity,name=name,position=position))
    if stack or roots != 1: raise FormatError('unclosed/absent OTBM root')


def render_tile(tile, server, client):
    records, missing = [], set()
    for item in tile.items:
        s = server.get(item)
        if not s: missing.add(f'server:{item}'); continue
        if tile.house_id and 'Unmove' not in s.flags: continue # native IOMap removes movable house items
        c = client.get(s.client_id)
        if not c: missing.add(f'client:{s.client_id} (server:{item})'); continue
        records.append((s,c))
    # Simulate Tile::internalAddThing + ProtocolGame::GetTileDescription's stationary stack.
    ground = next(((s,c) for s,c in records if s.layer == 0), None)
    top = sorted((p for p in records if 0 < p[0].layer < 4), key=lambda p:p[0].layer)
    down = list(reversed([p for p in records if p[0].layer == 4]))
    ordered = (([ground] if ground else []) + top + down)[:10]
    color = 0
    for s,c in ordered:
        if not c.flags.intersection((0,1,2,3)): break
        if c.color: color = c.color
    blocked = not ground or bool(missing)
    for s,c in records:
        blocked |= bool(s.flags.intersection(('Unpass','Avoid')) or c.flags.intersection((12,15,23)))
    speed = ground[1].speed if ground else 0
    # 250 is unknown, 255 blocked. Keep raw friction (not OTMM's speed/10).
    cost = 255 if blocked else max(1,min(249,speed))
    return color,cost,missing
