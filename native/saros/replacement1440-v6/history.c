/* Exact-build history maintenance. Uses the original title's texture extraction
 * function 0xb6bc36 with flags 0; no existing graph work is edited. */
typedef unsigned long ptr;typedef unsigned char byte;
#define P(p,n) (*(void**)((byte*)(p)+(n)))
#define U(p,n) (*(unsigned*)((byte*)(p)+(n)))
#define Q(p,n) (*(ptr*)((byte*)(p)+(n)))
extern const byte ht_module[] __attribute__((visibility("hidden")));
extern void hx_extract_texture(void*,void*,void**,unsigned) __attribute__((visibility("hidden")));
int hx_supported(void *v,void *in){
 if(((byte*)v)[0xcf48]&1||U(in,0)!=0x10001||((byte*)in)[4]||P(in,0xb0)||P(in,0xb8)||P(v,0xd890))return 0;
 /* r.TSR.Support.LensDistortion: 0 off, 1 desktop only, 2 all platforms. */
 void *lens=P(ht_module,0x8f829b0);
 return lens&&U(lens,0)<2&&U(lens,4)<2;
}
static void release(void **target){void*p=*target;if(p){*target=0;(*(void(**)(void*))((byte*)P(p,0)+0x40))(p);}}
void hx_history(void*b,byte*v,void*texture){
 byte *key=P(v,0x2710);
 /* Old stock history remains invalid, so the engine will reset if stock returns. */
 for(unsigned i=0;i<3;i++)release((void**)(key+0x1008+i*8));
 U(key,0x1054)=0;
 hx_extract_texture(b,texture,(void**)(key+0xfd8),0);
 Q(key,0xfe8)=3840UL|(2160UL<<32);Q(key,0xff0)=0;Q(key,0xff8)=3840UL|(2160UL<<32);U(key,0x1000)=0;
}
