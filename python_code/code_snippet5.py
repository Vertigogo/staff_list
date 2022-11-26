
#coding:utf-8
#!/usr/bin/env python

'''
  ___  ________   ________  ___  ________  ___  ___  _________
|\  \|\   ___  \|\   ____\|\  \|\   ____\|\  \|\  \|\___   ___\
\ \  \ \  \\ \  \ \  \___|\ \  \ \  \___|\ \  \\\  \|___ \  \_|
 \ \  \ \  \\ \  \ \_____  \ \  \ \  \  __\ \   __  \   \ \  \
  \ \  \ \  \\ \  \|____|\  \ \  \ \  \|\  \ \  \ \  \   \ \  \
   \ \__\ \__\\ \__\____\_\  \ \__\ \_______\ \__\ \__\   \ \__\
    \|__|\|__| \|__|\_________\|__|\|_______|\|__|\|__|    \|__|
                   \|_________|



 ___       ________  ________  ________
|\  \     |\   __  \|\   __  \|\   ____\
\ \  \    \ \  \|\  \ \  \|\ /\ \  \___|_
 \ \  \    \ \   __  \ \   __  \ \_____  \
  \ \  \____\ \  \ \  \ \  \|\  \|____|\  \
   \ \_______\ \__\ \__\ \_______\____\_\  \
    \|_______|\|__|\|__|\|_______|\_________\
                                 \|_________|

By Anthr@X

'''
#V1.01

import platform
import sys
import socket as sk
import httplib
try:
	from subprocess import Popen, PIPE
	lowversion=False
except:
	lowversion=True
import re
from optparse import OptionParser
import threading
from threading import Thread
from Queue import Queue

NUM = 50
TIMEOUT=2
PORTS=[21,22,23,25,80,81,110,135,139,389,443,445,873,1433,1434,1521,2433,3306,3307,3389,5800,5900,8080,22222,22022,27017,28017]
URLS=['','phpinfo.php','phpmyadmin/','xampp/','zabbix/','jmx-console/','.svn/entries','nagios/','index.action','login.action']

PROBES=[
'\r\n\r\n',
'GET / HTTP/1.0\r\n\r\n',
'GET / \r\n\r\n',
'\x01\x00\x00\x00\x01\x00\x00\x00\x08\x08',
'\x80\0\0\x28\x72\xFE\x1D\x13\0\0\0\0\0\0\0\x02\0\x01\x86\xA0\0\x01\x97\x7C\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0',
'\x03\0\0\x0b\x06\xe0\0\0\0\0\0',
'\0\0\0\xa4\xff\x53\x4d\x42\x72\0\0\0\0\x08\x01\x40\0\0\0\0\0\0\0\0\0\0\0\0\0\0\x40\x06\0\0\x01\0\0\x81\0\x02PC NETWORK PROGRAM 1.0\0\x02MICROSOFT NETWORKS 1.03\0\x02MICROSOFT NETWORKS 3.0\0\x02LANMAN1.0\0\x02LM1.2X002\0\x02Samba\0\x02NT LANMAN 1.0\0\x02NT LM 0.12\0',
'\x80\x9e\x01\x03\x01\x00u\x00\x00\x00 \x00\x00f\x00\x00e\x00\x00d\x00\x00c\x00\x00b\x00\x00:\x00\x009\x00\x008\x00\x005\x00\x004\x00\x003\x00\x002\x00\x00/\x00\x00\x1b\x00\x00\x1a\x00\x00\x19\x00\x00\x18\x00\x00\x17\x00\x00\x16\x00\x00\x15\x00\x00\x14\x00\x00\x13\x00\x00\x12\x00\x00\x11\x00\x00\n\x00\x00\t\x00\x00\x08\x00\x00\x06\x00\x00\x05\x00\x00\x04\x00\x00\x03\x07\x00\xc0\x06\x00@\x04\x00\x80\x03\x00\x80\x02\x00\x80\x01\x00\x80\x00\x00\x02\x00\x00\x01\xe4i<+\xf6\xd6\x9b\xbb\xd3\x81\x9f\xbf\x15\xc1@\xa5o\x14,M \xc4\xc7\xe0\xb6\xb0\xb2\x1f\xf9)\xe8\x98',
'\x16\x03\0\0S\x01\0\0O\x03\0?G\xd7\xf7\xba,\xee\xea\xb2`~\xf3\0\xfd\x82{\xb9\xd5\x96\xc8w\x9b\xe6\xc4\xdb<=\xdbo\xef\x10n\0\0(\0\x16\0\x13\0\x0a\0f\0\x05\0\x04\0e\0d\0c\0b\0a\0`\0\x15\0\x12\0\x09\0\x14\0\x11\0\x08\0\x06\0\x03\x01\0',
'< NTP/1.2 >\n',