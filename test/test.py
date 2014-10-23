#!/usr/bin/env python
import websocket
import threading
import time

def test_func(i, j):
    test_str = 'hello world'
    ws = websocket.WebSocket()
    ws.connect("ws://cp01-rdqa-dev347.cp01.baidu.com:8200/" + str(i))
    for k in range(0, j):
        ws.send(test_str)
        result = ws.recv()
        if result != test_str:
            print 'not match'
    ws.close()

for i in range(1, 50000):
    t = threading.Thread(target=test_func, args=(i, 10))
    t.start()

