#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include <unistd.h>

typedef struct { char *d; size_t n; } B;

static size_t w(void *p, size_t s, size_t n, void *u) {
 size_t r = s * n;
 B *b = u;
 char *t = realloc(b->d, b->n + r + 1);
 if (!t) return 0;
 b->d = t;
 memcpy(b->d + b->n, p, r);
 b->n += r;
 b->d[b->n] = 0;
 return r;
}

static char *dupstr(const char *s) {
 size_t n = strlen(s);
 char *d = malloc(n + 1);
 if (!d) return NULL;
 memcpy(d, s, n + 1);
 return d;
}

static char *read_stdin(void) {
 B b = {0};
 char chunk[4096];

 for (;;) {
  size_t n = fread(chunk, 1, sizeof(chunk), stdin);
  if (!n) break;
  if (!w(chunk, 1, n, &b)) {
   free(b.d);
   return NULL;
  }
 }

 if (!b.d) return dupstr("");
 return b.d;
}

static void put_utf8(unsigned code) {
 if (code <= 0x7f) putchar((int)code);
 else if (code <= 0x7ff) {
  putchar((int)(0xc0 | (code >> 6)));
  putchar((int)(0x80 | (code & 0x3f)));
 } else if (code <= 0xffff) {
  putchar((int)(0xe0 | (code >> 12)));
  putchar((int)(0x80 | ((code >> 6) & 0x3f)));
  putchar((int)(0x80 | (code & 0x3f)));
 } else {
  putchar((int)(0xf0 | (code >> 18)));
  putchar((int)(0x80 | ((code >> 12) & 0x3f)));
  putchar((int)(0x80 | ((code >> 6) & 0x3f)));
  putchar((int)(0x80 | (code & 0x3f)));
 }
}

static int hexval(char c) {
 if (c >= '0' && c <= '9') return c - '0';
 if (c >= 'a' && c <= 'f') return c - 'a' + 10;
 if (c >= 'A' && c <= 'F') return c - 'A' + 10;
 return -1;
}

static int read_u4(const char *s, unsigned *code) {
 unsigned v = 0;
 for (int i = 0; i < 4; i++) {
  int h = hexval(s[i]);
  if (h < 0) return 0;
  v = (v << 4) | (unsigned)h;
 }
 *code = v;
 return 1;
}

static int print_json_string(const char *s) {
 for (size_t i = 0; s[i]; i++) {
  if (s[i] == '"') {
   putchar('\n');
   return 1;
  }

  if (s[i] != '\\') {
   putchar((unsigned char)s[i]);
   continue;
  }

  i++;
  if (!s[i]) return 0;

  switch (s[i]) {
   case '"': putchar('"'); break;
   case '\\': putchar('\\'); break;
   case '/': putchar('/'); break;
   case 'b': putchar('\b'); break;
   case 'f': putchar('\f'); break;
   case 'n': putchar('\n'); break;
   case 'r': putchar('\r'); break;
   case 't': putchar('\t'); break;
   case 'u': {
    unsigned code;
    if (!read_u4(s + i + 1, &code)) return 0;
    put_utf8(code);
    i += 4;
    break;
   }
   default:
    return 0;
  }
 }

 return 0;
}

int main(int c, char **v) {
 int piped = !isatty(0);
 char *text = NULL;
 char *lang_url = NULL;
 const char *tl = "fr";
 CURL *h;
 char *e = NULL;
 char *post = NULL;
 B b = {0};
 struct curl_slist *headers = NULL;
 int rc = 1;

 if (piped) {
  text = read_stdin();
  if (c > 1) tl = v[1];
 } else if (c > 1) {
  text = dupstr(v[1]);
  if (c > 2) tl = v[2];
 } else {
  fprintf(stderr, "usage: %s <text> [lang]\n", v[0]);
  fprintf(stderr, "   or: ... | %s [lang]\n", v[0]);
  return 1;
 }

 if (!text) {
  fprintf(stderr, "failed to read input\n");
  return 1;
 }

 h = curl_easy_init();
 if (!h) {
  free(text);
  return 1;
 }

 e = curl_easy_escape(h, text, 0);
 if (!e) goto done;

 post = malloc(strlen(e) + 3);
 if (!post) goto done;
 sprintf(post, "q=%s", e);

 headers = curl_slist_append(headers, "Content-Type: application/x-www-form-urlencoded");
 curl_easy_setopt(h, CURLOPT_WRITEFUNCTION, w);
 curl_easy_setopt(h, CURLOPT_WRITEDATA, &b);
 curl_easy_setopt(h, CURLOPT_HTTPHEADER, headers);
 curl_easy_setopt(h, CURLOPT_POSTFIELDS, post);
 curl_easy_setopt(h, CURLOPT_POSTFIELDSIZE, (long)strlen(post));

 {
  size_t url_len = strlen(tl) + 80;
  lang_url = malloc(url_len);
  if (!lang_url) goto done;
  snprintf(lang_url, url_len,
   "https://translate.googleapis.com/translate_a/single?client=gtx&sl=auto&tl=%s&dt=t",
   tl);
 }

 curl_easy_setopt(h, CURLOPT_URL, lang_url);

 if (curl_easy_perform(h) == CURLE_OK && b.d) {
  char *s = strstr(b.d, "[[[\"");
  if (s && print_json_string(s + 4)) rc = 0;
 }

done:
 curl_free(e);
 free(post);
 free(lang_url);
 curl_slist_free_all(headers);
 curl_easy_cleanup(h);
 free(b.d);
 free(text);
 return rc;
}
