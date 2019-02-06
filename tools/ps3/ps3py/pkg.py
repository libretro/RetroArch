#!/usr/bin/env python2
from __future__ import with_statement
import struct, sys

class StructType(tuple):
	def __getitem__(self, value):
		return [self] * value
	def __call__(self, value, endian='<'):
		if isinstance(value, str):
			return struct.unpack(endian + tuple.__getitem__(self, 0), value[:tuple.__getitem__(self, 1)])[0]
		else:
			return struct.pack(endian + tuple.__getitem__(self, 0), value)

class Struct(object):
	__slots__ = ('__attrs__', '__baked__', '__defs__', '__endian__', '__next__', '__sizes__', '__values__')
	int8 = StructType(('b', 1))
	uint8 = StructType(('B', 1))

	int16 = StructType(('h', 2))
	uint16 = StructType(('H', 2))

	int32 = StructType(('l', 4))
	uint32 = StructType(('L', 4))

	int64 = StructType(('q', 8))
	uint64 = StructType(('Q', 8))

	float = StructType(('f', 4))

	def string(cls, len, offset=0, encoding=None, stripNulls=False, value=''):
		return StructType(('string', (len, offset, encoding, stripNulls, value)))
	string = classmethod(string)

	LE = '<'
	BE = '>'
	__endian__ = '<'

	def __init__(self, func=None, unpack=None, **kwargs):
		self.__defs__ = []
		self.__sizes__ = []
		self.__attrs__ = []
		self.__values__ = {}
		self.__next__ = True
		self.__baked__ = False

		if func == None:
			self.__format__()
		else:
			sys.settrace(self.__trace__)
			func()
			for name in func.func_code.co_varnames:
				value = self.__frame__.f_locals[name]
				self.__setattr__(name, value)

		self.__baked__ = True

		if unpack != None:
			if isinstance(unpack, tuple):
				self.unpack(*unpack)
			else:
				self.unpack(unpack)

		if len(kwargs):
			for name in kwargs:
				self.__values__[name] = kwargs[name]

	def __trace__(self, frame, event, arg):
		self.__frame__ = frame
		sys.settrace(None)

	def __setattr__(self, name, value):
		if name in self.__slots__:
			return object.__setattr__(self, name, value)

		if self.__baked__ == False:
			if not isinstance(value, list):
				value = [value]
				attrname = name
			else:
				attrname = '*' + name

			self.__values__[name] = None

			for sub in value:
				if isinstance(sub, Struct):
					sub = sub.__class__
				try:
					if issubclass(sub, Struct):
						sub = ('struct', sub)
				except TypeError:
					pass
				type_, size = tuple(sub)
				if type_ == 'string':
					self.__defs__.append(Struct.string)
					self.__sizes__.append(size)
					self.__attrs__.append(attrname)
					self.__next__ = True

					if attrname[0] != '*':
						self.__values__[name] = size[3]
					elif self.__values__[name] == None:
						self.__values__[name] = [size[3] for val in value]
				elif type_ == 'struct':
					self.__defs__.append(Struct)
					self.__sizes__.append(size)
					self.__attrs__.append(attrname)
					self.__next__ = True

					if attrname[0] != '*':
						self.__values__[name] = size()
					elif self.__values__[name] == None:
						self.__values__[name] = [size() for val in value]
				else:
					if self.__next__:
						self.__defs__.append('')
						self.__sizes__.append(0)
						self.__attrs__.append([])
						self.__next__ = False

					self.__defs__[-1] += type_
					self.__sizes__[-1] += size
					self.__attrs__[-1].append(attrname)

					if attrname[0] != '*':
						self.__values__[name] = 0
					elif self.__values__[name] == None:
						self.__values__[name] = [0 for val in value]
		else:
			try:
				self.__values__[name] = value
			except KeyError:
				raise AttributeError(name)

	def __getattr__(self, name):
		if self.__baked__ == False:
			return name
		else:
			try:
				return self.__values__[name]
			except KeyError:
				raise AttributeError(name)

	def __len__(self):
		ret = 0
		arraypos, arrayname = None, None

		for i in range(len(self.__defs__)):
			sdef, size, attrs = self.__defs__[i], self.__sizes__[i], self.__attrs__[i]

			if sdef == Struct.string:
				size, offset, encoding, stripNulls, value = size
				if isinstance(size, str):
					size = self.__values__[size] + offset
			elif sdef == Struct:
				if attrs[0] == '*':
					if arrayname != attrs:
						arrayname = attrs
						arraypos = 0
					size = len(self.__values__[attrs[1:]][arraypos])
				size = len(self.__values__[attrs])

			ret += size

		return ret

	def unpack(self, data, pos=0):
		for name in self.__values__:
			if not isinstance(self.__values__[name], Struct):
				self.__values__[name] = None
			elif self.__values__[name].__class__ == list and len(self.__values__[name]) != 0:
				if not isinstance(self.__values__[name][0], Struct):
					self.__values__[name] = None

		arraypos, arrayname = None, None

		for i in range(len(self.__defs__)):
			sdef, size, attrs = self.__defs__[i], self.__sizes__[i], self.__attrs__[i]

			if sdef == Struct.string:
				size, offset, encoding, stripNulls, value = size
				if isinstance(size, str):
					size = self.__values__[size] + offset

				temp = data[pos:pos+size]
				if len(temp) != size:
					raise StructException('Expected %i byte string, got %i' % (size, len(temp)))

				if encoding != None:
					temp = temp.decode(encoding)

				if stripNulls:
					temp = temp.rstrip('\0')

				if attrs[0] == '*':
					name = attrs[1:]
					if self.__values__[name] == None:
						self.__values__[name] = []
					self.__values__[name].append(temp)
				else:
					self.__values__[attrs] = temp
				pos += size
			elif sdef == Struct:
				if attrs[0] == '*':
					if arrayname != attrs:
						arrayname = attrs
						arraypos = 0
					name = attrs[1:]
					self.__values__[attrs][arraypos].unpack(data, pos)
					pos += len(self.__values__[attrs][arraypos])
					arraypos += 1
				else:
					self.__values__[attrs].unpack(data, pos)
					pos += len(self.__values__[attrs])
			else:
				values = struct.unpack(self.__endian__+sdef, data[pos:pos+size])
				pos += size
				j = 0
				for name in attrs:
					if name[0] == '*':
						name = name[1:]
						if self.__values__[name] == None:
							self.__values__[name] = []
						self.__values__[name].append(values[j])
					else:
						self.__values__[name] = values[j]
					j += 1

		return self

	def pack(self):
		arraypos, arrayname = None, None

		ret = ''
		for i in range(len(self.__defs__)):
			sdef, size, attrs = self.__defs__[i], self.__sizes__[i], self.__attrs__[i]

			if sdef == Struct.string:
				size, offset, encoding, stripNulls, value = size
				if isinstance(size, str):
					size = self.__values__[size]+offset

				if attrs[0] == '*':
					if arrayname != attrs:
						arraypos = 0
						arrayname = attrs
					temp = self.__values__[attrs[1:]][arraypos]
					arraypos += 1
				else:
					temp = self.__values__[attrs]

				if encoding != None:
					temp = temp.encode(encoding)

				temp = temp[:size]
				ret += temp + ('\0' * (size - len(temp)))
			elif sdef == Struct:
				if attrs[0] == '*':
					if arrayname != attrs:
						arraypos = 0
						arrayname = attrs
					ret += self.__values__[attrs[1:]][arraypos].pack()
					arraypos += 1
				else:
					ret += self.__values__[attrs].pack()
			else:
				values = []
				for name in attrs:
					if name[0] == '*':
						if arrayname != name:
							arraypos = 0
							arrayname = name
						values.append(self.__values__[name[1:]][arraypos])
						arraypos += 1
					else:
						values.append(self.__values__[name])

				ret += struct.pack(self.__endian__+sdef, *values)
		return ret

	def __getitem__(self, value):
		return [('struct', self.__class__)] * value

