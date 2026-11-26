/* UAPlay native TV visual system: 24-unit vector icons, theme-aware
   surfaces and a single deferred, animated focus ring. No glyph font needed. */
typedef enum { I_NONE,I_SEARCH,I_GEAR,I_BACK,I_NEXT,I_PLAY,I_PAUSE,I_PLUS,
 I_CHECK,I_CLOSE,I_STAR,I_FILM,I_TV,I_HEART,I_GLOBE,I_PALETTE,I_IMAGE,
 I_TRASH,I_AUDIO,I_CC,I_VOLUME,I_REWIND,I_FORWARD,I_CIRCLE,I_SQUARE,
 I_TRIANGLE,I_DPAD,I_UPDOWN,I_LEFTRIGHT,I_OPTIONS,I_LINK,I_SUN } UiIcon;
static SDL_Color ui_rgb(unsigned v,int alpha){return (SDL_Color){v>>16,v>>8,v,alpha};}
static SDL_Color ui_fg(void){return ui_rgb(ui_theme_enabled&&ui_light_theme?0x202a34:0xf4f6f8,255);}
static SDL_Color ui_muted(void){return ui_rgb(ui_theme_enabled&&ui_light_theme?0x526171:0xa9b8c6,255);}
typedef struct {SDL_Vertex v[1536]; int ix[4608],nv,ni;float x,y,k,s;SDL_Color c;} IconMesh;
static int im_point(IconMesh *m,float x,float y){
 int i=m->nv++;m->v[i]=(SDL_Vertex){{x,y},m->c,{0,0}};return i;
}
static void im_tri(IconMesh *m,int a,int b,int c){m->ix[m->ni++]=a;m->ix[m->ni++]=b;m->ix[m->ni++]=c;}
static void im_dot(IconMesh *m,float x,float y,float r){
 int center=im_point(m,x,y),first=m->nv;
 for(int i=0;i<10;i++){float a=i*.62831853f;im_point(m,x+r*cosf(a),y+r*sinf(a));}
 for(int i=0;i<10;i++)im_tri(m,center,first+i,first+(i+1)%10);
}
static void im_line(IconMesh *m,float x,float y,float X,float Y){
 x=m->x+x*m->k;y=m->y+y*m->k;X=m->x+X*m->k;Y=m->y+Y*m->k;
 float dx=X-x,dy=Y-y,len=sqrtf(dx*dx+dy*dy),r=m->s*.5f;
 if(len<.001f){im_dot(m,x,y,r);return;}
 float nx=-dy/len*r,ny=dx/len*r;int j=m->nv;
 im_point(m,x+nx,y+ny);im_point(m,X+nx,Y+ny);im_point(m,X-nx,Y-ny);im_point(m,x-nx,y-ny);
 im_tri(m,j,j+1,j+2);im_tri(m,j,j+2,j+3);
}
static void im_path(IconMesh *m,const float *p,int n,int close){
 for(int i=0;i<n-1;i++)im_line(m,p[2*i],p[2*i+1],p[2*i+2],p[2*i+3]);
 if(close)im_line(m,p[2*n-2],p[2*n-1],p[0],p[1]);
 for(int i=0;i<n;i++)im_dot(m,m->x+p[2*i]*m->k,m->y+p[2*i+1]*m->k,m->s*.5f);
}
static void im_ring(IconMesh *m,float x,float y,float radius){
 int j=m->nv;float r=radius*m->k;
 for(int i=0;i<32;i++){float a=i*.19634954f,co=cosf(a),si=sinf(a);
  im_point(m,m->x+x*m->k+(r+m->s*.5f)*co,m->y+y*m->k+(r+m->s*.5f)*si);
  im_point(m,m->x+x*m->k+(r-m->s*.5f)*co,m->y+y*m->k+(r-m->s*.5f)*si);}
 for(int i=0;i<32;i++){int a=j+2*i,b=j+2*((i+1)%32);im_tri(m,a,a+1,b);im_tri(m,b,a+1,b+1);}
}
static void im_fill(IconMesh *m,const float *p,int n){
 int j=m->nv;for(int i=0;i<n;i++)im_point(m,m->x+p[i*2]*m->k,m->y+p[i*2+1]*m->k);
 for(int i=1;i<n-1;i++)im_tri(m,j,j+i,j+i+1);
}
/* Supersampled coverage atlas, generated lazily from the same vector
   paths. A 3x logical mask with four samples per texel keeps fine icons
   smooth at 1080p and higher, without rasterizing icons each frame. */
