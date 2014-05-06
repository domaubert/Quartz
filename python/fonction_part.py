import sys, os, time
import numpy as np
from multiprocessing import Pool

from struct import *
from fonction_IO import *
from fonction_physique import *

class Part : 
	def __init__(self,filePart):

		self.x      = unpack('f',filePart[0 : 4])[0]
		self.y      = unpack('f',filePart[4 : 8])[0]
		self.z      = unpack('f',filePart[8 :12])[0]
		
		self.vx     = unpack('f',filePart[12:16])[0]
		self.vy     = unpack('f',filePart[16:20])[0]
		self.vz     = unpack('f',filePart[20:24])[0]

		self.idx    = unpack('f',filePart[24:28])[0]
		self.mass   = unpack('f',filePart[28:32])[0]
		self.epot   = unpack('f',filePart[32:36])[0]
		self.ekin   = unpack('f',filePart[36:40])[0]

		if len(filePart)==44 :
			self.age    = unpack('f',filePart[40:44])[0]

	def printPos(self):	
		print self.x, self.y, self.z

	def printVit(self):	
		print self.vx, self.vy, self.vz


#################################################################################################################################

def getN(filename):
	try :
		filePart = open(filename, "rb")
		N = unpack('i',filePart.read(4))[0]
		filePart.close()
		return N

	except IOError:
		print 'cannot open', filename 
		return 1

def getRes(filename):
	try :
		filePart = open(filename, "rb")
		filePart.read(8)
		R = Part(filePart.read(44))
		filePart.close()

		Res = m2mo(R.mass, 100)

		return Res

	except IOError:
		print 'cannot open', filename 
		return 1


def getNtot(filename, args):

	Ntot=0
	for proc in range(args.nproc):
		filename = filename[0:-5] + str(proc).zfill(5)		
		Ntot += getN(filename)
	print  Ntot, "Particles"
	return Ntot



def getA(filename):
	try : 
		filePart = open(filename, "rb")
	except IOError:
		print 'cannot open', filename 
		return 1			

	N = unpack('i',filePart.read(4))[0]
	a = unpack('f',filePart.read(4))[0]
	filePart.close()
	return a

def getZ(filename):
	return 1.0/getA(filename) -1


def ReadPart(filename, args):

	
	if filename[-17:-13] == "star": 
		sizeofPart = 44
	else : 
		sizeofPart = 40
	
	Ntot  = getNtot(filename, args)
	parts = Ntot * [Part]
	data  = Ntot * [None]
	N=0
	P=0

	print "Reading file ", filename[:-7]
	for proc in range(args.nproc):

		filename = filename[0:-5] + str(proc).zfill(5)		
		try:
			filePart = open(filename, "rb")	
		except IOError:
			print 'cannot open', filename 
			return 1			

		N = unpack('i',filePart.read(4))[0]
		t = unpack('f',filePart.read(4))[0]
		i=0
	
		while i<N:
			data[P]=( filePart.read(sizeofPart) )			
			i+=1
			P+=1
		filePart.close()

	print 'Read OK'

	x=0
	while x < Ntot :
		parts[x] = Part(data[x])
		x+=1	

	return Ntot,t,parts

def ReadStars(filename, args):
	filename = filename[:-17] +  "star" +  filename[-13:]
	return ReadPart(filename,args)

#################################################################################################################################

def getMtotPart(file, args):

	N,t,stars = ReadStars(file, args)
	M=0

	for s in stars :
		M += s.mass
	return M









