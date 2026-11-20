#!/usr/bin/env python3
"""Print a source house identity for review. This grants no furnishing approval."""
import argparse, hashlib, json, sys
sys.dont_write_bytecode=True
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
def signature(row):
    positions=lambda p: ','.join(map(str,p))+'\n'
    text=str(row['house_id'])+'\n'+row['name']+'\n'+positions([int(row['entry_x']),int(row['entry_y']),int(row['entry_z'])])
    for point in sorted(row['tiles'],key=lambda p:(p[2],p[1],p[0])): text+=positions(point)
    return hashlib.sha1(text.encode('utf8')).hexdigest()
def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--house-id',type=int,required=True);args=p.parse_args()
    b=json.loads((ROOT/'docs/HOUSE_MATRIX.json').read_text())
    for name,key in [('map_file','map_sha256'),('house_file','house_file_sha256')]:
        if hashlib.sha256((ROOT/b[name]).read_bytes()).hexdigest()!=b[key]:p.error('source drift: regenerate/review house inventory first')
    matches=[r for r in b['houses'] if r['house_id']==args.house_id]
    if len(matches)!=1:p.error('unknown/non-unique house ID')
    r=matches[0]
    print(json.dumps(dict(houseId=r['house_id'],name=r['name'],houseSignature=signature(r),mapSha1=b['map_sha1'],tileCount=r['tile_count'],liveOwner='UNKNOWN',approved=False),indent=2))
if __name__=='__main__':main()
