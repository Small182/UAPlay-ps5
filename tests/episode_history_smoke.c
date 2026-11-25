#include "library.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static void expect(int condition,const char *message) {
    if(!condition){fprintf(stderr,"Episode history: %s\n",message);exit(1);}
}
int main(void){
    /* The portable build uses its isolated SD path under the test cwd. */
    mkdir("sdmc:",0777);
    mkdir("sdmc:/switch",0777);
    mkdir("sdmc:/switch/uaplay",0777);
    Movie movie={0};
    movie.id=147;
    movie.series=1;
    snprintf(movie.title,sizeof(movie.title),"Native PS5 test series");
    library_init();
    double position=0;
    expect(library_episode_status(&movie,2,7,&position)==0,"fresh episode should be unwatched");
    library_set_episode_progress(&movie,2,7,126.7);
    expect(library_episode_status(&movie,2,7,&position)==1 && position>126,
           "started episode should retain progress");
    expect(library_episode_status(&movie,2,8,NULL)==0,
           "watch state must be per episode");
    library_init();
    expect(library_episode_status(&movie,2,7,&position)==1 && position>126,
           "started episode must persist across restart");
    library_mark_episode_completed(&movie,2,7);
    expect(library_episode_status(&movie,2,7,&position)==2 && position==0,
           "EOF must mark the episode fully watched");
    library_init();
    expect(library_episode_status(&movie,2,7,&position)==2,
           "completed marker must survive restart");
    library_set_episode_progress(&movie,2,8,45);
    expect(library_episode_status(&movie,2,8,&position)==1,
           "a second episode should be independent");
    expect(library_clear_history()>0,"clear history should clear saved episodes");
    expect(library_episode_status(&movie,2,7,NULL)==0,
           "clear history must clear completed marker");
    expect(library_episode_status(&movie,2,8,NULL)==0,
           "clear history must clear partial marker");
    puts("Native per-episode history tests passed.");
    return 0;
}
