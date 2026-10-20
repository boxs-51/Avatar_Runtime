"""Public protocol boundary negatives; no model bytes or process launch."""
import hashlib,struct,sys,unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[2]/'scripts'))
from moc3_lease import Key,command,EMPTY
from dataclasses import replace
class Frontend(unittest.TestCase):
 def setUp(self):self.key=Key(1,1,1,1,0,1,0,1,42,bytes(32),EMPTY)
 def test_exact_wire_and_max_controls(self):
  wire=command(self.key,[(i,(-1 if i%2 else 1)) for i in range(32)],3)
  self.assertEqual(len(wire),144+32*8);self.assertEqual(Key.read(wire[:136]),self.key)
 def test_identity_and_clock_overflow(self):
  for field,value in [('session',0),('epoch',0),('nonce',0),('generation',2**64-1),('clock',2**64),('local_clock',1_000_000_001),('local_sequence',9),('revision',2)]:
   with self.subTest(field=field),self.assertRaises(ValueError):replace(self.key,**{field:value}).wire()
 def test_invalid_controls_and_bindings(self):
  for controls in (True,-1,4,1.0):
   with self.assertRaises(ValueError):command(self.key,[],controls)
  for bindings in ([(0,0),(0,1)],[(True,0)],[(4096,0)],[(0,True)],[(0,float('nan'))],[(0,float('inf'))],[(0,1.1)],[(i,0) for i in range(33)]):
   with self.subTest(bindings=repr(bindings)),self.assertRaises(ValueError):command(self.key,bindings,1)
class ReferenceOracle(unittest.TestCase):
 def test_state_shape_and_finiteness(self):
  from moc3_verify_session_lifecycle import state_error
  valid=((((0.,0.),(0.,0.)),((0.,1.),(0.,0.))),)
  self.assertEqual(state_error(valid,valid),0)
  for bad in ((),valid+valid,((valid[0][0],),),((((float('nan'),0.),(0.,0.)),valid[0][1]),)):
   with self.assertRaises(AssertionError):state_error(bad,valid)
class AgentApi(unittest.TestCase):
 def test_exact_ppm_shape(self):
  from moc3_verify_session_viewer import ppm_pixels
  value=b'P6\n512 512\n255\n'+bytes(512*512*3)
  self.assertEqual(len(ppm_pixels(value)),512*512*3)
  for bad in (value[:-1],value+b'x',value[:100],b'P6\n1 1\n255\n'+bytes(3)):
   with self.assertRaises(AssertionError):ppm_pixels(bad)
 def test_idle_connections_do_not_block_requests_or_shutdown(self):
  import http.server,socket,threading,time,urllib.request
  from moc3_session_viewer import BoundedServer
  class Handler(http.server.BaseHTTPRequestHandler):
   def log_message(self,*args):pass
   def do_GET(self):self.send_response(200);self.end_headers();self.wfile.write(b'OK')
  server=BoundedServer(('127.0.0.1',0),Handler);thread=threading.Thread(target=server.serve_forever,kwargs={'poll_interval':.02});thread.start();clients=[]
  try:
   clients=[socket.create_connection(server.server_address) for _ in range(3)]
   clients[1].sendall(b'POST /status HTTP/1.0\r\nX: ')
   start=time.monotonic()
   with urllib.request.urlopen('http://127.0.0.1:'+str(server.server_port),timeout=2) as response:self.assertEqual(response.read(),b'OK')
   server.shutdown();server.server_close();thread.join(timeout=2)
   self.assertFalse(thread.is_alive());self.assertTrue(server.shutdown_verified);self.assertLess(time.monotonic()-start,2)
  finally:
   for client in clients:client.close()
   server.shutdown();server.server_close();thread.join(timeout=2)
 def test_delayed_body_cannot_enqueue_after_close(self):
  import http.server,socket,threading
  from moc3_session_viewer import BoundedServer,CommandMailbox
  mailbox=CommandMailbox();waiting=mailbox.submit('/update',{});entered=threading.Event()
  class Handler(http.server.BaseHTTPRequestHandler):
   def log_message(self,*args):pass
   def do_POST(self):
    self.rfile.read(1);entered.set();self.rfile.read(1)
    try:mailbox.submit('/update',{});code=200
    except ValueError:code=409
    self.send_response(code);self.end_headers()
  server=BoundedServer(('127.0.0.1',0),Handler);thread=threading.Thread(target=server.serve_forever,kwargs={'poll_interval':.02});thread.start();client=socket.create_connection(server.server_address);client.settimeout(2)
  try:
   client.sendall(b'POST /update HTTP/1.0\r\nContent-Length: 2\r\n\r\n{');self.assertTrue(entered.wait(2))
   mailbox.close();self.assertEqual(waiting.get(timeout=1)[0],409)
   client.sendall(b'}');self.assertIn(b'409',client.recv(1024));self.assertTrue(mailbox.commands.empty())
   with self.assertRaises(ValueError):mailbox.submit('/update',{})
  finally:
   client.close();server.shutdown();server.server_close();thread.join(timeout=2)
  self.assertTrue(server.shutdown_verified)
 def test_api_cas_and_schema_before_work(self):
  from moc3_session_viewer import Viewer
  from types import SimpleNamespace
  viewer=Viewer.__new__(Viewer);viewer.sequence=0;viewer.session=SimpleNamespace(session=7,package_revision=1,accepted=None)
  good={'session':7,'package':1,'generation':0,'sequence':1,'bindings':[],'control':1,'advance_ns':1}
  for patch in ({'session':8},{'package':2},{'generation':1},{'sequence':2},{'control':True},{'advance_ns':0},{'extra':0},{'bindings':[[0,float('nan')]]}):
   with self.subTest(patch=patch),self.assertRaises(ValueError):viewer.execute('/update',dict(good,**patch))
   self.assertEqual(viewer.sequence,0)
  import time
  with self.assertRaises(ValueError):viewer.execute('/update',good,time.monotonic()-1)
  self.assertEqual(viewer.sequence,0)
if __name__=='__main__':unittest.main()
