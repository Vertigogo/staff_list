/*global Uint8Array:true ArrayBuffer:true */
"use strict";

var zlib = require('zlib');
var PNG = require('./PNG');

var inflate = function(data, callback){
	return zlib.inflate(new Buffer(data), callback);
};

var slice = Array.prototype.slice;
var toString = Object.prototype.toString;

function equalBytes(a, b){
	if (a.length != b.length) return false;
	for (var l = a.length; l--;) if (a[l] != b[l]) return false;
	return true;
}

function readUInt32(buffer, offset){
	return (buffer[offset] << 24) +
		(buffer[offset + 1] << 16) +
		(buffer[offset + 2] << 8) +
		(buffer[offset + 3] << 0);
}

function readUInt16(buffer, offset){
	return (buffer[offset + 1] << 8) + (buffer[offset] << 0);
}

function readUInt8(buffer, offset){
	return buffer[offset] << 0;
}

function bufferToString(buffer){
	var str = '';
	for (var i = 0; i < buffer.length; i++){
		str += String.fromCharCode(buffer[i]);
	}
	return str;
}

var PNGReader = function(bytes){

	if (typeof bytes == 'string'){
		var bts = bytes;
		bytes = new Array(bts.length);
		for (var i = 0, l = bts.length; i < l; i++){
			bytes[i] = bts[i].charCodeAt(0);
		}
	} else {
		var type = toString.call(bytes).slice(8, -1);
		if (type == 'ArrayBuffer') bytes = new Uint8Array(bytes);
	}

	// current pointer
	this.i = 0;
	// bytes buffer
	this.bytes = bytes;
	// Output object
	this.png = new PNG();

	this.dataChunks = [];

};

PNGReader.prototype.readBytes = function(length){
	var end = this.i + length;
	if (end > this.bytes.length){
		throw new Error('Unexpectedly reached end of file');
	}
	var bytes = slice.call(this.bytes, this.i, end);
	this.i = end;
	return bytes;
};

/**
 * http://www.w3.org/TR/2003/REC-PNG-20031110/#5PNG-file-signature
 */
PNGReader.prototype.decodeHeader = function(){

	if (this.i !== 0){
		throw new Error('file pointer should be at 0 to read the header');
	}

	var header = this.readBytes(8);

	if (!equalBytes(header, [0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A])){
		throw new Error('invalid PNGReader file (bad signature)');
	}

	this.header = header;

};

/**
 * http://www.w3.org/TR/2003/REC-PNG-20031110/#5Chunk-layout
 *
 * length =  4      bytes
 * type   =  4      bytes (IHDR, PLTE, IDAT, IEND or others)
 * chunk  =  length bytes
 * crc    =  4      bytes
 */
PNGReader.prototype.decodeChunk = function(){

	var length = readUInt32(this.readBytes(4), 0);

	if (length < 0){
		throw new Error('Bad chunk length ' + (0xFFFFFFFF & length));
	}

	var type = bufferToString(this.readBytes(4