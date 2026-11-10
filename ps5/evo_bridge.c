#include "evo_bridge.h"

#include <curl/curl.h>
#include <stdio.h>
#include <string.h>

#define EVO_PLAY_ENDPOINT "http://127.0.0.1:8686/evo/play?url="

static size_t discard_body(char *buffer,size_t size,size_t count,void *userdata) {
    (void)buffer;(void)userdata;
    return size*count;
}

static int allowed_url(const char *url) {
    if(!url)return 0;
    const char *p=NULL;
    if(!strncmp(url,"https://",8))p=url+8;
    else if(!strncmp(url,"http://",7))p=url+7;
    if(!p || !*p || *p=='/' || *p=='?' || *p=='#')return 0;
    for(;*p;p++)if((unsigned char)*p<0x20 || (unsigned char)*p==0x7f)return 0;
    return 1;
}

static int append_encoded(char *out,size_t capacity,size_t *used,
                          const char *value) {
    static const char hex[]="0123456789ABCDEF";
    for(const unsigned char *p=(const unsigned char *)value;*p;p++) {
        unsigned char c=*p;
        int plain=(c>='a'&&c<='z')||(c>='A'&&c<='Z')||
                  (c>='0'&&c<='9')||c=='-'||c=='_'||c=='.'||c=='~';
        size_t need=plain?1:3;
        if(*used+need>=capacity)return 0;
        if(plain)out[(*used)++]=(char)c;
        else {out[(*used)++]='%';out[(*used)++]=hex[c>>4];out[(*used)++]=hex[c&15];}
    }
    out[*used]=0;
    return 1;
}

int uaplay_evo_build_request(const char *stream_url,const char *title,
                             char *out,size_t capacity) {
    if(!out || !capacity)return 0;
    out[0]=0;
    if(!allowed_url(stream_url))return 0;
    if(!title)title="";
    size_t used=sizeof(EVO_PLAY_ENDPOINT)-1;
    if(used>=capacity)return 0;
    memcpy(out,EVO_PLAY_ENDPOINT,used+1);
    if(!append_encoded(out,capacity,&used,stream_url))return 0;
    if(used+7>=capacity)return 0;
    memcpy(out+used,"&title=",7);used+=7;
    out[used]=0;
    return append_encoded(out,capacity,&used,title);
}

int uaplay_evo_handoff(const char *stream_url,const char *title,
                       char *error,size_t error_capacity) {
    char url[8192];
    if(error && error_capacity)error[0]=0;
    if(!uaplay_evo_build_request(stream_url,title,url,sizeof(url))) {
        if(error && error_capacity)snprintf(error,error_capacity,"Invalid or oversized stream URL");
        return -1;
    }
    CURL *curl=curl_easy_init();
    if(!curl) {
        if(error && error_capacity)snprintf(error,error_capacity,"curl initialization failed");
        return -1;
    }
    curl_easy_setopt(curl,CURLOPT_URL,url);
    curl_easy_setopt(curl,CURLOPT_CONNECTTIMEOUT_MS,500L);
    curl_easy_setopt(curl,CURLOPT_TIMEOUT_MS,1500L);
    curl_easy_setopt(curl,CURLOPT_NOSIGNAL,1L);
    curl_easy_setopt(curl,CURLOPT_WRITEFUNCTION,discard_body);
    /* Never send a stream URL to anything other than EVO loopback. */
    curl_easy_setopt(curl,CURLOPT_FOLLOWLOCATION,0L);
    CURLcode status=curl_easy_perform(curl);
    long http=0;
    if(status==CURLE_OK)curl_easy_getinfo(curl,CURLINFO_RESPONSE_CODE,&http);
    curl_easy_cleanup(curl);
    if(status==CURLE_OK && http==200)return 0;
    if(error && error_capacity)
        snprintf(error,error_capacity,"EVO host unavailable (curl=%d, HTTP=%ld)",
                 (int)status,http);
    return -1;
}
