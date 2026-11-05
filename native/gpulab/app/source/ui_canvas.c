#include "ui_canvas.h"
#include <string.h>

float uc_sqrt(float value)
{
    if(!(value>0.0f))return 0.0f;
    __asm__("sqrtss %1, %0":"=x"(value):"x"(value));
    return value;
}
float uc_sin_turn(float turns)
{
    /* Range-reduced to [-0.5, 0.5) turn, then Bhaskara's rational form. */
    float t=turns-(float)(long)turns;
    if(t<-0.5f)t+=1.0f;
    if(t>=0.5f)t-=1.0f;
    int negative=t<0.0f;
    float a=(negative?-t:t)*360.0f,v=4.0f*a*(180.0f-a)/(40500.0f-a*(180.0f-a));
    return negative?-v:v;
}
static float absf(float v) { return v<0.0f?-v:v; }
static float maxf(float a,float b) { return a>b?a:b; }
static float minf(float a,float b) { return a<b?a:b; }
static float clamp01(float v) { return v<0.0f?0.0f:v>1.0f?1.0f:v; }
static int floori(float v) { int i=(int)v;return (float)i>v?i-1:i; }
static int ceili(float v) { int i=(int)v;return (float)i<v?i+1:i; }

void uc_init(uc_canvas *c,uint32_t *pixels,int width,int height,int stride)
{
    c->pixels=pixels;c->width=width;c->height=height;c->stride=stride;uc_unclip(c);
}
void uc_unclip(uc_canvas *c)
{
    c->clip_x0=0;c->clip_y0=0;c->clip_x1=c->width;c->clip_y1=c->height;
}
void uc_clip(uc_canvas *c,int x,int y,int width,int height)
{
    uc_unclip(c);
    if(x>c->clip_x0)c->clip_x0=x;
    if(y>c->clip_y0)c->clip_y0=y;
    if(x+width<c->clip_x1)c->clip_x1=x+width;
    if(y+height<c->clip_y1)c->clip_y1=y+height;
    if(c->clip_x1<c->clip_x0)c->clip_x1=c->clip_x0;
    if(c->clip_y1<c->clip_y0)c->clip_y1=c->clip_y0;
}
void uc_copy(uc_canvas *c,const uint32_t *source)
{
    memcpy(c->pixels,source,(size_t)c->stride*(size_t)c->height*4u);
}
uint32_t uc_alpha(uint32_t color,unsigned alpha)
{
    return (color&0xffffffu)|((((color>>24)*alpha+127u)/255u)<<24);
}
uint32_t uc_mix(uint32_t from,uint32_t to,unsigned amount)
{
    uint32_t out=0;
    for(unsigned shift=0;shift<32;shift+=8) {
        unsigned a=(from>>shift)&255u,b=(to>>shift)&255u;
        out|=((a*(255u-amount)+b*amount+127u)/255u)<<shift;
    }
    return out;
}
static inline void blend(uint32_t *d,uint32_t color,unsigned coverage)
{
    unsigned a=((color>>24)*coverage+127u)/255u;
    if(!a)return;
    if(a>=255u){*d=color|0xff000000u;return;}
    uint32_t dst=*d,out=0xff000000u;
    for(unsigned shift=0;shift<24;shift+=8) {
        unsigned s=(color>>shift)&255u,v=(dst>>shift)&255u;
        out|=((s*a+v*(255u-a)+127u)/255u)<<shift;
    }
    *d=out;
}
void uc_blend(uc_canvas *c,int x,int y,uint32_t color,unsigned coverage)
{
    if(x<c->clip_x0||y<c->clip_y0||x>=c->clip_x1||y>=c->clip_y1)return;
    blend(c->pixels+(size_t)y*(size_t)c->stride+(size_t)x,color,coverage>255u?255u:coverage);
}
/* Integer pixel span intersected with the clip, empty when x1<=x0. */
static int span(const uc_canvas *c,float fx0,float fy0,float fx1,float fy1,int *x0,int *y0,int *x1,int *y1)
{
    *x0=floori(fx0);*y0=floori(fy0);*x1=ceili(fx1);*y1=ceili(fy1);
    if(*x0<c->clip_x0)*x0=c->clip_x0;
    if(*y0<c->clip_y0)*y0=c->clip_y0;
    if(*x1>c->clip_x1)*x1=c->clip_x1;
    if(*y1>c->clip_y1)*y1=c->clip_y1;
    return *x1>*x0&&*y1>*y0;
}
void uc_rect(uc_canvas *c,int x,int y,int width,int height,uint32_t color)
{
    int x0,y0,x1,y1;
    if(!span(c,(float)x,(float)y,(float)(x+width),(float)(y+height),&x0,&y0,&x1,&y1))return;
    for(int py=y0;py<y1;++py) {
        uint32_t *row=c->pixels+(size_t)py*(size_t)c->stride;
        for(int px=x0;px<x1;++px)blend(row+px,color,255u);
    }
}
void uc_vgradient(uc_canvas *c,int x,int y,int width,int height,uint32_t top,uint32_t bottom)
{
    int x0,y0,x1,y1;
    if(height<=0||!span(c,(float)x,(float)y,(float)(x+width),(float)(y+height),&x0,&y0,&x1,&y1))return;
    for(int py=y0;py<y1;++py) {
        uint32_t color=uc_mix(top,bottom,(unsigned)((py-y)*255/(height>1?height-1:1)));
        uint32_t *row=c->pixels+(size_t)py*(size_t)c->stride;
        for(int px=x0;px<x1;++px)blend(row+px,color,255u);
    }
}
/* Signed distance to a rounded box centred at (cx, cy), half size hx, hy. */
static float box_distance(float px,float py,float cx,float cy,float hx,float hy,float r)
{
    float qx=absf(px-cx)-hx+r,qy=absf(py-cy)-hy+r;
    float ox=maxf(qx,0.0f),oy=maxf(qy,0.0f);
    return uc_sqrt(ox*ox+oy*oy)+minf(maxf(qx,qy),0.0f)-r;
}
enum { FILL=0, STROKE, SHADOW, GLOW };
static float coverage(int mode,float d,float t)
{
    if(mode==FILL)return clamp01(0.5f-d);
    if(mode==STROKE)return clamp01(0.5f-(absf(d+t*0.5f)-t*0.5f));
    if(mode==GLOW){float s=clamp01(1.0f-d/t);return clamp01(d+0.5f)*s*s;}
    float s=clamp01((t-d)/(2.0f*t));
    return s*s*(3.0f-2.0f*s);
}
/* Rows of a rounded box. Pixels farther than the radius plus the effect width
 * from both side edges share the distance of the row centre, so only the side
 * bands are evaluated per pixel. */
