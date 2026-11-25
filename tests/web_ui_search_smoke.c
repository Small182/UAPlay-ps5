#include "web_ui_search.h"
#include <stdio.h>
#include <stdlib.h>
static void require(int passed,const char *label){
    if(!passed){fprintf(stderr,"UAPlay-web IPTV search: %s\n",label);exit(1);}
}
int main(void){
    require(ps5_tv_contains("Суспільне Новини","новини"),"casefold Ukrainian");
    require(ps5_tv_contains("ТЕЛЕКАНАЛ ЇЖА","їжа"),"Ukrainian Yi case");
    require(ps5_tv_contains("Єдині Новини","єдині"),"Ukrainian Ye case");
    require(ps5_tv_contains("ҐУДЗИК TV","ґудзик"),"Ukrainian G case");
    require(ps5_tv_contains("UAPLAY CNN LIVE","cnn"),"ASCII insensitive");
    require(ps5_tv_contains("Новини України","уКрАїНи"),"mixed-case suffix");
    require(!ps5_tv_contains("Суспільне Новини","спорт"),"negative Ukrainian match");
    require(!ps5_tv_contains("ABC","AC"),"non-contiguous query");
    require(!ps5_tv_contains("ABC","ABCD"),"query longer than title");
    require(ps5_tv_contains("ABC",""),"empty query");
    puts("UAPlay-web native TV search checks passed.");
    return 0;
}
