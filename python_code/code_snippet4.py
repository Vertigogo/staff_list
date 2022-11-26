

import os
import pickle
import hashlib
import binascii
import multiprocessing
from ellipticcurve.privateKey import PrivateKey

DATABASE = r'database/MAR_23_2019/'

def generate_private_key():
	"""
	Generate a random 32-byte hex integer which serves as a randomly
	generated Bitcoin private key.
	Average Time: 0.0000061659 seconds
	"""
	return binascii.hexlify(os.urandom(32)).decode('utf-8').upper()

def private_key_to_public_key(private_key):
	"""
	Accept a hex private key and convert it to its respective public key.
	Because converting a private key to a public key requires SECP256k1 ECDSA
	signing, this function is the most time consuming and is a bottleneck in