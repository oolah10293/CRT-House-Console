#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "../../RadioDisplay.h"
static int checks=0;
#define CHECK(x) do {checks++;if(!(x)){fprintf(stderr,"FAIL %s:%d: %s\n",__FILE__,__LINE__,#x);exit(1);}} while(0)
void parse(const char* artist,const char* title,const char* expectedArtist,const char* expectedTitle){
  char a[96],t[160];crt::radio::cleanMetadata(artist,title,a,sizeof(a),t,sizeof(t));
  CHECK(strcmp(a,expectedArtist)==0);CHECK(strcmp(t,expectedTitle)==0);
}
int main(){
  CHECK(crt::radio::isStreamUrl("https://stream.example.com/path"));
  CHECK(crt::radio::isStreamUrl("HTTP://example.test/live"));
  CHECK(!crt::radio::isStreamUrl("MP3s/A Song.mp3"));
  CHECK(crt::radio::sourceIsRadio("radio","MP3s/A Song.mp3"));
  CHECK(!crt::radio::sourceIsRadio("library","https://example.test/stream"));
  CHECK(crt::radio::sourceIsRadio("","https://example.test/stream"));
  parse("","Disturbed - text=\"I Will Not Break\" song_spot=\"N\" Media:Basal...","Disturbed","I Will Not Break");
  parse("Disturbed","text=\"I Will Not Break\" song_spot=\"N\"","Disturbed","I Will Not Break");
  parse("","Disturbed - I Will Not Break","Disturbed","I Will Not Break");
  parse("Disturbed","Disturbed - I Will Not Break","Disturbed","I Will Not Break");
  parse("","Artist - Song name song_spot=\"N\"","Artist","Song name");
  parse("Band","The Song","Band","The Song");
  parse("","Just A Song","","Just A Song");
  parse("","text=\"Soft Piano\" media:other","","Soft Piano");
  parse("","","","");
  parse("","Media:trailer","","");
  parse("","Text: Great Recording","","Text: Great Recording");
  char host[50];crt::radio::hostFromUrl("https://user:password@stream.example.test:8000/song",host,sizeof(host));
  CHECK(!strcmp(host,"stream.example.test:8000"));
  crt::radio::hostFromUrl("MP3s/track.mp3",host,sizeof(host));CHECK(!*host);
  char shortA[7],shortT[8];crt::radio::cleanMetadata("Very Long Artist","Long title",shortA,sizeof(shortA),shortT,sizeof(shortT));
  CHECK(strlen(shortA)<sizeof(shortA));CHECK(strlen(shortT)<sizeof(shortT));
  printf("PASS %d radio metadata/parser assertions.\n",checks);
}
