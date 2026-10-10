#pragma once
// Small, allocation-free helpers for displaying MPD live-radio metadata.
// This is presentation logic only: it never changes HOUSE/MPD playback.
#include <cstddef>
#include <cstring>
#include <cstdio>

namespace crt {
namespace radio {

inline char asciiLower(char ch) {
  return (ch >= 'A' && ch <= 'Z') ? char(ch + ('a' - 'A')) : ch;
}
inline bool equalsIgnoreCase(const char* a,const char* b) {
  if (!a || !b) return a == b;
  for (;*a && *b;a++,b++) if (asciiLower(*a) != asciiLower(*b)) return false;
  return *a == *b;
}
inline bool startsIgnoreCase(const char* str,const char* prefix) {
  if (!str || !prefix) return false;
  while (*prefix) {
    if (!*str || asciiLower(*str++) != asciiLower(*prefix++)) return false;
  }
  return true;
}
inline bool isStreamUrl(const char* path) {
  return startsIgnoreCase(path,"http://") || startsIgnoreCase(path,"https://");
}
inline bool sourceIsRadio(const char* explicitType,const char* path) {
  // A modern server's explicit source takes precedence over heuristics.
  if (explicitType && *explicitType) return equalsIgnoreCase(explicitType,"radio");
  return isStreamUrl(path);
}
inline const char* findIgnoreCase(const char* src,const char* needle) {
  if (!src || !needle || !*needle) return nullptr;
  for(const char* p=src;*p;p++) if (startsIgnoreCase(p,needle)) return p;
  return nullptr;
}
inline void copyRange(char* dst,size_t cap,const char* src,size_t len) {
  if (!dst || !cap) return;
  if (!src) { dst[0]=0; return; }
  if (len>=cap) len=cap-1;
  memcpy(dst,src,len); dst[len]=0;
}
inline void copyText(char* dst,size_t cap,const char* src) {
  copyRange(dst,cap,src,src?strlen(src):0);
}
inline void trim(char* s) {
  if (!s) return;
  size_t n=strlen(s),start=0;
  while(start<n && (s[start]==' ' || s[start]=='\t' || s[start]=='\r' || s[start]=='\n')) start++;
  while(n>start && (s[n-1]==' ' || s[n-1]=='\t' || s[n-1]=='\r' || s[n-1]=='\n')) n--;
  if(start) memmove(s,s+start,n-start);
  s[n-start]=0;
}
inline bool tokenBoundary(const char* beginning,const char* token) {
  return token==beginning || token[-1]==' ' || token[-1]=='\t' || token[-1]=='-';
}
inline void trimTrailingTags(char* text) {
  if(!text) return;
  const char* tags[]={"song_spot=", "media:", "duration=", "album_id=", "cover=", "image="};
  char* earliest=nullptr;
  for(const char* tag:tags) {
    const char* p=text;
    while((p=findIgnoreCase(p,tag))!=nullptr) {
      if(tokenBoundary(text,p)) { if(!earliest || p<earliest) earliest=const_cast<char*>(p); break; }
      p++;
    }
  }
  if(earliest) *earliest=0;
  trim(text);
}
inline void hostFromUrl(const char* url,char* host,size_t cap) {
  if(!host || !cap) return;
  host[0]=0;
  if(!isStreamUrl(url)) return;
  const char* p=strstr(url,"://");
  if(!p) return;
  p+=3;
  const char* end=p;
  while(*end && *end!='/' && *end!='?' && *end!='#') end++;
  // Hide URL userinfo if it exists; never display an embedded password.
  const char* at=p;
  while(at<end && *at!='@') at++;
  if(at<end) p=at+1;
  copyRange(host,cap,p,size_t(end-p));
}

// Common streams provide one of:
//    "Artist - Title"
//    "Artist - text=\"Title\" song_spot=\"N\" Media:..."
//    "text=\"Title\" ..." (sometimes with Artist separately)
// Keep station identity separate from mutable on-air song metadata.
inline void cleanMetadata(const char* rawArtist,const char* rawTitle,
                          char* artist,size_t artistCap,char* title,size_t titleCap) {
  if(!artist || !artistCap || !title || !titleCap) return;
  copyText(artist,artistCap,rawArtist);
  copyText(title,titleCap,rawTitle);
  trim(artist); trim(title);
  trimTrailingTags(artist);

  const char* token=findIgnoreCase(title,"text=");
  if(token && tokenBoundary(title,token)) {
    char quote=token[5];
    if(quote=='\"' || quote=='\'') {
      const char* begin=token+6;
      const char* end=strchr(begin,quote);
      if(end && end>begin) {
        char before[96];
        copyRange(before,sizeof(before),title,size_t(token-title));
        trim(before);
        size_t n=strlen(before);
        while(n && (before[n-1]=='-' || before[n-1]==':' || before[n-1]==' ')) before[--n]=0;
        trim(before);
        if(!artist[0] && before[0]) copyText(artist,artistCap,before);
        char extracted[160];
        copyRange(extracted,sizeof(extracted),begin,size_t(end-begin));
        copyText(title,titleCap,extracted);
        trim(title);
      }
    }
  }
  trimTrailingTags(title);
  // A plain "Artist - Title" stream can be split if MPD has no Artist tag.
  const char* divider=strstr(title," - ");
  if(divider) {
    char candidate[128];
    copyRange(candidate,sizeof(candidate),title,size_t(divider-title));
    trim(candidate);
    if(candidate[0] && (!artist[0] || equalsIgnoreCase(artist,candidate))) {
      if(!artist[0]) copyText(artist,artistCap,candidate);
      char remainder[160];copyText(remainder,sizeof(remainder),divider+3);
      trim(remainder);copyText(title,titleCap,remainder);
    }
  }
  trimTrailingTags(title);
}

} // namespace radio
} // namespace crt
