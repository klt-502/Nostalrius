"""Optional real-generated-world browser smoke test. Requires Playwright/Chromium, not a game server."""
from pathlib import Path
import argparse,shutil
from playwright.sync_api import sync_playwright
import json,threading
from http.server import ThreadingHTTPServer,SimpleHTTPRequestHandler
from functools import partial
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--viewer',type=Path,required=True)
parser.add_argument('--chromium',default=shutil.which('chromium'))
parser.add_argument('--screenshot',type=Path)
args=parser.parse_args()
root=args.viewer.resolve()
report=json.loads((root/'REPORT.json').read_text())
town=next(e for e in report['validation_coordinates'] if e['name'].lower()=='ankrahmun')
x,y,z=town['position']
class QuietHandler(SimpleHTTPRequestHandler):
    def log_message(self,*args): pass
server=ThreadingHTTPServer(("127.0.0.1",0),partial(QuietHandler,directory=str(root)))
threading.Thread(target=server.serve_forever,daemon=True).start()
origin=f"http://127.0.0.1:{server.server_port}"
with sync_playwright() as p:
    browser=p.chromium.launch(headless=True,executable_path=args.chromium,args=['--no-sandbox','--disable-dev-shm-usage'])
    context=browser.new_context(viewport={'width':1440,'height':960},permissions=['clipboard-read','clipboard-write'])
    page=context.new_page();errors=[];requests=[]
    page.on('pageerror',lambda e:errors.append(str(e)))
    page.on('console',lambda m:errors.append(m.text) if m.type=='error' else None)
    page.on('request',lambda r:requests.append(r.url))
    page.goto(origin+'/index.html');page.wait_for_timeout(1000)
    assert page.locator('#position').inner_text()==f'{x}, {y}, {z}'
    assert page.locator('#floor option').count()==16
    assert page.locator('#command').input_value()==f'/gotopos {x},{y},{z}'
    assert page.locator('canvas').evaluate('(c)=>{let d=c.getContext("2d").getImageData(0,0,c.width,c.height).data;let n=0;for(let i=0;i<d.length;i+=4)if(d[i]>50||d[i+1]>50||d[i+2]>50)n++;return n}')>10000
    if args.screenshot: page.screenshot(path=str(args.screenshot))
    page.locator('#copy').click()
    assert page.evaluate('navigator.clipboard.readText()')==f'/gotopos {x},{y},{z}'
    page.locator('#coordinates').fill('1,2,16');page.locator('#go').click();assert '0–15' in page.locator('#coordinate-error').inner_text()
    page.locator('#coordinates').fill(f'{x},{y},8');page.locator('#go').click();assert page.locator('#floor').input_value()=='8'
    page.locator('#floor').select_option('7');assert page.locator('#copy').is_disabled()
    page.locator('#search').fill('dwarf guard');assert 'matches' in page.locator('#matches').inner_text();assert page.locator('#results button').count()>0
    page.locator('#results button').first.click();assert 'spawn:' in page.locator('#entity').inner_text();assert 'group' in page.locator('#entity').inner_text()
    page.locator('#search').fill('house 1');assert page.locator('#results button').count()>0
    page.locator('#results button').first.click();assert 'house:' in page.locator('#entity').inner_text()
    page.locator('#coordinates').fill(f'{x},{y},{z}');page.locator('#go').click()
    b=page.locator('canvas').bounding_box();cx=b['x']+b['width']/2;cy=b['y']+b['height']/2
    page.mouse.click(cx,cy);assert page.locator('#position').inner_text()==f'{x}, {y}, {z}'
    oldscale=float(page.locator('#scale').inner_text()[:-1])
    page.mouse.move(cx,cy);page.mouse.down();page.mouse.move(cx+40,cy,steps=10);page.mouse.up();page.mouse.click(cx,cy)
    moved=list(map(int,page.locator('#position').inner_text().split(', ')))
    assert moved==[x-int(40/oldscale),y,z],moved
    page.locator('#zoom-in').click();page.wait_for_timeout(200)
    assert float(page.locator('#scale').inner_text()[:-1])>oldscale
    assert not errors,errors
    assert all(u.startswith(origin+'/') for u in requests),requests
    print('Browser passed: localhost-only assets, rendering, 16 floors, named/spawn/house search, coordinates, clipboard, pan, zoom, validation; zero console errors or external requests')
    browser.close()
server.shutdown()
