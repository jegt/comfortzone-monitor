import io, unittest
from device import sse_events

class SSETest(unittest.TestCase):
    def test_events_and_cursor(self):
        wire=(b'retry: 2000\r\n: heartbeat\r\n\r\n'
              b'id: boot-a:17\nevent: traffic\ndata: {"sequence":17}\n\n'
              b'event: status\ndata: {"ok":true}\n\n')
        self.assertEqual(list(sse_events(io.BytesIO(wire))),[
            ('traffic','{"sequence":17}','boot-a:17'),('status','{"ok":true}',None)])
    def test_incomplete_event_not_acknowledged(self):
        self.assertEqual(list(sse_events(io.BytesIO(b'id: boot:8\ndata: {"sequence":8}\n'))),[])
    def test_multiline_and_bad_id(self):
        self.assertEqual(list(sse_events(io.BytesIO(b'id: a\0b\ndata: first\ndata: second\n\n'))),
                         [('message','first\nsecond',None)])
if __name__=='__main__':unittest.main()