class SelfHeader(Struct):
	__endian__ = Struct.BE
	def __format__(self):
		self.magic	= Struct.uint32
		self.headerVer  = Struct.uint32
		self.flags	= Struct.uint16
		self.type	= Struct.uint16
		self.meta	= Struct.uint32
		self.headerSize = Struct.uint64
		self.encryptedSize = Struct.uint64
		self.unknown	= Struct.uint64
		self.AppInfo	= Struct.uint64
		self.elf	= Struct.uint64
		self.phdr	= Struct.uint64
		self.shdr	= Struct.uint64
		self.phdrOffsets = Struct.uint64
		self.sceversion = Struct.uint64
		self.digest	= Struct.uint64
		self.digestSize = Struct.uint64

class AppInfo(Struct):
	__endian__ = Struct.BE
	def __format__(self):
		self.authid	= Struct.uint64
		self.unknown	= Struct.uint32
		self.appType	= Struct.uint32
		self.appVersion = Struct.uint64

import struct
import sys
import hashlib
import os
import getopt
import ConfigParser
import io
import glob

TYPE_NPDRMSELF = 0x1
TYPE_RAW = 0x3
TYPE_DIRECTORY = 0x4

TYPE_OVERWRITE_ALLOWED = 0x80000000

class EbootMeta(Struct):
	__endian__ = Struct.BE
	def __format__(self):
		self.magic 		= Struct.uint32
		self.unk1 		= Struct.uint32
		self.drmType 		= Struct.uint32
		self.unk2		= Struct.uint32
		self.contentID 		= Struct.uint8[0x30]
		self.fileSHA1 		= Struct.uint8[0x10]
		self.notSHA1 		= Struct.uint8[0x10]
		self.notXORKLSHA1 	= Struct.uint8[0x10]
		self.nulls 		= Struct.uint8[0x10]
