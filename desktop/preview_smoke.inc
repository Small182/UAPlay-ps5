/* Included into main.c only for the desktop preview. Uses actual UI drawing. */
static int preview_capture(const char *name){
    int w=0,h=0;
    if(SDL_GetRendererOutputSize(ui_renderer,&w,&h)<0||w<=0||h<=0)return 0;
    SDL_Surface *img=SDL_CreateRGBSurfaceWithFormat(0,w,h,32,SDL_PIXELFORMAT_ARGB8888);
    if(!img)return 0;
    const char *dir=getenv("UAPLAY_PREVIEW_SMOKE");
    char path[1024];snprintf(path,sizeof(path),"%s/%s.bmp",dir,name);
    int ok=SDL_RenderReadPixels(ui_renderer,NULL,img->format->format,img->pixels,img->pitch)==0
           && SDL_SaveBMP(img,path)==0;
    SDL_FreeSurface(img);
    fprintf(stderr,"[preview] %s: %s\n",name,ok?"PASS":"FAIL");return ok;
}
static void preview_fixture(Movie *movies,int n){
    static const char *titles[]={"Тестовий фільм","Мандрівка у космос","Далекі береги","Нічне місто",
        "Крізь час","Загублений світ","Пригоди","Останній рейс","Море","Північне сяйво","Гори","Новий день"};
    for(int i=0;i<n;i++){
        movies[i].id=100000+i;
        movies[i].series=(i%3==1);
        movies[i].runtime_minutes=112;movies[i].season_count=3;movies[i].episode_count=24;
        movies[i].rating=7.8f;movies[i].details_state=1;
        snprintf(movies[i].title,sizeof(movies[i].title),"%s",titles[i%12]);
        snprintf(movies[i].original,sizeof(movies[i].original),"Preview %d",i+1);
        snprintf(movies[i].year,sizeof(movies[i].year),"%d",2026-i%8);
        snprintf(movies[i].poster,sizeof(movies[i].poster),"preview:%d",i);
        snprintf(movies[i].overview,sizeof(movies[i].overview),
            "Тестові дані для перевірки інтерфейсу UAPlay. Опис, рейтинг, тривалість, рамки виділення, "
            "розміщення елементів та читабельність тексту не залежать від мережі.");
    }
}
static int preview_page(Paint *paint,Page page,Movie *movies,Source *sources,const char *name){
    /* The regular draw() animates page transitions for ~200 ms. */
    draw(paint,page,movies,12,0,sources,2,0,"Популярні фільми","");
    SDL_Delay(350);
    draw(paint,page,movies,12,0,sources,2,0,"Популярні фільми","");
    return preview_capture(name);
}
static int preview_smoke(Paint *paint){
    Movie movies[12]={0};Source sources[2]={0};
    preview_fixture(movies,12);
    snprintf(sources[0].title,sizeof(sources[0].title),"Тестове джерело 1");
    snprintf(sources[1].title,sizeof(sources[1].title),"Тестове джерело 2");
    int ok=1;
    for(int light=0;light<2;light++){
        ui_light_theme=light;
        ui_category=0;expanded_catalog=0;home_shelf=0;ui_search_query[0]=0;
        ok &= preview_page(paint,CATALOG,movies,sources,light?"home-light":"home-dark");
        ui_category=2;expanded_catalog=1;
        ok &= preview_page(paint,CATALOG,movies,sources,light?"grid-light":"grid-dark");
        snprintf(ui_search_query,sizeof(ui_search_query),"Тест");
        ok &= preview_page(paint,CATALOG,movies,sources,light?"search-light":"search-dark");
        ui_search_query[0]=0;
        ok &= preview_page(paint,DETAILS,movies,sources,light?"details-light":"details-dark");
        ok &= preview_page(paint,SETTINGS,movies,sources,light?"settings-light":"settings-dark");
        ok &= preview_page(paint,APPEARANCE,movies,sources,light?"appearance-light":"appearance-dark");
        ok &= preview_page(paint,ACCENT_PALETTE,movies,sources,light?"accent-light":"accent-dark");
        ok &= preview_page(paint,PROVIDERS,movies,sources,light?"providers-light":"providers-dark");
        ok &= preview_page(paint,SOURCES,movies,sources,light?"sources-light":"sources-dark");
    }
    const char *clip=getenv("UAPLAY_PREVIEW_VIDEO");
    if(!clip||!*clip){fprintf(stderr,"Missing UAPLAY_PREVIEW_VIDEO\n");return 1;}
    mpv_handle *p=NULL;mpv_render_context *render=NULL;
    if(!open_player(&p,&render)){fprintf(stderr,"VLC startup failed: %s\n",player_init_message());return 1;}
    const char *args[]={"loadfile",clip,NULL};
    int loaded=0;
    Uint32 began=SDL_GetTicks();
    if(mpv_command_async(p,0,args)<0)ok=0;
    while(ok&&!loaded&&SDL_GetTicks()-began<15000){
        mpv_event *e=mpv_wait_event(p,0);
        if(e->event_id==MPV_EVENT_FILE_LOADED)loaded=1;
        if(e->event_id==MPV_EVENT_END_FILE)break;
        if(mpv_render_context_render(render,NULL)<0){ok=0;break;}
        SDL_Delay(16);
    }
    if(!loaded){fprintf(stderr,"VLC did not deliver first picture: %s\n",ps5_kitchen_diagnostic(p));ok=0;}
    if(loaded){
        double duration=0;mpv_get_property(p,"duration",MPV_FORMAT_DOUBLE,&duration);
        if(duration<5)ok=0;
        const char *seek_args[]={"seek","3","absolute",NULL};
        if(mpv_command_async(p,0,seek_args)<0)ok=0;
        began=SDL_GetTicks();double position=0;
        while(SDL_GetTicks()-began<4000){
            mpv_wait_event(p,0);
            if(mpv_render_context_render(render,NULL)<0){ok=0;break;}
            mpv_get_property(p,"time-pos",MPV_FORMAT_DOUBLE,&position);
            if(position>=2.5)break;
            SDL_Delay(20);
        }
        if(position<2.5)ok=0;
        command(p,"cycle","pause");
        int paused=0;began=SDL_GetTicks();
        while(SDL_GetTicks()-began<2000){
            mpv_get_property(p,"pause",MPV_FORMAT_FLAG,&paused);
            if(paused)break;SDL_Delay(20);
        }
        if(!paused)ok=0;
        if(mpv_render_context_render(render,NULL)<0)ok=0;
        ps5_player_focus=1;
        draw_player_controls(paint,p,&movies[0]);
        ok &= preview_capture("player-paused");
        command(p,"cycle","pause");
    }
    command(p,"stop",NULL);mpv_render_context_free(render);mpv_terminate_destroy(p);
    fprintf(stderr,"[preview] UI and local VLC smoke: %s\n",ok?"PASS":"FAIL");
    return ok?0:1;
}
