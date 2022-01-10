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
	for (var l = a.len