class MetaHeader(Struct):
	__endian__ = Struct.BE
	def __format__(self):
		self.unk1 	= Struct.uint32
		self.unk2 	= Struct.uint32
		self.drmType 	= Struct.uint32
		self.unk4 	= Struct.uint32

		self.unk21 	= Struct.uint32
		self.unk22 	= Struct.uint32
		self.unk23 	= Struct.uint32
		self.unk24 	= Struct.uint32

		self.unk31 	= Struct.uint32
		self.unk32 	= Struct.uint32
		self.unk33 	= Struct.uint32
		self.secondaryVersion 	= Struct.uint16
		self.unk34 	= Struct.uint16

		self.dataSize 	= Struct.uint32
		self.unk42 	= Struct.uint32
		self.unk43 	= Struct.uint32
		self.packagedBy 	= Struct.uint16
		self.packageVersion 	= Struct.uint16
class DigestBlock(Struct):
	__endian__ = Struct.BE
	def __format__(self):
		self.type 	= Struct.uint32
		self.size 	= Struct.uint32
		self.isNext = Struct.uint64
class FileHeader(Struct):
	__endian__ = Struct.BE
	def __format__(self):
		self.fileNameOff 	= Struct.uint32
		self.fileNameLength = Struct.uint32
		self.fileOff 		= Struct.uint64

		self.fileSize 	= Struct.uint64
		self.flags		= Struct.uint32
		self.padding 		= Struct.uint32
	def __str__(self):
		out  = ""
		out += "[X] File Name: %s [" % self.fileName
		if self.flags & 0xFF == TYPE_NPDRMSELF:
			out += "NPDRM Self]"
		elif self.flags & 0xFF == TYPE_DIRECTORY:
			out += "Directory]"
		elif self.flags & 0xFF == TYPE_RAW:
			out += "Raw Data]"
		else:
			out += "Unknown]"
		if (self.flags & TYPE_OVERWRITE_ALLOWED ) != 0:
			out += " Overwrite allowed.\n"
		else:
			out += " Overwrite NOT allowed.\n"
		out += "\n"

		out += "[X] File Name offset: %08x\n" % self.fileNameOff
		out += "[X] File Name Length: %08x\n" % self.fileNameLength
		out += "[X] Offset To File Data: %016x\n" % self.fileOff

		out += "[X] File Size: %016x\n" % self.fileSize
		out += "[X] Flags: %08x\n" % self.flags
		out += "[X] Padding: %08x\n\n" % self.padding
		assert self.padding == 0, "I guess I was wrong, this is not padding."

		return out
	def __repr__(self):
		return self.fileName + ("<FileHeader> Size: 0x%016x" % self.fileSize)
	def __init__(self):
		Struct.__init__(self)
		self.fileName = ""