typedef struct {UiIcon icon;int size;SDL_Texture *tex;Uint32 stamp;} UiIconRaster;
static UiIconRaster ui_icon_cache[96];
static Uint32 ui_icon_clock;
static SDL_Texture *ui_icon_raster(const IconMesh *m,int sz){
    int w=sz*3+12,h=w;size_t bytes=(size_t)w*h;
    Uint8 *coverage=calloc(bytes,1);
    if(!coverage)return NULL;
    for(int j=0;j<m->ni;j+=3){
        SDL_FPoint a=m->v[m->ix[j]].position,b=m->v[m->ix[j+1]].position,c=m->v[m->ix[j+2]].position;
        a.x=(a.x-m->x)*3+6;a.y=(a.y-m->y)*3+6;
        b.x=(b.x-m->x)*3+6;b.y=(b.y-m->y)*3+6;
        c.x=(c.x-m->x)*3+6;c.y=(c.y-m->y)*3+6;
        float area=(b.x-a.x)*(c.y-a.y)-(b.y-a.y)*(c.x-a.x);
        if(fabsf(area)<.001f)continue;
        int x0=(int)fmaxf(0,floorf(fminf(a.x,fminf(b.x,c.x))));
        int y0=(int)fmaxf(0,floorf(fminf(a.y,fminf(b.y,c.y))));
        int x1=(int)fminf(w-1,ceilf(fmaxf(a.x,fmaxf(b.x,c.x))));
        int y1=(int)fminf(h-1,ceilf(fmaxf(a.y,fmaxf(b.y,c.y))));
        for(int y=y0;y<=y1;y++)for(int x=x0;x<=x1;x++){
            Uint8 *mask=&coverage[y*w+x];if(*mask==15)continue;
            for(int k=0;k<4;k++){
                if(*mask&(1<<k))continue;
                float px=x+(k&1?.75f:.25f),py=y+(k&2?.75f:.25f);
                float u=(b.x-a.x)*(py-a.y)-(b.y-a.y)*(px-a.x);
                float v=(c.x-b.x)*(py-b.y)-(c.y-b.y)*(px-b.x);
                float z=(a.x-c.x)*(py-c.y)-(a.y-c.y)*(px-c.x);
                if(area>0?(u>=0&&v>=0&&z>=0):(u<=0&&v<=0&&z<=0))*mask|=(1<<k);
            }
        }
    }
    SDL_Surface *surface=SDL_CreateRGBSurfaceWithFormat(0,w,h,32,SDL_PIXELFORMAT_RGBA32);
    if(!surface){free(coverage);return NULL;}
    static const Uint8 alpha[16]={0,64,64,128,64,128,128,191,64,128,128,191,128,191,191,255};
    for(int y=0;y<h;y++){
        Uint8 *row=(Uint8*)surface->pixels+y*surface->pitch;
        for(int x=0;x<w;x++){row[4*x]=row[4*x+1]=row[4*x+2]=255;row[4*x+3]=alpha[coverage[y*w+x]];}
    }
    free(coverage);
    SDL_Texture *tex=SDL_CreateTextureFromSurface(ui_renderer,surface);SDL_FreeSurface(surface);
    if(tex){SDL_SetTextureBlendMode(tex,SDL_BLENDMODE_BLEND);SDL_SetTextureScaleMode(tex,SDL_ScaleModeLinear);}
    return tex;
}
static void ui_icon_cache_destroy(void){
    for(int j=0;j<96;j++)if(ui_icon_cache[j].tex){SDL_DestroyTexture(ui_icon_cache[j].tex);ui_icon_cache[j].tex=NULL;}
}
static void ui_icon(UiIcon icon,float cx,float cy,float size,SDL_Color c){
    int sz=(int)roundf(size);
    if(sz<=0||sz>96||icon==I_NONE)return;
    size=(float)sz;SDL_FRect dst={cx-size*.5f-2,cy-size*.5f-2,size+4,size+4};
    int slot=0;
    for(int j=0;j<96;j++){
        UiIconRaster *e=&ui_icon_cache[j];
        if(e->tex&&e->icon==icon&&e->size==sz){
            e->stamp=++ui_icon_clock;
            SDL_SetTextureColorMod(e->tex,c.r,c.g,c.b);SDL_SetTextureAlphaMod(e->tex,c.a);
            SDL_RenderCopyF(ui_renderer,e->tex,NULL,&dst);return;
        }
        if(!e->tex|| (ui_icon_cache[slot].tex&&e->stamp<ui_icon_cache[slot].stamp))slot=j;
    }
 IconMesh m;m.nv=m.ni=0;m.x=cx-size*.5f;m.y=cy-size*.5f;m.k=size/24.f;m.s=fmaxf(1.2f,size*1.8f/24.f);m.c=c;
#define P(...) do{const float q[]={__VA_ARGS__};im_path(&m,q,sizeof(q)/sizeof(float)/2,0);}while(0)
#define L(...) do{const float q[]={__VA_ARGS__};im_path(&m,q,sizeof(q)/sizeof(float)/2,1);}while(0)
#define F(...) do{const float q[]={__VA_ARGS__};im_fill(&m,q,sizeof(q)/sizeof(float)/2);}while(0)
#define C(x,y,r) im_ring(&m,x,y,r)
 switch(icon){
 case I_SEARCH:C(10,10,6.7f);P(15,15,21,21);break;
 case I_GEAR:{float q[64];for(int i=0;i<32;i++){float a=i*.19634954f,r=(i%4==1||i%4==2)?10:7.8f;q[2*i]=12+r*cosf(a);q[2*i+1]=12+r*sinf(a);}im_path(&m,q,32,1);C(12,12,3);break;}
 case I_BACK:P(15,5,8,12,15,19);break;
 case I_NEXT:P(9,5,16,12,9,19);break;
 case I_PLAY:F(8,4.5f,20,12,8,19.5f);break;
 case I_PAUSE:F(6,4,10,4,10,20,6,20);F(14,4,18,4,18,20,14,20);break;
 case I_PLUS:P(12,5,12,19);P(5,12,19,12);break;
 case I_CHECK:P(5,12,10,17,19,7);break;
 case I_CLOSE:P(6,6,18,18);P(18,6,6,18);break;
 case I_STAR:{float q[20];for(int i=0;i<10;i++){float a=-1.5707963f+i*.62831853f,r=i%2?4.4f:9.8f;q[2*i]=12+r*cosf(a);q[2*i+1]=12+r*sinf(a);}im_path(&m,q,10,1);break;}
 case I_FILM:L(3,4,21,4,21,20,3,20);P(7,4,7,20);P(17,4,17,20);P(3,9,7,9);P(3,15,7,15);P(17,9,21,9);P(17,15,21,15);break;
 case I_TV:L(3,6,21,6,21,20,3,20);P(8,2,12,6,16,2);break;
 case I_HEART:L(12,20,5,14,3.5f,10,5,6,8,5,12,8,16,5,19,6,20.5f,10,19,14);break;
 case I_GLOBE:C(12,12,9);P(3,12,21,12);P(12,3,8,7,7,12,8,17,12,21,16,17,17,12,16,7,12,3);break;
 case I_PALETTE:L(12,3,5,5,3,11,4,17,9,21,13,21,15,18,14,15,19,14,21,10,19,5);C(8,9,1.1f);C(12,6,1.1f);C(16,9,1.1f);break;
 case I_IMAGE:L(3,3,21,3,21,21,3,21);C(8,8,1.5f);P(3,18,10,11,14,15,17,12,21,16);break;
 case I_TRASH:P(3,6,21,6);P(9,6,9,3,15,3,15,6);L(6,6,18,6,17,21,7,21);P(10,10,10,17);P(14,10,14,17);break;
 case I_AUDIO:P(10,17,10,5,20,3,20,15);C(7.5f,17.5f,2.5f);C(17.5f,15.5f,2.5f);break;
 case I_CC:L(2,5,22,5,22,19,2,19);P(10,9,6,9,5,12,6,15,10,15);P(19,9,15,9,14,12,15,15,19,15);break;
 case I_VOLUME:L(3,9,7,9,12,5,12,19,7,15,3,15);P(15,9,17,12,15,15);P(18,6,21,12,18,18);break;
 case I_REWIND:P(12,5,5,12,12,19);P(20,5,13,12,20,19);break;
 case I_FORWARD:P(4,5,11,12,4,19);P(12,5,19,12,12,19);break;
 case I_CIRCLE:C(12,12,7.5f);break;
 case I_SQUARE:L(5,5,19,5,19,19,5,19);break;
 case I_TRIANGLE:L(12,4,21,19,3,19);break;
 case I_DPAD:L(9,2,15,2,15,9,22,9,22,15,15,15,15,22,9,22,9,15,2,15,2,9,9,9);break;
 case I_UPDOWN:P(6,8,12,2,18,8);P(6,16,12,22,18,16);break;
 case I_LEFTRIGHT:P(8,6,2,12,8,18);P(16,6,22,12,16,18);break;
 case I_OPTIONS:P(5,6,19,6);P(5,12,19,12);P(5,18,19,18);break;
 case I_LINK:P(10,7,14,3,19,3,21,6,21,10,17,14);P(14,17,10,21,5,21,3,18,3,14,7,10);P(8,16,16,8);break;
 case I_SUN:C(12,12,4);for(int i=0;i<8;i++){float a=i*.78539816f;im_line(&m,12+7*cosf(a),12+7*sinf(a),12+10*cosf(a),12+10*sinf(a));}break;
 default:break;
 }
#undef P
#undef L
#undef F
#undef C
 if(m.ni){
    SDL_Texture *tex=ui_icon_raster(&m,sz);
    if(tex){
        UiIconRaster *e=&ui_icon_cache[slot];if(e->tex)SDL_DestroyTexture(e->tex);
        *e=(UiIconRaster){icon,sz,tex,++ui_icon_clock};
        SDL_SetTextureColorMod(tex,c.r,c.g,c.b);SDL_SetTextureAlphaMod(tex,c.a);
        SDL_RenderCopyF(ui_renderer,tex,NULL,&dst);
    }else SDL_RenderGeometry(ui_renderer,NULL,m.v,m.nv,m.ix,m.ni);
 }
}
static void ui_center_color(Paint *p,const char *s,int x,int y,int w,int h,TTF_Font *font,SDL_Color c){
 int old=ui_theme_enabled;ui_theme_enabled=0;
 text_center_button(p,s,x,y,w,h,font,c.r/255.f,c.g/255.f,c.b/255.f);
 ui_theme_enabled=old;
}
static void ui_labeled_icon(Paint *p,UiIcon i,const char *s,int x,int y,int w,int h,SDL_Color c){
 TTF_Font *font=p->font_label?p->font_label:p->font_small;
 int sz=22,gap=8,tw=measure(p,s,font);
 int available=w-sz-gap-12;if(available<4)return;
 if(tw>available)tw=available;
 int left=x+(w-tw-sz-gap)/2;
 ui_icon(i,left+sz*.5f,y+h*.5f,sz,c);
 ui_center_color(p,s,left+sz+gap,y,tw+1,h,font,c);
}
/* Render focus last: no clipping by a subsequently drawn card. The logical
   focus switches immediately; the visual ring eases to the target. */
