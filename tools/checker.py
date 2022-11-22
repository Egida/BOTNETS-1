#!/usr/bin/python
# Mirai Telnet List Filter

import sys, re, os, socket, time
from threading import Thread

if len(sys.argv) < 3:
	sys.exit("\033[37mUsage: python checker.py list output.txt")

info = open(str(sys.argv[1]),'a+')
output_file = sys.argv[2]

def filter(ip,username,password):
	ip = str(ip).rstrip("\n")
	username = username.rstrip("\n")
	password = password.rstrip("\n")
	try:
		tn = socket.socket()
		tn.settimeout(5)
		tn.connect((ip,23))
		print "ONLINE:\033[32m %s\033[37m"%(ip)
		os.system("echo "+ip+":23 "+username+":"+password+" >> "+output_file+"")
		tn.close()
	except Exception:
		print "OFFLINE:\033[31m %s\033[37m"%(ip)
		tn.close()
		pass
 
for x in info:
	try:
		if ":23 " in x:
			x = x.replace(":23 ", ":")
		shl = x.split(":")
		fill = Thread(target=filter, args=(shl[0].rstrip("\n"),shl[1].rstrip("\n"),shl[2].rstrip("\n"),))
		fill.start()
		ip=shl[0]
		username=shl[1]
		password=shl[2]
		time.sleep(0.005)
	except:
		pass
#fill.join()