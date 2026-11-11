import pathlib,re,json,hashlib,collections
root=pathlib.Path(__file__).resolve().parents[2]
items={}
for block in re.split(r'(?=^TypeID\s*=)',(root/'data/items/items.srv').read_text(encoding='latin1'),flags=re.M):
 m=re.match(r'TypeID\s*=\s*(\d+)',block);n=re.search(r'^Name\s*=\s*"([^"]*)"',block,re.M)
 if m and n: items[int(m[1])]=re.sub(r'^(a|an) ','',n[1])
def expand(path,seen=()):
 if path in seen: return []
 out=[]
 for i,line in enumerate((root/path).read_text(encoding='latin1').splitlines(),1):
  match=re.match(r'\s*@"([^"]+)"',line)
  if match: out+=expand('data/npc/'+match[1],seen+(path,))
  else: out.append(dict(source=path,line=i,rule=line.strip()))
 return out
all_npcs=[];offers=[]
for p in sorted((root/'data/npc').glob('*.npc')):
 src=str(p.relative_to(root));raw=p.read_text(encoding='latin1');lines=expand(src)
 name=re.search(r'^Name\s*=\s*"([^"]+)"',raw,re.M);home=re.search(r'^Home\s*=\s*\[([^\]]+)\]',raw,re.M)
 npc=dict(file=src,name=name[1] if name else p.stem,home=home[1] if home else '',includes=sorted(set(x['source'] for x in lines if x['source']!=src)))
 all_npcs.append(npc)
 for row in lines:
  line=row['rule'];m=re.search(r'->\s*Type\s*=\s*(\d+)\s*,\s*Amount\s*=\s*1\s*,\s*Price\s*=\s*(\d+)\s*,.*\bTopic\s*=\s*(\d+)\s*$',line)
  if not m: continue
  id,price,topic=map(int,m.groups())
  confirms=[x for x in lines if re.match(r'Topic\s*=\s*'+str(topic)+r'\s*,"yes"',x['rule'])]
  valid=[x for x in confirms if 'Delete(Type)' in x['rule'] and re.search(r'\bCreateMoney\b',x['rule']) and 'Count(Type)>=Amount' in x['rule']]
  direction='purchase' if valid else 'npc_sells' if any('DeleteMoney' in x['rule'] and 'Create(Type)' in x['rule'] for x in confirms) else 'ambiguous'
  offers.append(dict(**npc,**row,id=id,item_name=items.get(id,'UNKNOWN'),price=price,topic=topic,direction=direction,confirmations=confirms))
result=dict(npcs=all_npcs,offers=offers,items=items)
if __name__ == "__main__":
 import argparse,csv,sys
 parser=argparse.ArgumentParser(description="Evidence extractor, not a catalog auto-approver")
 parser.add_argument("--json",type=pathlib.Path)
 parser.add_argument("--csv",type=pathlib.Path)
 args=parser.parse_args()
 if args.json: args.json.write_text(json.dumps(result,indent=2)+"\n")
 if args.csv:
  with args.csv.open("w",newline="") as out:
   writer=csv.writer(out);writer.writerow(["npc","npc_file","home","item_id","item_name","gp","direction","source","line","rule","confirmation"])
   for x in offers: writer.writerow([x["name"],x["file"],x["home"],x["id"],x["item_name"],x["price"],x["direction"],x["source"],x["line"],x["rule"]," | ".join(y["rule"] for y in x["confirmations"])])
 print(f"Scanned {len(all_npcs)} NPCs and recursive includes; {len(offers)} single-unit offers, {sum(x['direction']=='purchase' for x in offers)} purchase candidates.")
 print("Candidates still require buyer/access/tier/safety review; prices are never auto-selected.")
