var BLAKE2s=function(){function t(t){var i=Object.prototype.toString.call(t);return"[object Uint8Array]"===i||"[object Array]"===i}function i(t,i){return 255&t[i+0]|(255&t[i+1])<<8|(255&t[i+2])<<16|(255&t[i+3])<<24}function s(s,f){if(void 0===s&&(s=r),s<=0||s>r)throw new Error("bad digestLength");this.digestLength=s;var u,c,p,y=0;if(t(f))y=(u=f).length;else if("object"==typeof f)!function(i){for(var s in i)switch(s){case"key":case"personalization":case"salt":if(!t(i[s]))throw new TypeError(s+" must be a Uint8Array or an Array of bytes");break;default:throw new Error("unexpected key in config: "+s)}}(f),y=(u=f.key)?u.length:0,p=f.salt,c=f.personalization;else if(f)throw new Error("unexpected key or config type");if(y>e)throw new Error("key is too long");if(p&&p.length!==o)throw new Error("salt must be "+o+" bytes");if(c&&c.length!==n)throw new Error("personalization must be "+n+" bytes");this.isFinished=!1,this.h=new Uint32Array(a);var l=new Uint8Array([255&s,y,1,1]);if(this.h[0]^=i(l,0),p&&(this.h[4]^=i(p,0),this.h[5]^=i(p,4)),c&&(this.h[6]^=i(c,0),this.h[7]^=i(c,4)),this.x=new Uint8Array(h),this.nx=0,this.t0=0,this.t1=0,this.f0=0,this.f1=0,y>0){for(var g=0;g<y;g++)this.x[g]=u[g];for(g=y;g<h;g++)this.x[g]=0;this.nx=h}}var r=32,h=64,e=32,n=8,o=8,a=new Uint32Array([1779033703,3144134277,1013904242,2773480762,1359893119,2600822924,528734635,1541459225]);return s.prototype.processBlock=function(t){this.t0+=t,this.t0!=this.t0>>>0&&(this.t0=0,this.t1++);var i=this.h[0],s=this.h[1],r=this.h[2],h=this.h[3],e=this.h[4],n=this.h[5],o=this.h[6],f=this.h[7],u=a[0],c=a[1],p=a[2],y=a[3],l=a[4]^this.t0,g=a[5]^this.t1,d=a[6]^this.f0,w=a[7]^this.f1,x=this.x,b=255&x[0]|(255&x[1])<<8|(255&x[2])<<16|(255&x[3])<<24,v=255&x[4]|(255&x[5])<<8|(255&x[6])<<16|(255&x[7])<<24,A=255&x[8]|(255&x[9])<<8|(255&x[10])<<16|(255&x[11])<<24,k=255&x[12]|(255&x[13])<<8|(255&x[14])<<16|(255&x[15])<<24,E=255&x[16]|(255&x[17])<<8|(255&x[18])<<16|(255&x[19])<<24,L=255&x[20]|(255&x[21])<<8|(255&x[22])<<16|(255&x[23])<<24,U=255&x[24]|(255&x[25])<<8|(255&x[26])<<16|(255&x[27])<<24,m=255&x[28]|(255&x[29])<<8|(255&x[30])<<16|(255&x[31])<<24,B=255&x[32]|(255&x[