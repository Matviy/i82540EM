from socket import *
import sys
import time

DST_MAC 	= bytearray([0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF])
SRC_MAC 	= bytearray([0x52, 0x54, 0x00, 0x12, 0x34, 0x57])
ETHER_TYPE 	= bytearray([0x80, 0x00])

DATA1 		= bytearray([0xDD] * 50)
DATA2 		= bytearray([0xCC] * 30)
DATA3 		= bytearray([0xAA] * 30)

DATA = DATA1 + DATA2 + DATA3

def sendeth(dst, src, eth_type, payload):

	assert(len(src) == len(dst) == 6)
	assert(len(eth_type) == 2)

	t = socket(AF_PACKET, SOCK_RAW)

	t.bind((sys.argv[1], 0))
	t.send(dst + src + eth_type + payload)

for x in range(0, 1000):
	sendeth(DST_MAC, SRC_MAC, ETHER_TYPE, DATA)
	#time.sleep(.001)
