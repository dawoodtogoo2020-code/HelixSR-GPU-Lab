/* Renders the 512x512 home screen icon (sce_sys/icon0.png) with the app's own
 * mark and faces. Build and run with tools/icon.sh. */
#include "ui_canvas.h"
#include "ui_font.h"
#include "ui_screen.h"
#include <stdio.h>

enum { SIZE=512 };
static uint32_t pixels[SIZE*SIZE];

int main(int argc,char **argv)
{
    if(argc!=2)return 2;
    uc_canvas c;uc_init(&c,pixels,SIZE,SIZE,SIZE);
    uc_vgradient(&c,0,0,SIZE,SIZE,0xff121b34u,0xff070a12u);
    uc_glow(&c,70.0f,30.0f,440.0f,0x504d9dffu);
    uc_glow(&c,480.0f,520.0f,420.0f,0x408b73ffu);
    us_draw_mark(&c,146.0f,84.0f,220.0f);
    const char *name="HelixSR",*role="GPU Lab";
    uf_draw(&c,UF_HERO,(SIZE-uf_measure(UF_HERO,name,0))/2,404,name,0xffeef2f8u,0);
    uf_draw(&c,UF_TITLE,(SIZE-uf_measure(UF_TITLE,role,0))/2,450,role,0xffa3aec2u,0);
    FILE *f=fopen(argv[1],"wb");
    if(!f)return 1;
    fprintf(f,"P6\n%d %d\n255\n",SIZE,SIZE);
    for(int i=0;i<SIZE*SIZE;++i){unsigned char rgb[3]={(unsigned char)(pixels[i]>>16),(unsigned char)(pixels[i]>>8),(unsigned char)pixels[i]};fwrite(rgb,1,3,f);}
    return fclose(f)!=0;
}
