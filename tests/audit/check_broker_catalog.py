"""Compare every approved row to literal, unconditional NPC transaction evidence."""
import pathlib,sys,hashlib,json,re,xml.etree.ElementTree as E
sys.dont_write_bytecode=True
root=pathlib.Path(sys.argv[1])
sys.path.insert(0,str(root/'tests/audit'))
from scan_npc_offers import result
review={x['id']:x for x in json.loads((root/'docs/NPC_BUY_AUDIT.json').read_text())['approved']}
policy={int(x.get('id')):x for x in E.parse(root/'data/zedas/lootspill.xml').getroot()}
raw=(root/'data/zedas/broker.xml').read_bytes().replace(b'\r\n',b'\n')
digest=hashlib.sha1(raw).hexdigest()
assert f'REVIEWED_CATALOG_SHA1 = "{digest}"' in (root/'src/broker.cpp').read_text()
seen=set()
for buyer in E.fromstring(raw):
 source=(root/buyer.get('source')).read_bytes().replace(b'\r\n',b'\n')
 assert hashlib.sha1(source).hexdigest()==buyer.get('sha1')
 text=source.decode('latin1')
 assert re.search(r'^Name\s*=\s*"'+re.escape(buyer.get('name'))+'"',text,re.M)
 assert re.search(r'^Home\s*=\s*\['+re.escape(buyer.get('station').replace(';',','))+r'\]',text,re.M)
 assert buyer.get('kind')=='ordinary-town' and buyer.get('access')=='unconditional'
 # The reviewed successful greeting path must not be premium/quest/gender/vocation gated.
 assert re.search(r'^ADDRESS,"(?:hi\$|hello\$)"(?:,"[^"]+")?,!\s*->',text,re.M)
 for node in buyer:
  id=int(node.get('id'));row=review[id];assert id not in seen;seen.add(id)
  assert row['unconditional'] and row['kind']=='ordinary-town'
  assert all(node.get(k)==str(row[k]) for k in ('id','name','price','rule','line'))
  assert buyer.get('name')==row['buyer'] and buyer.get('town')==row['town'] and buyer.get('source')==row['source']
  candidates=[x for x in result['offers'] if x['file']==row['source'] and x['source']==row['source'] and x['line']==row['line']]
  assert len(candidates)==1 and candidates[0]['direction']=='purchase'
  assert candidates[0]['id']==id and candidates[0]['price']==row['price'] and candidates[0]['topic']==2
  assert buyer.get('confirmation') in [x['rule'] for x in candidates[0]['confirmations']]
  assert policy[id].get('category')=='broker-fodder'
assert seen==review.keys() and len(seen)==58
for id in (3362,3375,3378,3446,3447,3369,3370,3371,3415,3416,3232,2994): assert id not in seen
assert {3376,3559,3276,3349,3350,3425}<=seen
print(f'Validated {len(seen)} exact ordinary purchase identities, nine buyer/access/source paths and canonical fingerprint.')
