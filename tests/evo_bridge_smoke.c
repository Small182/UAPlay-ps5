#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../ps5/evo_bridge.h"

int main(void) {
    char request[512];
    assert(uaplay_evo_build_request("https://example.org/stream.m3u8?token=a&b=2",
                                    "Фільм",request,sizeof(request)));
    assert(strstr(request,"127.0.0.1:8686/evo/play?url=https%3A%2F%2Fexample.org"));
    assert(strstr(request,"%26b%3D2&title="));
    assert(!uaplay_evo_build_request("file:///etc/passwd","test",request,sizeof(request)));
    assert(!uaplay_evo_build_request("https://example.org/a\r\nHeader: x","test",request,sizeof(request)));
    assert(!uaplay_evo_build_request("https://example.org/a","test",request,8));
    puts("EVO loopback URL handoff contract OK");
    return 0;
}