static struct {SDL_FRect from,visual;SDL_Rect target,pending;float from_radius,visual_radius,target_radius,pending_radius;int valid,scene,seen;Uint32 changed;} ui_focus;
static void ui_focus_begin(int scene){
 if(ui_focus.scene!=scene){ui_focus.valid=0;ui_focus.scene=scene;}
 ui_focus.seen=0;
}
static void ui_focus_sample(Uint32 now){
 float t=fminf(1.f,(now-ui_focus.changed)/175.f),f=1.f-(1.f-t)*(1.f-t)*(1.f-t);
 SDL_FRect a=ui_focus.from;SDL_Rect b=ui_focus.target;
 ui_focus.visual=(SDL_FRect){a.x+(b.x-a.x)*f,a.y+(b.y-a.y)*f,a.w+(b.w-a.w)*f,a.h+(b.h-a.h)*f};
 ui_focus.visual_radius=ui_focus.from_radius+(ui_focus.target_radius-ui_focus.from_radius)*f;
}
static void ui_focus_set(SDL_Rect r){
 /* Collect exactly one target per draw; never restart easing mid-frame. */
 ui_focus.pending=r;ui_focus.pending_radius=r.h>94?16.f:14.f;ui_focus.seen=1;
}
static void ui_focus_finish(void){
 if(!ui_focus.seen){ui_focus.valid=0;return;}
 SDL_Rect target=ui_focus.pending;
 float rad=ui_focus.pending_radius;
 Uint32 at=SDL_GetTicks();
 if(!ui_focus.valid){
  ui_focus.from=ui_focus.visual=(SDL_FRect){target.x,target.y,target.w,target.h};
  ui_focus.from_radius=ui_focus.visual_radius=ui_focus.target_radius=rad;
  ui_focus.target=target;ui_focus.changed=at;ui_focus.valid=1;
 }else if(!SDL_RectEquals(&target,&ui_focus.target)||rad!=ui_focus.target_radius){
  ui_focus_sample(at);
  ui_focus.from=ui_focus.visual;ui_focus.from_radius=ui_focus.visual_radius;
  /* Circle controls are intentional only in the player. Do not morph
     between a circular control and a rectangle. */
  int was_circle=ui_focus.target_radius>=fminf(ui_focus.target.w,ui_focus.target.h)*.5f-1;
  int is_circle=rad>=fminf(target.w,target.h)*.5f-1;
  if(was_circle!=is_circle||fabsf(ui_focus.from.y-target.y)>280||fabsf(ui_focus.from.w-target.w)>550){
   ui_focus.from=(SDL_FRect){target.x,target.y,target.w,target.h};ui_focus.from_radius=rad;
  }
  ui_focus.target=target;ui_focus.target_radius=rad;ui_focus.changed=at;
 }
 Uint32 now=SDL_GetTicks();ui_focus_sample(now);SDL_FRect v=ui_focus.visual;
 float age=(float)(now-ui_press_at);float inset=ui_press_at&&age<145?2.5f*sinf(3.14159265f*age/145.f):0;
 SDL_FRect r={v.x-3+inset,v.y-3+inset,v.w+6-2*inset,v.h+6-2*inset};
 if(r.w<10||r.h<10)return;
 float a,b,c;accent_real(ui_accent_index,&a,&b,&c);
 if(ui_theme_enabled&&ui_light_theme&&ui_accent_index==ACCENT_WHITE)a=b=c=.35f;
 SDL_Color color={color_byte(a),color_byte(b),color_byte(c),255};
 float radius=ui_focus.visual_radius+3.f-inset;
 if(radius>r.w/2)radius=r.w/2;if(radius>r.h/2)radius=r.h/2;
 SDL_FRect glow={r.x-2,r.y-2,r.w+4,r.h+4};SDL_Color halo=color;halo.a=ui_theme_enabled&&ui_light_theme?25:46;
 rounded_outline_f(glow,radius+2,2,halo);rounded_outline_f(r,radius,2,color);
}
static int ui_focus_moving(void){Uint32 now=SDL_GetTicks();return ui_focus.valid&&(now-ui_focus.changed<175||(ui_press_at&&now-ui_press_at<145));}
/* Cards sit closer to the background than buttons. Shadows are short,
   low-opacity and independent of focus, without changing hit geometry. */