class Header(Struct):
	__endian__ = Struct.BE
	def __format__(self):
		self.magic = Struct.uint32
		self.type = Struct.uint32
		self.pkgInfoOff = Struct.uint32
		self.unk1 = Struct.uint32

		self.headSize = Struct.uint32
		self.itemCount = Struct.uint32
		self.packageSize = Struct.uint64

		self.dataOff = Struct.uint64
		self.dataSize = Struct.uint64

		self.contentID = Struct.uint8[0x30]
		self.QADigest = Struct.uint8[0x10]
		self.KLicensee = Struct.uint8[0x10]

	def __str__(self):
		context = keyToContext(self.QADigest)
		setContextNum(context, 0xFFFFFFFFFFFFFFFF)
		licensee = crypt(context, listToString(self.KLicensee), 0x10)

		out  = ""
		out += "[X] Magic: %08x\n" % self.magic
		out += "[X] Type: %08x\n" % self.type
		out += "[X] Offset to package info: %08x\n" % self.pkgInfoOff
		out += "[ ] unk1: %08x\n" % self.unk1

		out += "[X] Head Size: %08x\n" % self.headSize
		out += "[X] Item Count: %08x\n" % self.itemCount
		out += "[X] Package Size: %016x\n" % self.packageSize

		out += "[X] Data Offset: %016x\n" % self.dataOff
		out += "[X] Data Size: %016x\n" % self.dataSize

		out += "[X] ContentID: '%s'\n" % (nullterm(self.contentID))

		out += "[X] QA_Digest: %s\n" % (nullterm(self.QADigest, True))
		out += "[X] K Licensee: %s\n" % licensee.encode('hex')

		return out
def listToString(inlist):
	if isinstance(inlist, list):
		return ''.join(["%c" % el for el in inlist])
	else:
		return ""
def nullterm(str_plus, printhex=False):
	if isinstance(str_plus, list):
		if printhex:
			str_plus = ''.join(["%X" % el for el in str_plus])
		else:
			str_plus = listToString(str_plus)
	z = str_plus.find('\0')
	if z != -1:
		return str_plus[:z]
	else:
		return str_plus

def keyToContext(key):
	if isinstance(key, list):
		key = listToString(key)
		key = key[0:16]
	largekey = []
	for i in range(0, 8):
		largekey.append(ord(key[i]))
	for i in range(0, 8):
		largekey.append(ord(key[i]))
	for i in range(0, 8):
		largekey.append(ord(key[i+8]))
	for i in range(0, 8):
		largekey.append(ord(key[i+8]))
	for i in range(0, 0x20):
		largekey.append(0)
	return largekey

#Thanks to anonymous for the help with the RE of this part,
# the x86 mess of ands and ors made my head go BOOM headshot.
def manipulate(key):
	if not isinstance(key, list):
		return
	tmp = listToString(key[0x38:])

	tmpnum = struct.unpack('>Q', tmp)[0]
	tmpnum += 1
	tmpnum = tmpnum & 0xFFFFFFFFFFFFFFFF
	setContextNum(key, tmpnum)
