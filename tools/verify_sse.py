#!/usr/bin/env python3
"""Live SSE acceptance: anonymous access, concurrency, reboot cursor, heartbeat and client limit. Requires all three SSE slots to be available (close other dashboards/collectors first)."""
import argparse,json,time,urllib.request,urllib.error
from device import fetch,sse_events,NoRedirect
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--host',default='comfortzone-monitor.local')
host=parser.parse_args().host
def connect(cursor=None):
    headers={}
    if cursor:headers['Last-Event-ID']=cursor
    return urllib.request.build_opener(NoRedirect).open(urllib.request.Request('http://'+host+'/api/events',headers=headers),timeout=12)
s=fetch(host,'/api/status');assert s['firmware'].startswith(('comfortzone-diagnostic-','comfortzone-monitor-'))
streams=[]
try:
    first=connect('previous-boot:99999');streams.append(first)
    assert first.headers.get_content_type()=='text/event-stream'
    parser=sse_events(first);kind,payload,cursor=next(parser)
    assert kind=='status' and cursor==s['boot_id']+':0'
    for _ in range(2):
        r=connect(cursor);streams.append(r)
        assert next(sse_events(r))[0]=='status'
    assert fetch(host,'/api/status')['boot_id']==s['boot_id']
    try:connect()
    except urllib.error.HTTPError as e:assert e.code==503
    else:raise AssertionError('Expected bounded client count')
    deadline=time.monotonic()+15
    while True:
        kind,payload,_=next(parser)
        if kind=='status':break
        assert time.monotonic()<deadline,'No periodic status event'

finally:
    for r in streams:r.close()
print('PASS: anonymous SSE access, three concurrent streams, status access, fourth-client rejection, heartbeat and old-boot cursor reset.')