static void ui_card_shadow(SDL_Rect rect,int radius){
    int light=ui_theme_enabled&&ui_light_theme;
    SDL_Rect soft={rect.x-2,rect.y+1,rect.w+4,rect.h+4};
    rounded_box(soft,radius+2,ui_rgb(0x000000,light?10:22),NULL);
    SDL_Rect contact={rect.x,rect.y+2,rect.w,rect.h+1};
    rounded_box(contact,radius,ui_rgb(0x000000,light?9:17),NULL);
}
static void ui_card_edge(SDL_Rect rect,int radius){
    int light=ui_theme_enabled&&ui_light_theme;
    rounded_outline(rect,radius,1,ui_rgb(light?0x536476:0xd2e4f7,light?34:22));
}
static void ui_panel(Paint *p,int x,int y,int w,int h,int radius){
 SDL_Rect rect={x,y,w,h};
 ui_card_shadow(rect,radius);
 /* Soft vertical tonal gradient in one rounded mesh, no banding.
    Distinct bright/dark palettes keep text and poster artwork intact. */
 SDL_Color top=ui_rgb(ui_light_theme?0xf8fafc:0x202a34,ui_wallpaper_active?215:255);
 SDL_Color bottom=ui_rgb(ui_light_theme?0xeef2f6:0x151b23,ui_wallpaper_active?205:255);
 enum {SEG=20,N=4*(SEG+1)};
 SDL_Vertex v[N+1];int idx[N*3];
 SDL_Color mid={(top.r+bottom.r)/2,(top.g+bottom.g)/2,(top.b+bottom.b)/2,(top.a+bottom.a)/2};
 v[0]=(SDL_Vertex){{x+w*.5f,y+h*.5f},mid,{0,0}};
 for(int corner=0;corner<4;corner++){
  float cx=x+(corner==0||corner==3?w-radius:radius),cy=y+(corner<2?h-radius:radius);
  for(int j=0;j<=SEG;j++){
   int k=corner*(SEG+1)+j;float angle=(corner*SEG+j)*1.57079632679f/SEG;
   float px=cx+radius*cosf(angle),py=cy+radius*sinf(angle),t=(py-y)/h;
   SDL_Color c={top.r+(bottom.r-top.r)*t,top.g+(bottom.g-top.g)*t,top.b+(bottom.b-top.b)*t,top.a+(bottom.a-top.a)*t};
   v[k+1]=(SDL_Vertex){{px,py},c,{0,0}};idx[k*3]=0;idx[k*3+1]=k+1;idx[k*3+2]=(k+1)%N+1;
  }
 }
 SDL_RenderGeometry(ui_renderer,NULL,v,N+1,idx,N*3);
 rounded_outline(rect,radius,1,ui_rgb(ui_light_theme?0xd1d9e1:0x526272,90));
 SDL_Rect inside={x+1,y+1,w-2,h-2};
 rounded_outline(inside,radius-1,1,ui_rgb(0xffffff,ui_light_theme?44:12));
 (void)p;
}

