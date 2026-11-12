/*
 * Thin Linux bridge to the EXISTING UAPlay Switch parsers, not a new scraper.
 * Usage: uaplay_bridge movie|series title original year season episode provider(1-7)
 * stdout: {"streams":[Stremio stream objects]}; diagnostics only to stderr.
 */
#include <curl/curl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "catalog.h"

#define SOURCES_CAP 12
#define EPISODES_CAP 512

static void escaped(const char *s) {
    putchar('"');
    for (const unsigned char *p = (const unsigned char *)(s ? s : ""); *p; ++p) {
        if (*p == '"' || *p == '\\') { putchar('\\'); putchar(*p); }
        else if (*p == '\n') fputs("\\n", stdout);
        else if (*p == '\r') fputs("\\r", stdout);
        else if (*p == '\t') fputs("\\t", stdout);
        else if (*p < 0x20) printf("\\u%04x", (unsigned)*p);
        else putchar(*p);
    }
    putchar('"');
}
static int valid_media_url(const char *s) {
    return s && (!strncmp(s, "https://", 8) || !strncmp(s, "http://", 7)) &&
      (strstr(s, ".m3u8") || strstr(s, ".mp4") || strstr(s, ".mpd") || strstr(s, ".mkv"));
}
static int emit(const char *provider, const char *label, const char *url, int *count) {
    if (!valid_media_url(url) || *count >= 32) return 0;
    if (*count) putchar(',');
    fputs("{\"name\":", stdout);
    char name[120];
    snprintf(name, sizeof name, "UAPlay | %s", provider);
    escaped(name);
    fputs(",\"title\":", stdout);
    escaped(label);
    fputs(",\"url\":", stdout);
    escaped(url);
    int n = uakino_resolved_subtitle_count();
    if (n > 0) {
        fputs(",\"subtitles\":[", stdout);
        int printed = 0;
        for (int i = 0; i < n && i < 12; ++i) {
            const char *sub = uakino_resolved_subtitle_url(i);
            if (!sub || (strncmp(sub,"http://",7) && strncmp(sub,"https://",8))) continue;
            if (printed++) putchar(',');
            fputs("{\"lang\":\"ukr\",\"url\":", stdout);
            escaped(sub);
            putchar('}');
        }
        putchar(']');
    }
    putchar('}');
    ++*count;
    return 1;
}
int main(int argc, char **argv) {
    if (argc != 8 || (strcmp(argv[1],"movie") && strcmp(argv[1],"series"))) {
        fputs("Usage: uaplay_bridge movie|series title original year season episode provider\n", stderr);
        return 2;
    }
    int is_series = !strcmp(argv[1],"series");
    int season = atoi(argv[5]), episode = atoi(argv[6]), provider = atoi(argv[7]);
    if (provider < 1 || provider > 7 || (is_series && (season < 1 || episode < 1))) return 2;
    if (strlen(argv[2]) >= 192 || strlen(argv[3]) >= 192 || strlen(argv[4]) >= 12) return 2;
    static const char *names[] = { "UAFlix","UASerials","Eneyida","UAFilm","UAKino","Mikai","AnimeON" };
    Movie movie = {0};
    movie.series = is_series;
    snprintf(movie.title,sizeof movie.title,"%s",argv[2]);
    snprintf(movie.original,sizeof movie.original,"%s",argv[3]);
    snprintf(movie.year,sizeof movie.year,"%s",argv[4]);
    if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK) return 2;
    catalog_http_init();

    Source *sources = (Source *)calloc(SOURCES_CAP, sizeof(Source));
    Episode *episodes = (Episode *)calloc(EPISODES_CAP, sizeof(Episode));
    if (!sources || !episodes) {
        free(sources); free(episodes);
        catalog_http_cleanup(); curl_global_cleanup();
        return 2;
    }
    char error[256] = {0};
    int found = uakino_search_provider(&movie,sources,SOURCES_CAP,provider,error,sizeof error);
    if (found > 0) found = catalog_rank_sources(&movie,sources,found,provider);
    if (found < 0) found = 0;
    if (found > SOURCES_CAP) found = SOURCES_CAP;
    int count = 0;
    fputs("{\"streams\":[", stdout);
    for (int i = 0; i < found && i < 3 && count < 24; i++) {
        if (is_series) {
            int n = uakino_episodes(&sources[i],episodes,EPISODES_CAP,error,sizeof error);
            for (int j = 0; j < n && count < 24; j++) {
                if (episodes[j].season != season || episodes[j].number != episode) continue;
                Source selected = {0};
                snprintf(selected.title,sizeof selected.title,"%s",episodes[j].title);
                snprintf(selected.url,sizeof selected.url,"%s",episodes[j].url);
                char stream[2048] = {0};
                if (!uakino_resolve(&selected,stream,sizeof stream,error,sizeof error)) continue;
                char label[300];
                snprintf(label,sizeof label,"%s | S%02dE%02d | %.90s",
                         sources[i].title,season,episode,episodes[j].voice);
                emit(names[provider-1],label,stream,&count);
            }
        } else {
            char stream[2048] = {0};
            if (!uakino_resolve(&sources[i],stream,sizeof stream,error,sizeof error)) continue;
            emit(names[provider-1], sources[i].title, stream, &count);
            int alternate = uakino_resolved_audio_count();
            // Alternate audio sources can be separate stream URLs in PlayerJS.
            for (int j = 0; j < alternate && count < 24; j++) {
                const char *alt = uakino_resolved_audio_url(j);
                if (!valid_media_url(alt) || !strcmp(alt,stream)) continue;
                char label[300];
                snprintf(label,sizeof label,"%s | %.90s",
                         sources[i].title,uakino_resolved_audio_label(j));
                emit(names[provider-1],label,alt,&count);
            }
        }
    }
    fputs("]}\n", stdout);
    free(sources); free(episodes);
    catalog_http_cleanup();
    curl_global_cleanup();
    return 0;
}
