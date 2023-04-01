import argparse
from time import sleep
import os
import binascii
import random

'''
Generates a stream of random hexidecimal characters
'''
parser = argparse.ArgumentParser(description='Process some integers.')
parser.add_argument('--space', type=bool, default=False,
                    help='boolean for whether or not to include spaces between data chunks')
parser.add_argument('--reps', type=int, default=10000,
                    help='number of times the loop should repeat itself, default is 10000')
parser.add_argument('--chars', type=int, default=15,
                    help='number of characters to use per chunk, default is 15')
parser.add_argument('--rchars_min', type=int,
                    help='random minimum number of characters to use to use per chunk')
parser.add_argument('--rchars_max', type=int,
                    help='random maximum number of characters to use to use per chunk')
parser.add_argument('--speed', type=float, default=.07,
                    help='speed at which output will be generated, default is .07, lower is faster')
args, 