/* PS5-inspired ambient background. Original procedural artwork, no external
   download, no Sony assets. Two cached textures; never drawn over playback. */
static SDL_Texture *ui_ambient[2];
static unsigned ui_noise(unsigned v){v^=v>>16;v*=0x7feb352du;v^=v>>15;v*=0x846ca68bu;return v^(v>>16);}
static SDL_Texture *ui_ambient_create(int light){
    enum {W=1280,H=720};
    SDL_Surface *s=SDL_CreateRGBSurfaceWithFormat(0,W,H,32,SDL_PIXELFORMAT_RGBA32);
    if(!s)return NULL;
    for(int y=0;y<H;y++){
        Uint8 *row=(Uint8*)s->pixels+y*s->pitch;
        float fy=(float)y/(H-1);
        for(int x=0;x<W;x++){
            float fx=(float)x/(W-1);
            float dx=fx-.79f,dy=fy-.23f;
            float cool=expf(-5.5f*dx*dx-7.5f*dy*dy);
            float wave=fy-(.72f-.25f*fx+.10f*sinf(fx*3.2f));
            float ribbon=expf(-wave*wave*55.f)*(.36f+.64f*fx);
            float warm=expf(-14.f*(fx-.78f)*(fx-.78f)-12.f*(fy-.80f)*(fy-.80f));
            float vignette=(fx-.5f)*(fx-.5f)+(fy-.5f)*(fy-.5f);
            float dither=((int)(ui_noise((unsigned)(y*W+x))&255)-128)/128.f;
            float red,green,blue;
            if(light){red=237-9*cool+4*ribbon+3*warm-4*vignette;
                green=241-5*cool+3*ribbon-4*vignette;blue=247-2*cool-4*warm-4*vignette;}
            else {red=10+9*cool+7*ribbon+12*warm-5*vignette;
                green=14+15*cool+9*ribbon+7*warm-5*vignette;
                blue=23+26*cool+12*ribbon+1*warm-6*vignette;}
            row[4*x]=(Uint8)fminf(255,fmaxf(0,red+dither));
            row[4*x+1]=(Uint8)fminf(255,fmaxf(0,green+dither));
            row[4*x+2]=(Uint8)fminf(255,fmaxf(0,blue+dither));
            row[4*x+3]=255;
        }
    }
    /* Sparse, subdued dust towards the right, like an ambient console theme.
       No particle simulation or per-frame generation. */
    if(!light)for(unsigned i=0;i<94;i++){
        unsigned h=ui_noise(i+191u);
        int x=520+(int)(h%745),y=75+(int)((h>>10)%610);
        int strength=7+(int)((h>>24)&15);
        for(int yy=-2;yy<=2;yy++)for(int xx=-2;xx<=2;xx++){
            int px=x+xx,py=y+yy;
            if(px<0||px>=W||py<0||py>=H)continue;
            float d=xx*xx+yy*yy;int add=(int)(strength*expf(-d*.85f));
            Uint8 *p=(Uint8*)s->pixels+py*s->pitch+px*4;
            p[0]=(Uint8)fminf(255,p[0]+add);p[1]=(Uint8)fminf(255,p[1]+add*.85f);
            p[2]=(Uint8)fminf(255,p[2]+add*.58f);
        }
    }
    SDL_Texture *tex=SDL_CreateTextureFromSurface(ui_renderer,s);SDL_FreeSurface(s);
    if(tex){SDL_SetTextureBlendMode(tex,SDL_BLENDMODE_NONE);SDL_SetTextureScaleMode(tex,SDL_ScaleModeLinear);}
    return tex;
}
static int ui_standard_background(void){
    int light=!!ui_light_theme;
    if(!ui_ambient[light])ui_ambient[light]=ui_ambient_create(light);
    if(!ui_ambient[light])return 0;
    SDL_Rect dst={0,0,1280,720};
    return SDL_RenderCopy(ui_renderer,ui_ambient[light],NULL,&dst)==0;
}
static void ui_ambient_destroy(void){
    for(int i=0;i<2;i++)if(ui_ambient[i]){SDL_DestroyTexture(ui_ambient[i]);ui_ambient[i]=NULL;}
}
static SDL_Color ui_mix(SDL_Color a,SDL_Color b,float t){
    return (SDL_Color){a.r+(b.r-a.r)*t,a.g+(b.g-a.g)*t,a.b+(b.b-a.b)*t,a.a+(b.a-a.a)*t};
}
/* Rounded gradient with one physical pixel coverage fringe, matching
   the high resolution rounded_box geometry. */