def setContextNum(key, tmpnum):
	tmpchrs = struct.pack('>Q', tmpnum)

	key[0x38] = ord(tmpchrs[0])
	key[0x39] = ord(tmpchrs[1])
	key[0x3a] = ord(tmpchrs[2])
	key[0x3b] = ord(tmpchrs[3])
	key[0x3c] = ord(tmpchrs[4])
	key[0x3d] = ord(tmpchrs[5])
	key[0x3e] = ord(tmpchrs[6])
	key[0x3f] = ord(tmpchrs[7])

try:
	import pkgcrypt
except:
	print ""
	print "---------------------"
	print "RETROARCH BUILD ERROR"
	print "---------------------"
	print "Couldn't make PKG file. Go into the ps3py directory, and type the following:"
	print ""
	print "python2 setup.py build"
	print ""
	print "This should create a pkgcrypt.so file in the build/ directory. Move that file"
	print "over to the root of the ps3py directory and try running this script again."

def crypt(key, inbuf, length):
	if not isinstance(key, list):
		return ""
	return pkgcrypt.pkgcrypt(listToString(key), inbuf, length);

	# Original python (slow) implementation
	ret = ""
	offset = 0
	while length > 0:
		bytes_to_dump = length
		if length > 0x10:
			bytes_to_dump = 0x10
		outhash = SHA1(listToString(key)[0:0x40])
		for i in range(0, bytes_to_dump):
			ret += chr(ord(outhash[i]) ^ ord(inbuf[offset]))
			offset += 1
		manipulate(key)
		length -= bytes_to_dump
	return ret
def SHA1(data):
	m = hashlib.sha1()
	m.update(data)
	return m.digest()

pkgcrypt.register_sha1_callback(SHA1)

def getFiles(files, folder, original):
	oldfolder = folder
	foundFiles = glob.glob( os.path.join(folder, '*') )
	sortedList = []
	for filepath in foundFiles:
		if not os.path.isdir(filepath):
			sortedList.append(filepath)
	for filepath in foundFiles:
		if os.path.isdir(filepath):
			sortedList.append(filepath)
	for filepath in sortedList:
		newpath = filepath.replace("\\", "/")
		newpath = newpath[len(original):]
		if os.path.isdir(filepath):
			folder = FileHeader()
			folder.fileName = newpath
			folder.fileNameOff 	= 0
			folder.fileNameLength = len(folder.fileName)
			folder.fileOff 		= 0

			folder.fileSize 	= 0
			folder.flags		= TYPE_OVERWRITE_ALLOWED | TYPE_DIRECTORY
			folder.padding 		= 0
			files.append(folder)
			getFiles(files, filepath, original)
		else:
			file = FileHeader()
			file.fileName = newpath
			file.fileNameOff 	= 0
			file.fileNameLength = len(file.fileName)
			file.fileOff 		= 0
			file.fileSize 	= os.path.getsize(filepath)
			file.flags		= TYPE_OVERWRITE_ALLOWED | TYPE_RAW
			if newpath == "USRDIR/EBOOT.BIN":
				file.fileSize = ((file.fileSize - 0x30 + 63) & ~63) + 0x30
				file.flags		= TYPE_OVERWRITE_ALLOWED | TYPE_NPDRMSELF

			file.padding 		= 0
			files.append(file)

