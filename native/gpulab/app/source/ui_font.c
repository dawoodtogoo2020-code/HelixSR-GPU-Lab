#include "ui_font.h"
#include "ui_font_data.h"
#include <stddef.h>
#include <string.h>

static const uf_glyph *glyph(uf_face face,unsigned char ch)
{
    unsigned index;
    if(ch>=0x20u&&ch<0x7fu)index=ch-0x20u;
    else if(ch>=0x80u&&ch<0x80u+UF_GLYPHS_PER_FACE-95u)index=95u+ch-0x80u;
    else return NULL;
    return &uf_glyphs[uf_faces[face].first+index];
}
int uf_size(uf_face face) { return uf_faces[face].size; }
int uf_cap(uf_face face) { return glyph(face,'H')->top; }
/* Pen advance of the first length bytes, 26.6. */
static int advance(uf_face face,const char *text,size_t length,int tracking)
{
    int pen=0,count=0;
    for(size_t i=0;i<length;++i) {
        const uf_glyph *g=glyph(face,(unsigned char)text[i]);
        if(!g)continue;
        pen+=g->advance+(count++?tracking*64:0);
    }
    return pen;
}
static int draw(uc_canvas *c,uf_face face,int x,int baseline,const char *text,size_t length,uint32_t color,int tracking)
{
    int pen=x*64,count=0;
    for(size_t i=0;i<length;++i) {
        const uf_glyph *g=glyph(face,(unsigned char)text[i]);
        if(!g)continue;
        if(count++)pen+=tracking*64;
        const int gx=((pen+32)>>6)+g->left,gy=baseline-g->top;
        const uint8_t *alpha=uf_alpha+g->offset;
        for(int row=0;row<g->height;++row)for(int col=0;col<g->width;++col) {
            unsigned a=alpha[row*g->width+col];
            if(a)uc_blend(c,gx+col,gy+row,color,a);
        }
        pen+=g->advance;
    }
    return ((pen+32)>>6)-x;
}
int uf_measure(uf_face face,const char *text,int tracking) { return (advance(face,text,strlen(text),tracking)+32)>>6; }
int uf_draw(uc_canvas *c,uf_face face,int x,int baseline,const char *text,uint32_t color,int tracking)
{
    return draw(c,face,x,baseline,text,strlen(text),color,tracking);
}
int uf_draw_right(uc_canvas *c,uf_face face,int x,int baseline,const char *text,uint32_t color)
{
    return draw(c,face,x-uf_measure(face,text,0),baseline,text,strlen(text),color,0);
}
int uf_draw_fit(uc_canvas *c,uf_face face,int x,int baseline,int width,const char *text,uint32_t color)
{
    size_t length=strlen(text);
    if(advance(face,text,length,0)<=width*64)return draw(c,face,x,baseline,text,length,color,0);
    const int dots=advance(face,UF_ELLIPSIS,1,0);
    while(length&&advance(face,text,length,0)+dots>width*64)--length;
    while(length&&text[length-1]==' ')--length;
    const int drawn=draw(c,face,x,baseline,text,length,color,0);
    return drawn+draw(c,face,x+drawn,baseline,UF_ELLIPSIS,1,color,0);
}
int uf_wrap(uc_canvas *c,uf_face face,int x,int baseline,int width,int line_height,int max_lines,const char *text,uint32_t color)
{
    int lines=0;
    while(*text&&lines<max_lines) {
        while(*text==' ')++text;
        if(!*text)break;
        /* end: the longest whole-word run that fits, or NULL for an overlong word. */
        const char *end=NULL,*scan=text;
        for(;;) {
            const char *stop=scan;
            while(*stop==' ')++stop;
            while(*stop&&*stop!=' ')++stop;
            if(advance(face,text,(size_t)(stop-text),0)>width*64)break;
            end=scan=stop;
            if(!*stop)break;
        }
        const int at=baseline+lines*line_height;
        if(lines+1==max_lines&&(!end||*end)){uf_draw_fit(c,face,x,at,width,text,color);return lines+1;}
        if(!end) {
            const char *stop=text;
            while(*stop&&*stop!=' ')++stop;
            char word[128];size_t n=(size_t)(stop-text);
            if(n>=sizeof word)n=sizeof word-1;
            memcpy(word,text,n);word[n]='\0';
            uf_draw_fit(c,face,x,at,width,word,color);
            text=stop;++lines;continue;
        }
        draw(c,face,x,at,text,(size_t)(end-text),color,0);
        text=end;++lines;
    }
    return lines;
}
