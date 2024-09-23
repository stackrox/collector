#!/usr/bin/env python3

import socket

from flask import Flask

app = Flask(__name__)


opened_sockets = {}


@app.route('/open/<int:port>')
def open_port(port: int):
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.bind((socket.gethostname(), port))
    s.listen()
    opened_sockets[port] = s
    return 'ok'


@app.route('/close/<int:port>')
def close_port(port: int):
    s = opened_sockets.get(port)
    if s is None:
        return 'not opened'
    s.close()
    return 'ok'
