var BLAKE2s=function(){function t(t){var i=Object.prototype.toString.call(t);return"[object Uint8Array]"===i||"[object Array]"===i}function i(t,i){return 255&t[i+0]|(255&t[i+1])<<8|(255&t[i+2])<<16|(255&t[i+3])<<24}function s(s,f){if(void 0===s&&(s=r),s<=0||s>r)throw new Error("bad digestLength");this.digestLength=s;var u,c,p,y=0;if(t(f))y=(u=f).length;else if("object"==typeof f)!function(i){for(var s in i)switch(s){case"key":case"personalization":case"salt":if(!t(i[s]))throw new TypeError(s+" must be a Uint8Array or an Array of bytes");break;d