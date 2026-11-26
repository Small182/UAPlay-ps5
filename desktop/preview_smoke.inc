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
/* Separate live network gate: offline screenshot fixtures cannot validate
   a real TMDb credential or a configured Worker. Never log the request URL. */
static int preview_live_catalog(void){
    struct { const char *category,*query,*label; } checks[]={
        {"films","","Films"},{"series","","Series"},{"films","Interstellar","Search"}
    };
    int ok=1;
    for(unsigned i=0;i<sizeof(checks)/sizeof(checks[0]);i++){
        Movie movie={0};
        char error[256]={0};
        int more=0;
        int count=catalog_load_page_sorted(checks[i].category,checks[i].query,
                                           1,0,&movie,1,error,sizeof(error),&more);
        int pass=count>0 && movie.id>0 && movie.title[0] && !error[0];
        fprintf(stderr,"[preview] live catalogue %s: %s (%d items%s%s)\n",
                checks[i].label,pass?"PASS":"FAIL",count,
                error[0]?"; ":"",error);
        ok &= pass;
    }
    return ok;
}
static int preview_focus_motion(Paint *paint,Movie *movies,Source *sources){
    ui_light_theme=0;ui_category=0;expanded_catalog=0;home_shelf=0;
    catalog_hero_focus=0;ui_search_query[0]=0;
    ui_focus.valid=0;
    draw(paint,CATALOG,movies,12,0,sources,2,0,"Популярні фільми","");
    SDL_Rect first=ui_focus.target;
    int ok=ui_focus.valid&&ui_focus.seen&&preview_capture("focus-start");
    draw(paint,CATALOG,movies,12,1,sources,2,0,"Популярні фільми","");
    SDL_Rect second=ui_focus.target;
    if(first.x>=second.x)ok=0;
    /* Test the easing curve with a controlled timestamp, independent of CI
       renderer speed or OS scheduling. */
    ui_focus_sample(ui_focus.changed+55);
    if(ui_focus.visual.x<=first.x||ui_focus.visual.x>=second.x)ok=0;
    ui_focus.changed=SDL_GetTicks()-55;
    draw(paint,CATALOG,movies,12,1,sources,2,0,"Популярні фільми","");
    ok &= preview_capture("focus-moving");
    ui_focus.changed=SDL_GetTicks()-200;
    draw(paint,CATALOG,movies,12,1,sources,2,0,"Популярні фільми","");
    ok &= fabsf(ui_focus.visual.x-second.x)<.1f&&preview_capture("focus-settled");
    fprintf(stderr,"[preview] focus motion: %s\n",ok?"PASS":"FAIL");
    return ok;
}
static int preview_iptv_focus(Paint *p,Movie *movies,Source *sources){
    int ok=1;
    ps5_tv_selection=4;ps5_tv_filter_focus=0;
    tv_focus_move(SDL_CONTROLLER_BUTTON_DPAD_UP,12);
    ok &= ps5_tv_filter_focus==2||ps5_tv_filter_focus==3;
    tv_focus_move(SDL_CONTROLLER_BUTTON_DPAD_UP,12);
    ok &= ps5_tv_filter_focus==4;
    tv_focus_move(SDL_CONTROLLER_BUTTON_DPAD_RIGHT,12);
    ok &= ps5_tv_header_focus==11;
    tv_focus_move(SDL_CONTROLLER_BUTTON_DPAD_DOWN,12);
    ok &= ps5_tv_filter_focus>=1&&ps5_tv_filter_focus<=3;
    tv_focus_move(SDL_CONTROLLER_BUTTON_DPAD_DOWN,12);
    ok &= ps5_tv_filter_focus==0&&ps5_tv_selection<6;
    tv_focus_normalize(0);ok &= ps5_tv_filter_focus==1;
    draw(p,IPTV_CHANNELS,movies,12,0,sources,2,0,"IPTV","");
    SDL_Rect filter=tv_filter_rect(0);
    ok &= SDL_RectEquals(&ui_focus.target,&filter)&&ui_focus.target_radius==14;
    ps5_tv_filter_focus=4;ps5_tv_header_focus=6;
    draw(p,IPTV_CHANNELS,movies,12,0,sources,2,0,"IPTV","");
    SDL_Rect gear={1058,17,53,47};
    ok &= SDL_RectEquals(&ui_focus.target,&gear)&&ui_focus.target_radius==15;
    ui_focus_sample(ui_focus.changed+55);
    ok &= ui_focus.visual_radius<=16.f;
    ok &= preview_capture("iptv-header-focus");
    ps5_tv_reset_selection();ps5_tv_header_focus=IPTV_TAB_FOCUS;
    fprintf(stderr,"[preview] IPTV focus routing and exact gear radius: %s\n",ok?"PASS":"FAIL");
    return ok;
}
static int preview_card_layout(Paint *p){
    int ok=1;
    CardGrid grids[]={card_grid(2,0,5,0),card_grid(1,0,0,0),
        card_grid(2,1,0,0),card_grid(11,0,0,1),card_grid(12,0,0,1)};
    for(unsigned i=0;i<sizeof(grids)/sizeof(grids[0]);i++){
        CardGrid g=grids[i];
        int h=g.ph+CARD_TITLE_GAP+CARD_TITLE_H;
        ok &= g.step-h>=CARD_ROW_GAP && g.y+g.step+h<=g.bottom-CARD_ROW_GAP;
    }
    const CardText *short_title=card_text_layout(p,"Мандрівка у космос",CARD_TITLE_W);
    ok &= short_title->lines[0][0] && short_title->lines[1][0] &&
        measure(p,short_title->lines[0],short_title->font)<=CARD_TITLE_W &&
        measure(p,short_title->lines[1],short_title->font)<=CARD_TITLE_W;
    const CardText *long_title=card_text_layout(p,"Неймовірна мандрівка у космос та пригоди на далеких планетах",CARD_TITLE_W);
    ok &= strstr(long_title->lines[1],"…")!=NULL &&
        measure(p,long_title->lines[1],long_title->font)<=CARD_TITLE_W;
    const CardText *long_word=card_text_layout(p,"Надзвичайнонадзвичайнодовганазвафільму",CARD_TITLE_W);
    ok &= strstr(long_word->lines[1],"…")!=NULL &&
        measure(p,long_word->lines[0],long_word->font)<=CARD_TITLE_W &&
        measure(p,long_word->lines[1],long_word->font)<=CARD_TITLE_W;
    fprintf(stderr,"[preview] two-line card titles and grid clearance: %s\n",ok?"PASS":"FAIL");
    return ok;
}
static int preview_smoke(Paint *paint){
    Movie movies[12]={0};Source sources[2]={0};
    preview_fixture(movies,12);
    snprintf(sources[0].title,sizeof(sources[0].title),"Тестове джерело 1");
    snprintf(sources[1].title,sizeof(sources[1].title),"Тестове джерело 2");
    int ok=preview_pad_selftest();
    ok &= preview_card_layout(paint);
    for(int mode=0;mode<2;mode++){
        SDL_Rect lang=keyboard_key_rect(mode,3,0),space=keyboard_key_rect(mode,3,1),search=keyboard_key_rect(mode,3,2);
        ok &= keyboard_columns(mode,3)==3 && space.x+space.w/2==640 &&
              lang.x+lang.w<space.x && space.x+space.w<search.x &&
              lang.x+search.x+search.w==1280 &&
              keyboard_nearest_col(mode,3,lang.x+lang.w/2)==0 &&
              keyboard_nearest_col(mode,3,640)==1 &&
              keyboard_nearest_col(mode,3,search.x+search.w/2)==2;
    }
    for(int light=0;light<2;light++){
        ui_light_theme=light;
        int ambient_ok=ui_standard_background();
        fprintf(stderr,"[preview] standard %s ambient: %s\n",light?"light":"dark",ambient_ok?"PASS":"FAIL");
        ok &= ambient_ok && preview_capture(light?"ambient-light":"ambient-dark");
    }
    fprintf(stderr,"[preview] bundled web typography: %s\n",paint->web_font_faces?"PASS":"FAIL");
    ok &= paint->web_font_faces;
    int raster_w=0,raster_h=0;
    int logical_w=0,logical_h=0;
    SDL_RenderGetLogicalSize(ui_renderer,&logical_w,&logical_h);
    int hd=SDL_GetRendererOutputSize(ui_renderer,&raster_w,&raster_h)==0 &&
           raster_w==1920 && raster_h==1080 && logical_w==1280 && logical_h==720;
    fprintf(stderr,"[preview] native raster %dx%d, logical %dx%d: %s\n",
            raster_w,raster_h,logical_w,logical_h,hd?"PASS":"FAIL");
    ok &= hd;
    int live_ok=!getenv("UAPLAY_PREVIEW_LIVE_CATALOG") || preview_live_catalog();
    for(int light=0;light<2;light++){
        ui_light_theme=light;
        ui_category=0;expanded_catalog=0;home_shelf=0;ui_search_query[0]=0;
        ok &= preview_page(paint,CATALOG,movies,sources,light?"home-light":"home-dark");
        ui_category=2;expanded_catalog=1;
        ok &= preview_page(paint,CATALOG,movies,sources,light?"grid-light":"grid-dark");
        snprintf(ui_search_query,sizeof(ui_search_query),"Тест");
        ok &= preview_page(paint,CATALOG,movies,sources,light?"search-light":"search-dark");
        ui_search_query[0]=0;
        ui_category=11;ok &= preview_page(paint,CATALOG,movies,sources,light?"watchlist-light":"watchlist-dark");
        ui_category=12;ok &= preview_page(paint,CATALOG,movies,sources,light?"history-light":"history-dark");
        ui_category=2;
        ok &= preview_page(paint,DETAILS,movies,sources,light?"details-light":"details-dark");
        ok &= preview_page(paint,SETTINGS,movies,sources,light?"settings-light":"settings-dark");
        ok &= preview_page(paint,APPEARANCE,movies,sources,light?"appearance-light":"appearance-dark");
        ok &= preview_page(paint,ACCENT_PALETTE,movies,sources,light?"accent-light":"accent-dark");
        ok &= preview_page(paint,PROVIDERS,movies,sources,light?"providers-light":"providers-dark");
        ok &= preview_page(paint,SOURCES,movies,sources,light?"sources-light":"sources-dark");
        ok &= preview_page(paint,PROVIDER_SETTINGS,movies,sources,light?"provider-settings-light":"provider-settings-dark");
        ok &= preview_page(paint,VIDEO_SETTINGS,movies,sources,light?"video-settings-light":"video-settings-dark");
        season_count=2;season_ids[0]=1;season_ids[1]=2;selected_season=0;selected_episode=0;
        episode_count=4;
        for(int i=0;i<4;i++){
            episodes[i]=(Episode){0};episodes[i].season=1+i/2;episodes[i].number=1+i%2;
            snprintf(episodes[i].title,sizeof(episodes[i].title),"Серія %d",episodes[i].number);
        }
        ok &= preview_page(paint,SEASONS,movies,sources,light?"seasons-light":"seasons-dark");
        ok &= preview_page(paint,EPISODES,movies,sources,light?"episodes-light":"episodes-dark");
        season_count=episode_count=0;
        ui_category=0;expanded_catalog=0;catalog_hero_focus=8;
        ok &= preview_page(paint,CATALOG,movies,sources,light?"inactive-header-focus-light":"inactive-header-focus-dark");
        SDL_Rect inactive={295,19,95,43};
        ok &= SDL_RectEquals(&ui_focus.target,&inactive)&&ui_focus.target_radius==15.f;
        catalog_hero_focus=0;
    }
    ok &= preview_focus_motion(paint,movies,sources);
    ok &= preview_iptv_focus(paint,movies,sources);
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
    return (ok&&live_ok)?0:1;
}
