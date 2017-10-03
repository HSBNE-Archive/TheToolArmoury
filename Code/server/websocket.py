import random
import traceback
import sys
import argparse
import threading
import logging.handlers
import logging
import json
import string
from autobahn.twisted.websocket import WebSocketServerProtocol, WebSocketServerFactory

# pylint: disable=invalid-name

params = argparse.ArgumentParser(description='HSBNE Tool Locker')
params.add_argument('--machineip', dest='machineip', help='server network interface', default=None)
params.add_argument('-v', '--verbose', action='store_true', help='output all debug levels')
params.add_argument('-l', '--logFile', dest='logFilename', default='/var/log/toolWebsocket.log', help='Path to the logfile')
args = params.parse_args()
globals().update(args.__dict__)


# Set main thread name for logging
threading.current_thread().name = 'ws_locker'

# https://docs.python.org/2/library/logging.html#logrecord-attributes
messageFormat = '%(asctime)s %(levelname)s:%(threadName)s:%(funcName)s %(message)s'
if verbose:
    logLevel = logging.DEBUG
else:
    logLevel = logging.INFO

logging.basicConfig(format=messageFormat, level=logLevel)

handler = logging.handlers.RotatingFileHandler(filename=logFilename, maxBytes=22000000, backupCount=5)
handler.setFormatter(logging.Formatter(messageFormat))
logging.getLogger().addHandler(handler)
log = logging.getLogger()

if machineip is not None:
    ipAddress = machineip
else:
    import netifaces as ni
    # pylint: disable=no-member
    if 2 in ni.ifaddresses('en0'):
        ipAddress = ni.ifaddresses('en0')[2][0]['addr']
    else:
        ipAddress = ni.ifaddresses('re0')[2][0]['addr']

connectionLookup = {}

class BrowserConnection():
    def __init__(self, proto, clientId):
        self.proto = proto
        self.clientId = clientId

    def onOpen(self):
        log.info("opening connection")
        connectionLookup[self.clientId] = self

    def onClose(self, wasClean, code, reason): # pylint: disable=unused-argument
        log.info("connection closed")
        connectionLookup.pop(self.clientId, None)

    def onMessage(self, payload, isBinary):
        if not isBinary:
            try:
                data = json.loads(payload.decode('utf8'))
                log.info("in: {}".format(data))
                self.proto.sendMessage(json.dumps(data).encode('utf8'))
            except Exception as err:
                #self.proto.sendClose(1000, "Exception raised: {0}".format(e))
                log.info("Unknown Message {}".format(payload.decode('utf8')))
                log.info(err)
                log.info(''.join(traceback.format_tb(err.__traceback__)))


class ToolConnectionServerProtocol(WebSocketServerProtocol):
    def __idGenerator(self, size=10, chars=string.ascii_uppercase + string.digits):
        return ''.join(random.choice(chars) for _ in range(size))

    def onConnect(self, request):
        # request has all the information from the initial
        # WebSocket opening handshake ..
        # print(request.peer)
        # print(request.headers)
        # print(request.host)
        # print(request.path)
        # print(request.params)
        # print(request.version)
        # print(request.origin)
        # print(request.protocols)
        # print(request.extensions)
        clientId = self.__idGenerator()
        while clientId in connectionLookup:
            log.info("Found identical Id, Generating new one")
            clientId = self.__idGenerator()

        connectionLookup[clientId] = BrowserConnection(self, clientId)
        # pylint: disable=attribute-defined-outside-init
        self.service = connectionLookup[clientId]
        self.onMessage = connectionLookup[clientId].onMessage
        self.onOpen = connectionLookup[clientId].onOpen
        self.onClose = connectionLookup[clientId].onClose

if __name__ == '__main__':
    from twisted.python import log as tlog
    from twisted.internet import reactor

    tlog.startLogging(sys.stdout)

    factory = WebSocketServerFactory(u"ws://{}:9000".format(ipAddress))
    factory.protocol = ToolConnectionServerProtocol

    reactor.listenTCP(9000, factory)
    reactor.run()
