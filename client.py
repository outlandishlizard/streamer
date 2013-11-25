import socket
import sys
import pygame
import select

def client_hello(sock):
    sock.send("Text Client Hello")
def client_process(recv):
    print recv
def client_seek(r):
    if(r=='>'):
        client_send('CMD,SEEK,10')
    elif(r=='<'):
        client_send('CMD,SEEK,-10')
def client_toggleplay():
    client_send('CMD,TOGGLE')

sock = socket.socket(socket.AF_INET,socket.SOCK_STREAM)
sock.connect((sys.argv[1],int(sys.argv[2])))
client_hello(sock)

while 1:
    data = sock.recv(256)
    client_process(data)
    if sys.stdin in select.select([sys.stdin], [], [], 0)[0]:
        r = sys.stdin.readline().rstrip()
        if r:
            if r == '>' or r == '<':
                client_seek(r)
            elif r=='s':
                client_toggleplay()

        else:
            sys.exit(1)