def pack(folder, contentid, outname=None):

	qadigest = hashlib.sha1()

	header = Header()
	header.magic = 0x7F504B47
	header.type = 0x01
	header.pkgInfoOff = 0xC0
	header.unk1 = 0x05

	header.headSize = 0x80
	header.itemCount = 0
	header.packageSize = 0

	header.dataOff = 0x140
	header.dataSize = 0

	for i in range(0, 0x30):
		header.contentID[i] = 0

	for i in range(0,0x10):
		header.QADigest[i] = 0
		header.KLicensee[i] = 0

	metaBlock = MetaHeader()
	metaBlock.unk1 		= 1 #doesnt change output of --extract
	metaBlock.unk2 		= 4 #doesnt change output of --extract
	metaBlock.drmType 	= 3 #1 = Network, 2 = Local, 3 = Free, anything else = unknown
	metaBlock.unk4 		= 2

	metaBlock.unk21 	= 4
	metaBlock.unk22 	= 5 #5 == gameexec, 4 == gamedata
	metaBlock.unk23 	= 3
	metaBlock.unk24 	= 4

	metaBlock.unk31 	= 0xE   #packageType 0x10 == patch, 0x8 == Demo&Key, 0x0 == Demo&Key (AND UserFiles = NotOverWrite), 0xE == normal, use 0xE for gamexec, and 8 for gamedata
	metaBlock.unk32 	= 4   #when this is 5 secondary version gets used??
	metaBlock.unk33 	= 8   #doesnt change output of --extract
	metaBlock.secondaryVersion 	= 0
	metaBlock.unk34 	= 0

	metaBlock.dataSize 	= 0
	metaBlock.unk42 	= 5
	metaBlock.unk43 	= 4
	metaBlock.packagedBy 	= 0x1061
	metaBlock.packageVersion 	= 0

	files = []
	getFiles(files, folder, folder)
	header.itemCount = len(files)
	dataToEncrypt = ""
	fileDescLength = 0
	fileOff = 0x20 * len(files)
	for file in files:
		alignedSize = (file.fileNameLength + 0x0F) & ~0x0F
		file.fileNameOff = fileOff
		fileOff += alignedSize
	for file in files:
		file.fileOff = fileOff
		fileOff += (file.fileSize + 0x0F) & ~0x0F
		dataToEncrypt += file.pack()
	for file in files:
		alignedSize = (file.fileNameLength + 0x0F) & ~0x0F
		dataToEncrypt += file.fileName
		dataToEncrypt += "\0" * (alignedSize-file.fileNameLength)
	fileDescLength = len(dataToEncrypt)
	for file in files:
		if not file.flags & 0xFF == TYPE_DIRECTORY:
			path = os.path.join(folder, file.fileName)
			fp = open(path, 'rb')
			fileData = fp.read()
			qadigest.update(fileData)
			fileSHA1 = SHA1(fileData)
			fp.close()
			if fileData[0:9] == "SCE\0\0\0\0\x02\x80":
				fselfheader = SelfHeader()
				fselfheader.unpack(fileData[0:len(fselfheader)])
				appheader = AppInfo()
				appheader.unpack(fileData[fselfheader.AppInfo:fselfheader.AppInfo+len(appheader)])
				found = False
				digestOff = fselfheader.digest
				while not found:
					digest = DigestBlock()
					digest.unpack(fileData[digestOff:digestOff+len(digest)])
					if digest.type == 3:
						found = True
					else:
						digestOff += digest.size
					if digest.isNext != 1:
						break
				digestOff += len(digest)
				if appheader.appType == 8 and found:
					dataToEncrypt += fileData[0:digestOff]

					meta = EbootMeta()
					meta.magic = 0x4E504400
					meta.unk1 			= 1
					meta.drmType 		= metaBlock.drmType
					meta.unk2			= 1
					for i in range(0,min(len(contentid), 0x30)):
						meta.contentID[i] = ord(contentid[i])
					for i in range(0,0x10):
						meta.fileSHA1[i] 		= ord(fileSHA1[i])
						meta.notSHA1[i] 		= (~meta.fileSHA1[i]) & 0xFF
						if i == 0xF:
							meta.notXORKLSHA1[i] 	= (1 ^ meta.notSHA1[i] ^ 0xAA) & 0xFF
						else:
							meta.notXORKLSHA1[i] 	= (0 ^ meta.notSHA1[i] ^ 0xAA) & 0xFF
						meta.nulls[i] 			= 0
					dataToEncrypt += meta.pack()
					dataToEncrypt += fileData[digestOff + 0x80:]
				else:
					dataToEncrypt += fileData
			else:
				dataToEncrypt += fileData

			dataToEncrypt += '\0' * (((file.fileSize + 0x0F) & ~0x0F) - len(fileData))
	header.dataSize = len(dataToEncrypt)
	metaBlock.dataSize 	= header.dataSize
	header.packageSize = header.dataSize + 0x1A0
	head = header.pack()
	qadigest.update(head)
	qadigest.update(dataToEncrypt[0:fileDescLength])
	QA_Digest = qadigest.digest()

	for i in range(0, 0x10):
		header.QADigest[i] = ord(QA_Digest[i])

	for i in range(0, min(len(contentid), 0x30)):
		header.contentID[i] = ord(contentid[i])

	context = keyToContext(header.QADigest)
	setContextNum(context, 0xFFFFFFFFFFFFFFFF)
	licensee = crypt(context, listToString(header.KLicensee), 0x10)

	for i in range(0, min(len(contentid), 0x10)):
		header.KLicensee[i] = ord(licensee[i])

	if outname != None:
		outFile = open(outname, 'wb')
	else:
		outFile = open(contentid + ".pkg", 'wb')
	outFile.write(header.pack())
	headerSHA = SHA1(header.pack())[3:19]
	outFile.write(headerSHA)

	metaData = metaBlock.pack()
	metaBlockSHA = SHA1(metaData)[3:19]
	metaBlockSHAPad = '\0' * 0x30

	context = keyToContext([ord(c) for c in metaBlockSHA])
	metaBlockSHAPadEnc = crypt(context, metaBlockSHAPad, 0x30)

	context = keyToContext([ord(c) for c in headerSHA])
	metaBlockSHAPadEnc2 = crypt(context, metaBlockSHAPadEnc, 0x30)
	outFile.write(metaBlockSHAPadEnc2)
	outFile.write(metaData)
	outFile.write(metaBlockSHA)
	outFile.write(metaBlockSHAPadEnc)

	context = keyToContext(header.QADigest)
	encData = crypt(context, dataToEncrypt, header.dataSize)
	outFile.write(encData)
	outFile.write('\0' * 0x60)
	outFile.close()
	print header

