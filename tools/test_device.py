"""Exercise remote collection across overwrite gaps and board reboots."""
import json, tempfile, unittest
from pathlib import Path
from unittest.mock import patch
import device

class CollectorTest(unittest.TestCase):
    def test_reboot_gap_and_backlog(self):
        calls=[]
        a={'boot_id':'a','dropped_chunks':5,'chunks':[{'sequence':6,'ms':20,'hex':'0102'}]}
        b={'boot_id':'b','dropped_chunks':0,'chunks':[{'sequence':1,'ms':10,'hex':'0304'}]}
        responses=iter([a, {'rx_bytes':2}, a, b, {'rx_bytes':2}, b])
        def fetch(host,path,password):
            calls.append(path)
            try:return next(responses)
            except StopIteration:raise KeyboardInterrupt
        with tempfile.TemporaryDirectory() as tmp:
            dest=Path(tmp)/'capture.jsonl'
            with patch.object(device,'fetch',fetch),patch.object(device.time,'sleep'):
                with self.assertRaises(KeyboardInterrupt):device.collect('localhost',dest,'test-password')
            records=[json.loads(line) for line in dest.read_text().splitlines()]
        self.assertEqual([r['boot_id'] for r in records if r.get('event')=='boot'],['a','b'])
        self.assertEqual([r['chunks'] for r in records if r.get('event')=='gap'],[5])
        self.assertEqual([(r['boot_id'],r['sequence'],r['hex']) for r in records if 'hex' in r],
                         [('a',6,'0102'),('b',1,'0304')])
        self.assertIn('/api/traffic?after=6',calls)
        self.assertEqual(calls.count('/api/traffic?after=0'),3)

    def test_connection_loss_recovers(self):
        responses=iter([OSError('disconnected'),{'boot_id':'a','dropped_chunks':0,'chunks':[]}, {},
                        {'boot_id':'a','dropped_chunks':0,'chunks':[]}])
        def fetch(*args):
            try:r=next(responses)
            except StopIteration:raise KeyboardInterrupt
            if isinstance(r,Exception):raise r
            return r
        with tempfile.TemporaryDirectory() as tmp:
            dest=Path(tmp)/'capture.jsonl'
            with patch.object(device,'fetch',fetch),patch.object(device.time,'sleep'):
                with self.assertRaises(KeyboardInterrupt):device.collect('localhost',dest,'test-password')
            records=[json.loads(line) for line in dest.read_text().splitlines()]
        self.assertEqual(records[0]['event'],'connection_error')
        self.assertEqual(records[1]['event'],'boot')

if __name__=='__main__':unittest.main()