static void ui_gradient_round(SDL_Rect rect,float radius,SDL_Color top,SDL_Color bottom){
    if(rect.w<=0||rect.h<=0)return;
    SDL_FRect r={rect.x,rect.y,rect.w,rect.h};
    float rad=fminf(radius,fminf(r.w,r.h)*.5f),aa=fminf(ui_pixel(),rad*.5f);
    SDL_Vertex v[1+2*UI_PERIM];int ix[9*UI_PERIM];
    v[0]=(SDL_Vertex){{r.x+r.w*.5f,r.y+r.h*.5f},ui_mix(top,bottom,.5f),{0,0}};
    for(int k=0;k<UI_PERIM;k++){
        int a=1+2*k,b=1+2*((k+1)%UI_PERIM);
        SDL_FPoint p=ui_round_point(r,rad,k,-aa*.5f),q=ui_round_point(r,rad,k,aa*.5f);
        SDL_Color c=ui_mix(top,bottom,fminf(1.f,fmaxf(0.f,(p.y-r.y)/r.h))),clear=c;clear.a=0;
        v[a]=(SDL_Vertex){p,c,{0,0}};v[a+1]=(SDL_Vertex){q,clear,{0,0}};
        int j=k*9;ix[j]=0;ix[j+1]=a;ix[j+2]=b;ix[j+3]=a;ix[j+4]=a+1;ix[j+5]=b;
        ix[j+6]=b;ix[j+7]=a+1;ix[j+8]=b+1;
    }
    SDL_RenderGeometry(ui_renderer,NULL,v,1+2*UI_PERIM,ix,9*UI_PERIM);
}
static int ui_is_focused(SDL_Rect r){return ui_focus.seen&&SDL_RectEquals(&r,&ui_focus.pending);}
static void ui_card_depth(SDL_Rect rect,int radius,SDL_Color base){
    if(ui_is_focused(rect))ui_focus.pending_radius=(float)radius;
    ui_card_shadow(rect,radius);
    SDL_Color top=ui_mix(base,ui_rgb(0xffffff,base.a),ui_light_theme?.085f:.055f);
    SDL_Color bottom=ui_mix(base,ui_rgb(0x000000,base.a),ui_light_theme?.035f:.075f);
    ui_gradient_round(rect,radius,top,bottom);
    ui_card_edge(rect,radius);
}
static int ui_button_depth(SDL_Rect rect,int radius,SDL_Color base,int focused){
    if(ui_is_focused(rect))ui_focus.pending_radius=(float)radius;
    int light=ui_theme_enabled&&ui_light_theme;
    float age=(float)(SDL_GetTicks()-ui_press_at);
    float press=focused&&ui_press_at&&age<145.f?sinf(3.14159265f*age/145.f):0.f;
    int drop=(int)roundf(2.f*press),lift=0;
    /* Keep the logical hit rectangle fixed. Only the face moves 0–2 px. */
    SDL_Rect shadow={rect.x-1,rect.y+2+lift,rect.w+2,rect.h+2};
    rounded_box(shadow,radius+1,ui_rgb(0x000000,(int)((light?23:48)*(1.f-.65f*press))),NULL);
    shadow=(SDL_Rect){rect.x,rect.y+4+lift,rect.w,rect.h};
    rounded_box(shadow,radius,ui_rgb(0x000000,(int)((light?13:30)*(1.f-.65f*press))),NULL);
    SDL_Rect face=rect;face.y+=drop;
    SDL_Color white=ui_rgb(0xffffff,base.a),black=ui_rgb(0x000000,base.a);
    SDL_Color top=ui_mix(base,white,(light?.20f:.17f));
    SDL_Color bottom=ui_mix(base,black,light?.09f:.24f);
    top=ui_mix(top,black,.12f*press);bottom=ui_mix(bottom,black,.09f*press);
    ui_gradient_round(face,radius,top,bottom);
    SDL_Rect inside={face.x+1,face.y+1,face.w-2,face.h-2};
    rounded_outline(inside,radius-1,1,ui_rgb(light?0xffffff:0xd2e4f7,light?125:31));
    rounded_outline(face,radius,1,ui_rgb(light?0x586e82:0x000000,light?46:62));
    return drop;
}
static void ui_header_left(Paint *p,const char *s,int x,int width,int cy,TTF_Font *font,SDL_Color c){
    int old=ui_theme_enabled;ui_theme_enabled=0;
    text_font(p,s,x,text_midline_top(p,s,font,cy),width,font,c.r/255.f,c.g/255.f,c.b/255.f);
    ui_theme_enabled=old;
}
