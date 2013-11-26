import pygame
import socket
import sys
#import pygame
import io
import select
import struct
displaySurface = None
class Sock:
  # When a new socket is created, init it
  def __init__(self, host, port):
    self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    self.sock.connect((host, port))
  # Self explanitory
  def send_msg(self, msg):
    self.sock.send(msg)
  def get_msg(self, size):
    return self.sock.recv(size)
  def send_hello(self):
    l = len(sys.argv[3])
    client_sock.send_msg(struct.pack('!i', int(l)))
    client_sock.send_msg(sys.argv[3])

#    return self.sock.recv(size,socket.MSG_WAITALL)
  def close(self):
    return self.sock.close()

def client_process(data_frame):
  data_frame.seek(0)
  frame = pygame.image.load(data_frame)
  displaySurface.blit(frame,(0,0))
  pygame.display.flip()
if __name__ == "__main__":
  pygame.init()
  displaySurface = pygame.display.set_mode((256,256),pygame.DOUBLEBUF)
     
    # Check that host and port were included as args
  if len(sys.argv) != 4:
    print "Incorrect usage. ./server <host> <port> <videoname>"
    exit(1)
  try:
    # Make a new socket, passing host and port as params
    client_sock = Sock(sys.argv[1], int(sys.argv[2]))
    client_sock.send_hello()
  except:
    print "Error connecting:", sys.exc_info()[0]
    raise
  else:
    paused = 1
    # Get input from user, send it over the socket, read response
    while True:
      try:
        if sys.stdin in select.select([sys.stdin], [], [], 0)[0]:
          r = sys.stdin.readline().rstrip()
          if r:
            client_sock.send_msg(struct.pack('!i', int(r)))
            print "Sent: "+r
            paused*=-1
          else:
            print "Invalid command"

        frame_file = io.BytesIO('frame')
        if(paused > 0):
            length= client_sock.get_msg(4)
            length= struct.unpack('i',length)[0]
            data = client_sock.get_msg(length)
            f = open('./got','w+')
            f.write(data)
            f.close()
            frame_file.write(data)
            client_process(frame_file)
            frame_file.close()
      except SystemExit:
        exit()
      except KeyboardInterrupt:
        print " Shutdown requested...exiting"
        client_sock.send_msg(struct.pack('!i', 3))
        exit()
      except:
        print "Unexpected error with reading/writing:", sys.exc_info()[0]
        raise
        
  # Clean up
  client_sock.close()




"""def client_process(recv):
    print recv
def client_seek(r):
    if(r=='>'):
        client_send('CMD,SEEK,10')
    elif(r=='<'):
        client_send('CMD,SEEK,-10')
def client_toggleplay():
    client_send('CMD,TOGGLE')
def client_stop():
    client_send('CMD,STOP')

while 1:
    data = sock.recv(256)
    client_process(data)
    if sys.stdin in select.select([sys.stdin], [], [], 0)[0]:
        r = sys.stdin.readline().rstrip()
        if r:
            if r == '>' or r == '<':
                client_seek(r)
            elif r == 'p':
                client_toggleplay()
            elif r == 's':
                client_stop()

        else:
            sys.exit(1)
"""

