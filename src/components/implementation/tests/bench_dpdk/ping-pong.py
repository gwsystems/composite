#!/usr/bin/python3
from scapy.all import *
import time
import _thread

def show_packet(packet):
	# packet.show()
	print ('Test Success!')
	exit(0)

def send_pkt():
	sent_pkt=Ether(dst='66:66:66:66:66:66')/IP(src='10.10.1.1', dst='10.10.1.2')/ICMP()
	# sent_pkt.show()
	sendp(sent_pkt,iface='tap0',filter='inbound and ether dst 66:66:66:66:66:66')

def dump_packet():
	rx=sniff(iface='tap0', prn=show_packet, timeout=5)
	npackets=len(rx)
	if npackets ==0 :
		print ('Test Fail!')
		exit(0)
try:
	_thread.start_new_thread(dump_packet, ())
	time.sleep(2)
	_thread.start_new_thread(send_pkt, ())
except:
	print ("Error: failed to start packet thread!")

while 1:
	pass