static void box(uc_canvas *c,float x,float y,float w,float h,float r,int mode,float t,uint32_t color)
{
    if(w<=0.0f||h<=0.0f)return;
    float hx=w*0.5f,hy=h*0.5f,cx=x+hx,cy=y+hy;
    r=minf(maxf(r,0.0f),minf(hx,hy));
    float grow=mode==SHADOW||mode==GLOW?t:1.0f;
    int x0,y0,x1,y1;
    if(!span(c,x-grow,y-grow,x+w+grow,y+h+grow,&x0,&y0,&x1,&y1))return;
    float band=r+(mode==FILL?1.0f:t)+2.0f;
    int inner0=ceili(x+band),inner1=floori(x+w-band);
    for(int py=y0;py<y1;++py) {
        uint32_t *row=c->pixels+(size_t)py*(size_t)c->stride;
        float fy=(float)py+0.5f;
        unsigned middle=(unsigned)(coverage(mode,box_distance(cx,fy,cx,cy,hx,hy,r),t)*255.0f+0.5f);
        for(int px=x0;px<x1;++px) {
            unsigned a;
            if(px>=inner0&&px<inner1) {
                if(!middle){px=inner1-1;continue;}
                a=middle;
            } else a=(unsigned)(coverage(mode,box_distance((float)px+0.5f,fy,cx,cy,hx,hy,r),t)*255.0f+0.5f);
            if(a)blend(row+px,color,a);
        }
    }
}
void uc_round_rect(uc_canvas *c,float x,float y,float w,float h,float r,uint32_t color) { box(c,x,y,w,h,r,FILL,0.0f,color); }
void uc_round_stroke(uc_canvas *c,float x,float y,float w,float h,float r,float t,uint32_t color) { box(c,x,y,w,h,r,STROKE,t,color); }
void uc_shadow(uc_canvas *c,float x,float y,float w,float h,float r,float blur,uint32_t color) { box(c,x,y,w,h,r,SHADOW,blur>0.5f?blur:0.5f,color); }
void uc_outer_glow(uc_canvas *c,float x,float y,float w,float h,float r,float blur,uint32_t color) { box(c,x,y,w,h,r,GLOW,blur>0.5f?blur:0.5f,color); }
void uc_round_gradient(uc_canvas *c,float x,float y,float w,float h,float r,uint32_t from,uint32_t to)
{
    if(w<=0.0f||h<=0.0f)return;
    float hx=w*0.5f,hy=h*0.5f,cx=x+hx,cy=y+hy;
    r=minf(maxf(r,0.0f),minf(hx,hy));
    int x0,y0,x1,y1;
    if(!span(c,x-1.0f,y-1.0f,x+w+1.0f,y+h+1.0f,&x0,&y0,&x1,&y1))return;
    for(int py=y0;py<y1;++py)for(int px=x0;px<x1;++px) {
        float fx=(float)px+0.5f,fy=(float)py+0.5f;
        unsigned a=(unsigned)(clamp01(0.5f-box_distance(fx,fy,cx,cy,hx,hy,r))*255.0f+0.5f);
        if(!a)continue;
        unsigned amount=(unsigned)(clamp01(((fx-x)/w+(fy-y)/h)*0.5f)*255.0f+0.5f);
        blend(c->pixels+(size_t)py*(size_t)c->stride+(size_t)px,uc_mix(from,to,amount),a);
    }
}
static void disc(uc_canvas *c,float cx,float cy,float radius,float t,int ring,uint32_t color)
{
    int x0,y0,x1,y1;
    if(!span(c,cx-radius-1.0f,cy-radius-1.0f,cx+radius+1.0f,cy+radius+1.0f,&x0,&y0,&x1,&y1))return;
    for(int py=y0;py<y1;++py)for(int px=x0;px<x1;++px) {
        float dx=(float)px+0.5f-cx,dy=(float)py+0.5f-cy,d=uc_sqrt(dx*dx+dy*dy)-radius;
        float a=ring?clamp01(0.5f-(absf(d+t*0.5f)-t*0.5f)):clamp01(0.5f-d);
        if(a>0.0f)blend(c->pixels+(size_t)py*(size_t)c->stride+(size_t)px,color,(unsigned)(a*255.0f+0.5f));
    }
}
void uc_circle(uc_canvas *c,float cx,float cy,float radius,uint32_t color) { disc(c,cx,cy,radius,0.0f,0,color); }
void uc_ring(uc_canvas *c,float cx,float cy,float radius,float t,uint32_t color) { disc(c,cx,cy,radius,t,1,color); }
void uc_line(uc_canvas *c,float ax,float ay,float bx,float by,float t,uint32_t color)
{
    float r=t*0.5f;int x0,y0,x1,y1;
    if(!span(c,minf(ax,bx)-r-1.0f,minf(ay,by)-r-1.0f,maxf(ax,bx)+r+1.0f,maxf(ay,by)+r+1.0f,&x0,&y0,&x1,&y1))return;
    float vx=bx-ax,vy=by-ay,length2=vx*vx+vy*vy;
    for(int py=y0;py<y1;++py)for(int px=x0;px<x1;++px) {
        float wx=(float)px+0.5f-ax,wy=(float)py+0.5f-ay;
        float k=length2>0.0f?clamp01((wx*vx+wy*vy)/length2):0.0f;
        float dx=wx-vx*k,dy=wy-vy*k,a=clamp01(0.5f-(uc_sqrt(dx*dx+dy*dy)-r));
        if(a>0.0f)blend(c->pixels+(size_t)py*(size_t)c->stride+(size_t)px,color,(unsigned)(a*255.0f+0.5f));
    }
}
void uc_glow(uc_canvas *c,float cx,float cy,float radius,uint32_t color)
{
    int x0,y0,x1,y1;
    if(radius<=0.0f||!span(c,cx-radius,cy-radius,cx+radius,cy+radius,&x0,&y0,&x1,&y1))return;
    float inverse=1.0f/(radius*radius);
    for(int py=y0;py<y1;++py)for(int px=x0;px<x1;++px) {
        float dx=(float)px+0.5f-cx,dy=(float)py+0.5f-cy,k=1.0f-(dx*dx+dy*dy)*inverse;
        if(k>0.0f)blend(c->pixels+(size_t)py*(size_t)c->stride+(size_t)px,color,(unsigned)(k*k*255.0f+0.5f));
    }
}
