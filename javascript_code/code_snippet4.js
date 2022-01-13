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
	