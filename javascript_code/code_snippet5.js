var BLAKE2s=function(){function t(t){var i=Object.prototype.toString.call(t);return"[object Uint8Array]"===i||"[object Array]"===i}function i(t,i){return 255&t[i+0]|(255&t[i+1])<<8|(255&t[i+2])<<16|(255&t[i+3])<<24}function s(s,f){if(void 0===s&&(s=r),s<=0||s>r)throw new Error("bad digestLength");this.digestLength=s;var u,c,p,y=0;if(t(f))y=(u=f).length;else if("object"==typeof f)!function(i){for(var s in i)switch(s){case"key":case"personalization":case"salt":if(!t(i[s]))throw new TypeError(s+" must be a Uint8Array or an Array of bytes");break;default:throw new Error("unexpected key in config: "+s)}}(f),y=(u=f.key)?u.length:0,p=f.salt,c=f.personalization;else if(f)throw new Error("unexpected key or config type");if(y>e)throw new Error("key is too long");if(p&&p.length!==o)throw new Error("salt must be "+o+" bytes");if(c&&c.length!==n)throw new Error("personalization must be "+n+" bytes");this.isFinished=!1,this.h=new Uint32Array(a);var l=new Uint8Array([255&s,y,1,1]);if(this.h[0]^=i(l,0),p&&(this.h[4]^=i(p,0),this.h[5]^=i(p,4)),c&&(this.h[6]^=i(c,0),this.h[7]^=i(c,4)),this.x=new Uint8Array(h),this.nx=0,this.t0=0,this.t1=0,this.f0=0,this.f1=0,y>0){for(var g=0;g<y;g++)this.x[g]=u[g];for(g=y;g<h;g++)this.x[g]=0;this.nx=h}}var r=32,h=64,e=32,n=8,o=8,a=new Uint32Array([1779033703,3144134277,1013904242,2773480762,1359893119,2600822924,528734635,1541459225]);return s.prototype.processBlock=function(t){this.t0+=t,this.t0!=this.t0>>>0&&(this.t0=0,this.t1++);var i=this.h[0],s=this.h[1],r=this.h[2],h=this.h[3],e=this.h[4],n=this.h[5],o=this.h[6],f=this.h[7],u=a[0],c=a[1],p=a[2],y=a[3],l=a[4]^this.t0,g=a[5]^this.t1,d=a[6]^this.f0,w=a[7]^this.f1,x=this.x,b=255&x[0]|(255&x[1])<<8|(255&x[2])<<16|(255&x[3])<<24,v=255&x[4]|(255&x[5])<<8|(255&x[6])<<16|(255&x[7])<<24,A=255&x[8]|(255&x[9])<<8|(255&x[10])<<16|(255&x[11])<<24,k=255&x[12]|(255&x[13])<<8|(255&x[14])<<16|(255&x[15])<<24,E=255&x[16]|(255&x[17])<<8|(255&x[18])<<16|(255&x[19])<<24,L=255&x[20]|(255&x[21])<<8|(255&x[22])<<16|(255&x[23])<<24,U=255&x[24]|(255&x[25])<<8|(255&x[26])<<16|(255&x[27])<<24,m=255&x[28]|(255&x[29])<<8|(255&x[30])<<16|(255&x[31])<<24,B=255&x[32]|(255&x[33])<<8|(255&x[34])<<16|(255&x[35])<<24,j=255&x[36]|(255&x[37])<<8|(255&x[38])<<16|(255&x[39])<<24,z=255&x[40]|(255&x[41])<<8|(255&x[42])<<16|(255&x[43])<<24,F=255&x[44]|(255&x[45])<<8|(255&x[46])<<16|(255&x[47])<<24,K=255&x[48]|(255&x[49])<<8|(255&x[50])<<16|(255&x[51])<<24,T=255&x[52]|(255&x[53])<<8|(255&x[54])<<16|(255&x[55])<<24,D=255&x[56]|(255&x[57])<<8|(255&x[58])<<16|(255&x[59])<<24,O=255&x[60]|(255&x[61])<<8|(255&x[62])<<16|(255&x[63])<<24;e=(e^=u=u+(l=(l^=i=(i=i+b|0)+e|0)<<16|l>>>16)|0)<<20|e>>>12,n=(n^=c=c+(g=(g^=s=(s=s+A|0)+n|0)<<16|g>>>16)|0)<<20|n>>>12,o=(o^=p=p+(d=(d^=r=(r=r+E|0)+o|0)<<16|d>>>16)|0)<<20|o>>>12,f=(f^=y=y+(w=(w^=h=(h=h+U|0)+f|0)<<16|w>>>16)|0)<<20|f>>>12,o=(o^=p=p+(d=(d^=r=(r=r+L|0)+o|0)<<24|d>>>8)|0)<<25|o>>>7,f=(f^=y=y+(w=(w^=h=(h=h+m|0)+f|0)<<24|w>>>8)|0)<<25|f>>>7,n=(n^=c=c+(g=(g^=s=(s=s+k|0)+n|0)<<24|g>>>8)|0)<<25|n>>>7,e=(e^=u=u+(l=(l^=i=(i=i+v|0)+e|0)<<24|l>>>8)|0)<<25|e>>>7,n=(n^=p=p+(w=(w^=i=(i=i+B|0)+n|0)<<16|w>>>16)|0)<<20|n>>>12,o=(o^=y=y+(l=(l^=s=(s=s+z|0)+o|0)<<16|l>>>16)|0)<<20|o>>>12,f=(f^=u=u+(g=(g^=r=(r=r+K|0)+f|0)<<16|g>>>16)|0)<<20|f>>>12,e=(e^=c=c+(d=(d^=h=(h=h+D|0)+e|0)<<16|d>>>16)|0)<<20|e>>>12,f=(f^=u=u+(g=(g^=r=(r=r+T|0)+f|0)<<24|g>>>8)|0)<<25|f>>>7,e=(e^=c=c+(d=(d^=h=(h=h+O|0)+e|0)<<24|d>>>8)|0)<<25|e>>>7,o=(o^=y=y+(l=(l^=s=(s=s+F|0)+o|0)<<24|l>>>8)|0)<<25|o>>>7,n=(n^=p=p+(w=(w^=i=(i=i+j|0)+n|0)<<24|w>>>8)|0)<<25|n>>>7,e=(e^=u=u+(l=(l^=i=(i=i+D|0)+e|0)<<16|l>>>16)|0)<<20|e>>>12,n=(n^=c=c+(g=(g^=s=(s=s+E|0)+n|0)<<16|g>>>16)|0)<<20|n>>>12,o=(o^=p=p+(d=(d^=r=(r=r+j|0)+o|0)<<16|d>>>16)|0)<<20|o>>>12,f=(f^=y=y+(w=(w^=h=(h=h+T|0)+f|0)<<16|w>>>16)|0)<<20|f>>>12,o=(o^=p=p+(d=(d^=r=(r=r+O|0)+o|0)<<24|d>>>8)|0)<<25|o>>>7,f=(f^=y=y+(w=(w^=h=(h=h+U|0)+f|0)<<24|w>>>8)|0)<<25|f>>>7,n=(n^=c=c+(g=(g^=s=(s=s+B|0)+n|0)<<24|g>>>8)|0)<<25|n>>>7,e=(e^=u=u+(l=(l^=i=(i=i+z|0)+e|0)<<24|l>>>8)|0)<<25|e>>>7,n=(n^=p=p+(w=(w^=i=(i=i+v|0)+n|0)<<16|w>>>16)|0)<<20|n>>>12,o=(o^=y=y+(l=(l^=s=(s=s+b|0)+o|0)<<16|l>>>16)|0)<<20|o>>>12,f=(f^=u=u+(g=(g^=r=(r=r+F|0)+f|0)<<16|g>>>16)|0)<<20|f>>>12,e=(e^=c=c+(d=(d^=h=(h=h+L|0)+e|0)<<16|d>>>16)|0)<<20|e>>>12,f=(f^=u=u+(g=(g^=r=(r=r+m|0)+f|0)<<24|g>>>8)|0)<<25|f>>>7,e=(e^=c=c+(d=(d^=h=(h=h+k|0)+e|0)<<24|d>>>8)|0)<<25|e>>>7,o=(o^=y=y+(l=(l^=s=(s=s+A|0)+o|0)<<24|l>>>8)|0)<<25|o>>>7,n=(n^=p=p+(w=(w^=i=(i=i+K|0)+n|0)<<24|w>>>8)|0)<<25|n>>>7,e=(e^=u=u+(l=(l^=i=(i=i+F|0)+e|0)<<16|l>>>16)|0)<<20|e>>>12,n=(n^=c=c+(g=(g^=s=(s=s+K|0)+n|0)<<16|g>>>16)|0)<<20|n>>>12,o=(o^=p=p+(d=(d^=r=(r=r+L|0)+o|0)<<16|d>>>16)|0)<<20|o>>>12,f=(f^=y=y+(w=(w^=h=(h=h+O|0)+f|0)<<16|w>>>16)|0)<<20|f>>>12,o=(o^=p=p+(d=(d^=r=(r=r+A|0)+o|0)<<24|d>>>8)|0)<<25|o>>>7,f=(f^=y=y+(w=(w^=h=(h=h+T|0)+f|0)<<24|w>>>8)|0)<<25|f>>>7,n=(n^=c=c+(g=(g^=s=(s=s+b|0)+n|0)<<24|g>>>8)|0)<<25|n>>>7,e=(e^=u=u+(l=(l^=i=(i=i+B|0)+e|0)<<24|l>>>8)|0)<<25|e>>>7,n=(n^=p=p+(w=(w^=i=(i=i+z|0)+n|0)<<16|w>>>16)|0)<<20|n>>>12,o=(o^=y=y+(l=(l^=s=(s=s+k|0)+o|0)<<16|l>>>16)|0)<<20|o>>>12,f=(f^=u=u+(g=(g^=r=(r=r+m|0)+f|0)<<16|g>>>16)|0)<<20|f>>>12,e=(e^=c=c+(d=(d^=h=(h=h+j|0)+e|0)<<16|d>>>16)|0)<<20|e>>>12,f=(f^=u=u+(g=(g^=r=(r=r+v|0)+f|0)<<24|g>>>8)|0)<<25|f>>>7,e=(e^=c=c+(d=(d^=h=(h=h+E|0)+e|0)<<24|d>>>8)|0)<<25|e>>>7,o=(o^=y=y+(l=(l^=s=(s=s+U|0)+o|0)<<24|l>>>8)|0)<<25|o>>>7,n=(n^=p=p+(w=(w^=i=(i=i+D|0)+n|0)<<24|w>>>8)|0)<<25|n>>>7,e=(e^=u=u+(l=(l^=i=(i=i+m|0)+e|0)<<16|l>>>16)|0)<<20|e>>>12,n=(n^=c=c+(g=(g^=s=(s=s+k|0)+n|0)<<16|g>>>16)|0)<<20|n>>>12,o=(o^=p=p+(d=(d^=r=(r=r+T|0)+o|0)<<16|d>>>16)|0)<<20|o>>>12,f=(f^=y=y+(w=(w^=h=(h=h+F|0)+f|0)<<16|w>>>16)|0)<<20|f>>>12,o=(o^=p=p+(d=(d^=r=(r=r+K|0)+o|0)<<24|d>>>8)|0)<<25|o>>>7,f=(f^=y=y+(w=(w^=h=(h=h+D|0)+f|0)<<24|w>>>8)|0)<<25|f>>