#ifndef UAPLAY_WEB_UI_SEARCH_H
#define UAPLAY_WEB_UI_SEARCH_H

/* Shared UTF-8 case-insensitive substring search for native PS5 TV cards.
   Covers the Ukrainian alphabet and ASCII while allocating no buffers. */
static inline unsigned ps5_tv_fold_next(const unsigned char **ptr){
    const unsigned char *p=*ptr;
    unsigned c=*p++;
    if(c>=0xc2 && c<0xe0 && (p[0]&0xc0)==0x80){
        c=((c&0x1f)<<6)|(p[0]&0x3f);p++;
    }else if(c>=0xe0 && c<0xf0 && (p[0]&0xc0)==0x80 &&
             (p[1]&0xc0)==0x80){
        c=((c&0x0f)<<12)|((p[0]&0x3f)<<6)|(p[1]&0x3f);p+=2;
    }
    *ptr=p;
    if(c>='A' && c<='Z')return c+32;
    if(c>=0x0410 && c<=0x042f)return c+32;
    if(c==0x0401)return 0x0451;
    if(c==0x0404)return 0x0454;
    if(c==0x0406)return 0x0456;
    if(c==0x0407)return 0x0457;
    if(c==0x0490)return 0x0491;
    return c;
}
static inline int ps5_tv_contains(const char *haystack,const char *needle){
    if(!needle || !*needle)return 1;
    const unsigned char *start=(const unsigned char *)(haystack?haystack:"");
    for(const unsigned char *h=start;*h;){
        const unsigned char *a=h,*b=(const unsigned char *)needle;
        int equal=1;
        while(*b){
            if(!*a || ps5_tv_fold_next(&a)!=ps5_tv_fold_next(&b)){
                equal=0;break;
            }
        }
        if(equal)return 1;
        (void)ps5_tv_fold_next(&h);
    }
    return 0;
}
#endif