def usage():
	print """usage: [based on revision 1061]

    python pkg.py target-directory [out-file]

    python pkg.py [options] npdrm-package
        -l | --list             list packaged files.
        -x | --extract          extract package.

    python pkg.py [options]
        --version               print revision.
        --help                  print this message."""

def version():
	print """pkg.py 0.5"""

def main():
	extract = False
	list = False
	contentid = None
	try:
		opts, args = getopt.getopt(sys.argv[1:], "hx:dvl:c:", ["help", "extract=", "version", "list=", "contentid="])
	except getopt.GetoptError:
		usage()
		sys.exit(2)
	for opt, arg in opts:
		if opt in ("-h", "--help"):
			usage()
			sys.exit(2)
		elif opt in ("-v", "--version"):
			version()
			sys.exit(2)
		elif opt in ("-x", "--extract"):
			fileToExtract = arg
			extract = True
		elif opt in ("-l", "--list"):
			fileToList = arg
			list = True
		elif opt in ("-c", "--contentid"):
			contentid = arg
		else:
			usage()
			sys.exit(2)
	if extract:
		unpack(fileToExtract)
	else:
		if len(args) == 1 and contentid != None:
			pack(args[0], contentid)
		elif len(args) == 2 and contentid != None:
			pack(args[0], contentid, args[1])
		else:
			usage()
			sys.exit(2)
if __name__ == "__main__":
	main()
