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
static void ui_icon(UiIcon icon,float cx,float cy,float size,SDL_Color c){
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
 if(m.ni)SDL_RenderGeometry(ui_renderer,NULL,m.v,m.nv,m.ix,m.ni);
}
static void ui_center_color(Paint *p,const char *s,int x,int y,int w,int h,TTF_Font *font,SDL_Color c){
 int old=ui_theme_enabled;ui_theme_enabled=0;
 text_center_button(p,s,x,y,w,h,font,c.r/255.f,c.g/255.f,c.b/255.f);
 ui_theme_enabled=old;
}
static void ui_labeled_icon(Paint *p,UiIcon i,const char *s,int x,int y,int w,int h,SDL_Color c){
 int tw=measure(p,s,p->font_small),sz=22,gap=8,left=x+(w-tw-sz-gap)/2;
 if(left<x+6)left=x+6;
 ui_icon(i,left+sz*.5f,y+h*.5f,sz,c);
 ui_center_color(p,s,left+sz+gap,y,w-(left-x)-sz-gap-5,h,p->font_small,c);
}
/* Render focus last: no clipping by a subsequently drawn card. The logical
   focus switches immediately; the visual ring eases to the target. */
static struct {SDL_FRect from,visual;SDL_Rect target;int valid,scene,seen;Uint32 changed;} ui_focus;
static void ui_focus_begin(int scene){
 if(ui_focus.scene!=scene){ui_focus.valid=0;ui_focus.scene=scene;}
 ui_focus.seen=0;
}
static void ui_focus_sample(Uint32 now){
 float t=fminf(1.f,(now-ui_focus.changed)/175.f),f=1.f-(1.f-t)*(1.f-t)*(1.f-t);
 SDL_FRect a=ui_focus.from;SDL_Rect b=ui_focus.target;
 ui_focus.visual=(SDL_FRect){a.x+(b.x-a.x)*f,a.y+(b.y-a.y)*f,a.w+(b.w-a.w)*f,a.h+(b.h-a.h)*f};
}
static void ui_focus_set(SDL_Rect r){
 Uint32 now=SDL_GetTicks();
 if(!ui_focus.valid){
  ui_focus.from=ui_focus.visual=(SDL_FRect){r.x,r.y,r.w,r.h};ui_focus.target=r;ui_focus.changed=now;ui_focus.valid=1;
 }else if(!SDL_RectEquals(&r,&ui_focus.target)){
  ui_focus_sample(now);ui_focus.from=ui_focus.visual;ui_focus.target=r;ui_focus.changed=now;
  if(fabsf(ui_focus.from.y-r.y)>280||fabsf(ui_focus.from.w-r.w)>550)
   ui_focus.from=(SDL_FRect){r.x,r.y,r.w,r.h};
 }
 ui_focus.seen=1;
}
static void ui_focus_finish(void){
 if(!ui_focus.seen){ui_focus.valid=0;return;}
 Uint32 now=SDL_GetTicks();ui_focus_sample(now);SDL_FRect v=ui_focus.visual;
 float age=(float)(now-ui_press_at);int inset=ui_press_at&&age<145?(int)roundf(2.5f*sinf(3.14159265f*age/145.f)):0;
 SDL_Rect r={(int)roundf(v.x)-3+inset,(int)roundf(v.y)-3+inset,(int)roundf(v.w)+6-2*inset,(int)roundf(v.h)+6-2*inset};
 if(r.w<10||r.h<10)return;
 float a,b,c;accent_real(ui_accent_index,&a,&b,&c);
 if(ui_theme_enabled&&ui_light_theme&&ui_accent_index==ACCENT_WHITE)a=b=c=.35f;
 SDL_Color color={color_byte(a),color_byte(b),color_byte(c),255};
 int radius=r.h>94?15:16;
 if(abs(r.w-r.h)<8 && r.h>=50)radius=r.h/2;
 if(radius>r.w/2)radius=r.w/2;if(radius>r.h/2)radius=r.h/2;
 SDL_Rect glow={r.x-2,r.y-2,r.w+4,r.h+4};SDL_Color halo=color;halo.a=ui_theme_enabled&&ui_light_theme?25:46;
 rounded_outline(glow,radius+2,2,halo);rounded_outline(r,radius,2,color);
}
static int ui_focus_moving(void){Uint32 now=SDL_GetTicks();return ui_focus.valid&&(now-ui_focus.changed<175||(ui_press_at&&now-ui_press_at<145));}
static void ui_panel(Paint *p,int x,int y,int w,int h,int radius){
 SDL_Color bg=ui_rgb(ui_light_theme?0xf4f6f8:0x171c24,ui_wallpaper_active?215:255);
 rounded_box((SDL_Rect){x,y,w,h},radius,bg,NULL);
 rounded_outline((SDL_Rect){x,y,w,h},radius,1,ui_rgb(ui_light_theme?0xd1d9e1:0x526272,90));
 (void)p;
}
