#!/usr/bin/env python3
"""Build/upload and collect diagnostics; reads credentials locally, never prints them."""
import argparse, json, pathlib, re, subprocess, time, urllib.request
ROOT = pathlib.Path(__file__).resolve().parents[1]
FQBN = 'esp32:esp32:esp32s3:USBMode=hwcdc,CDCOnBoot=cdc,FlashSize=16M,PartitionScheme=app3M_fat9M_16MB,PSRAM=opi'

def config():
    text = (ROOT/'comfortzone_wifi/secrets.h').read_text()
    result = {}
    for key in ('WIFI_SSID','WIFI_PASSWORD','OTA_PASSWORD','DEVICE_HOSTNAME'):
        m = re.search(r'^#define\s+'+key+r'\s+("(?:[^"\\]|\\.)*")\s*$', text, re.M)
        if not m: raise SystemExit('Missing or invalid private configuration field: '+key)
        result[key] = json.loads(m[1])
    return result

def run(args, secrets=()):
    # Avoid revealing credentials in Arduino CLI error messages or command echoes.
    p = subprocess.Popen(args, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    for line in p.stdout:
        for secret in secrets:
            if secret: line=line.replace(secret,'<redacted>')
        print(line,end='',flush=True)
    if p.wait(): raise SystemExit('Arduino CLI operation failed')

def build():
    run(['arduino-cli','compile','--fqbn',FQBN,'--libraries',str(ROOT/'libraries'),
         '--build-path',str(ROOT/'build'),str(ROOT/'comfortzone_wifi')])

class NoRedirect(urllib.request.HTTPRedirectHandler):
    def redirect_request(self,*args,**kwargs): return None

def fetch(host, path, password=None):
    req=urllib.request.Request('http://'+host+path)
    with urllib.request.build_opener(NoRedirect).open(req,timeout=5) as r: return json.load(r)

def collect(host, destination, password, duration=0):
    after=0; boot=None; started=time.monotonic()
    with open(destination,'a',buffering=1) as f:
        def emit(record): f.write(json.dumps(record)+'\n')
        while not duration or time.monotonic()-started<duration:
            try:
                data=fetch(host,'/api/traffic?after='+str(after),password)
                if data['boot_id']!=boot:
                    boot=data['boot_id'];after=0
                    emit({'event':'boot','boot_id':boot,'received_unix':time.time()})
                    emit({'event':'status','data':fetch(host,'/api/status',password)})
                    data=fetch(host,'/api/traffic?after=0',password)
                if data['dropped_chunks']:
                    emit({'event':'gap','boot_id':boot,'chunks':data['dropped_chunks']})
                for chunk in data['chunks']:
                    emit({'boot_id':boot,'received_unix':time.time(),**chunk});after=chunk['sequence']
                time.sleep(0.05 if len(data['chunks'])==64 else 0.5)
            except (OSError,ValueError) as e:
                emit({'event':'connection_error','type':type(e).__name__,'received_unix':time.time()})
                time.sleep(2)

def sse_events(response):
    """Parse complete SSE records; never advance a cursor for a partial event."""
    event='message'; data=[]; cursor=None
    for raw in response:
        line=raw.decode('utf-8').rstrip('\r\n')
        if not line:
            if data: yield event, '\n'.join(data), cursor
            event='message'; data=[]; cursor=None
        elif not line.startswith(':'):
            field, _, value=line.partition(':')
            if value.startswith(' '):value=value[1:]
            if field=='event':event=value
            elif field=='data':data.append(value)
            elif field=='id' and '\0' not in value:cursor=value

def stream_collect(host,destination,password,duration=0):
    cursor=None; started=time.monotonic()
    with open(destination,'a',buffering=1) as f:
        def emit(record):f.write(json.dumps(record)+'\n')
        while not duration or time.monotonic()-started<duration:
            headers={'Accept':'text/event-stream'}
            if cursor is not None:headers['Last-Event-ID']=cursor
            req=urllib.request.Request('http://'+host+'/api/events',headers=headers)
            try:
                with urllib.request.build_opener(NoRedirect).open(req,timeout=10) as response:
                    for event,data,event_id in sse_events(response):
                        emit({'event':event,'received_unix':time.time(),'data':json.loads(data)})
                        if event_id is not None:cursor=event_id
                        if duration and time.monotonic()-started>=duration:return
                time.sleep(2)
            except (OSError,ValueError) as e:
                emit({'event':'connection_error','type':type(e).__name__,'received_unix':time.time()})
                time.sleep(2)

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('action',choices=['build','usb','ota','status','collect'])
    p.add_argument('--port',default='/dev/cu.usbmodem101')
    p.add_argument('--host',help='Defaults to DEVICE_HOSTNAME.local from private configuration')
    p.add_argument('--output',default='comfortzone-traffic.jsonl')
    p.add_argument('--seconds',type=float,default=0)
    p.add_argument('--poll',action='store_true',help='Use legacy HTTP polling instead of SSE')
    a=p.parse_args(); c=config()
    if not a.host:a.host=c['DEVICE_HOSTNAME']+'.local'
    if a.action=='build': build();return
    if not c['WIFI_SSID'] or len(c['OTA_PASSWORD'])<16:
        raise SystemExit('Fill WIFI_SSID and a 16+ character OTA_PASSWORD in comfortzone_wifi/secrets.h first.')
    if a.action in ('usb','ota'):
        build()  # Ensure changes to credentials/code are compiled before upload.
        args=['arduino-cli','upload','--fqbn',FQBN,'--input-dir',str(ROOT/'build')]
        if a.action=='usb': args+=['--port',a.port,'--protocol','serial']
        else: args+=['--port',a.host,'--protocol','network','--upload-field','password='+c['OTA_PASSWORD'],
                     '--upload-property','upload.port.properties.port=3232']
        run(args+[str(ROOT/'comfortzone_wifi')],c.values())
    elif a.action=='status':print(json.dumps(fetch(a.host,'/api/status',c['OTA_PASSWORD']),indent=2))
    else:
        try:(collect if a.poll else stream_collect)(a.host,a.output,c['OTA_PASSWORD'],a.seconds)
        except KeyboardInterrupt:pass
if __name__=='__main__':main()
