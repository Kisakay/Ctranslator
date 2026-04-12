#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include <unistd.h>

typedef struct{char*d;size_t n;}B;

static size_t w(void*p,size_t s,size_t n,void*u){
 size_t r=s*n;B*b=u;
 char*t=realloc(b->d,b->n+r+1);
 if(!t)return 0;
 b->d=t;
 memcpy(b->d+b->n,p,r);
 b->n+=r;
 b->d[b->n]=0;
 return r;
}

int main(int c,char**v){
 char in[4096];

 // stdin pipe support
 if(!isatty(0)){
  fread(in,1,sizeof(in)-1,stdin);
 } else if(c>1){
  strncpy(in,v[1],sizeof(in)-1);
 } else return 1;

 char *text = in;
 char *tl = c>2 ? v[2] : (c>1 && isatty(0) ? v[1] : "fr");

 // if pipe → arg2 = language
 if(!isatty(0) && c>1) tl = v[1];

 CURL*h=curl_easy_init();
 if(!h)return 1;

 char*e=curl_easy_escape(h,text,0);
 char url[1024];

 snprintf(url,sizeof url,
  "https://translate.googleapis.com/translate_a/single?client=gtx&sl=auto&tl=%s&dt=t&q=%s",
  tl,e);

 B b={0};

 curl_easy_setopt(h,CURLOPT_URL,url);
 curl_easy_setopt(h,CURLOPT_WRITEFUNCTION,w);
 curl_easy_setopt(h,CURLOPT_WRITEDATA,&b);

 if(!curl_easy_perform(h)&&b.d){
  char*s=strstr(b.d,"[[[\"");
  if(s){
   s+=4;
   for(char*p=s;*p;p++){
    if(*p=='"' && p[-1]!='\\'){ *p=0; puts(s); break; }
   }
  }
 }

 curl_free(e);
 curl_easy_cleanup(h);
 free(b.d);
}