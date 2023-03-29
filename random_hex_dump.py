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
                    help='number of times the loop should repe