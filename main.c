#include "../nhttp/nhttp.h"
#include <time.h>

/* TODO:
-auto redirect when posting
-only show 5 recent replies to threads on main page
-thread catalog
*/

static const char
home_head[] =
    "<head>"
    "<title>bbs</title>"
    "<style>"
    ".thread {"
    "border:1px inset #000;"
    "padding:5px;"
    "margin-top:7px;"
    "display:block;"
    "}"
    "</style>"
    "</head>"
    "<body bgcolor=\"white\">"
    "<p align=\"center\"><b>str8c talk</b></p>",

home_end[] =
    "<br>"
    "<b>New Thread</b>"
    "<form method=\"post\" action=\"post\">"
    "Subject: <input type=\"text\" name=\"t\"><br>"
    "Name: <input type=\"text\" name=\"n\"> "
    "<input type=\"submit\" value=\"New thread\"><br>"
    "<textarea cols=\"64\" rows=\"5\" name=\"p\"></textarea>"
    "</form>"
    "</body>",

thread_head[] =
    "<div class=\"thread\">"
    "[%u:%u] <a href=\"%s/\">%s</a>",

thread_end[] =
    "<br>"
    "<form method=\"post\" action=\"%s/post\">"
    "Name: <input type=\"text\" name=\"n\">"
    "<label><input type=\"checkbox\" name=\"d\">Don't bump </label>"
    "<input type=\"submit\" value=\"Reply\"><br>"
    "<textarea cols=\"64\" rows=\"5\" name=\"p\"></textarea>"
    "</form>"
    "</div>",

threadmain_head[] =
    "<head>"
    "<title>%s</title>"
    "</head>"
    "<body bgcolor=\"white\">"
    "<a href=\"../\">back</a><br>"
    "<b>%s</b><br>",

threadmain_end[] =
    "<br>"
    "<form method=\"post\" action=\"post\">"
    "Name: <input type=\"text\" name=\"n\">"
    "<label><input type=\"checkbox\" name=\"d\">Don't bump </label>"
    "<input type=\"submit\" value=\"Reply\"><br>"
    "<textarea cols=\"64\" rows=\"5\" name=\"p\"></textarea>"
    "</form>"
    "</body>",

post_submit[] = "Posted. <a href=\"./\">go to thread</a><br><a href=\"../\">go to front page</a>",
thread_submit[] = "Posted. <a href=\"./\">go to front page</a>",


post_omitted[] = "<br>%u posts omitted. <a href=\"%s/\">Click to see all</a><br><br>",
post_format[] =
    "<div>"
        "<b>%u</b> Name: <b>%s</b> : %s<br>%s"
    "</div>";


typedef struct {
    uint32_t time, name, content;
    uint16_t upvotes, downvotes;
} POST;

typedef struct {
    uint32_t id, title, npost, bumptime;
    POST post[255];
} THREAD;

static char text[1024 * 1024], *textp = text; //1m
static THREAD *thread[8192]; //64k
static int nthread;

static void commit_text(char *base)
{
    FILE *file;

    file = fopen("text", "ab");
    if(!file) {
        //fatal error
    }

    fwrite(base, 1, textp - base, file);
    fclose(file);
}

static void insert(THREAD *t)
{
    memmove(thread + 1, thread, nthread * sizeof(*thread));
    thread[0] = t;
    nthread++;
}

static void commit_thread(THREAD *t)
{
    FILE *file;

    file = fopen("threads", "ab");
    if(!file) {
        //fatal error
    }

    fwrite(t, 1, sizeof(THREAD), file);
    insert(t);
}

static THREAD** threadbyid(int id)
{
    int i;

    for(i = 0; i != nthread; i++) {
        if(thread[i]->id == id) {
            return &thread[i];
        }
    }

    return NULL;
}

static void bump(THREAD *t, THREAD **tp, uint32_t time)
{
    memmove(thread + 1, thread, (tp - thread) * sizeof(*thread));
    thread[0] = t;
    t->bumptime = time;
}

static void insertload(THREAD *t)
{
    int i;

    for(i = 0; i < nthread; i++) {
        if(t->bumptime > thread[i]->bumptime) {
            /* insert */
            memmove(thread + i + 1, thread + i, (nthread - i) * sizeof(*thread));
            thread[i] = t;
            nthread++;
            return;
        }
    }
    thread[nthread++] = t;
}

static int tohex(char ch)
{
    if(ch >= '0' && ch <= '9') {
        return ch - '0';
    }

    if(ch >= 'A' && ch <= 'F') {
        return ch - 'A' + 10;
    }

    return -1;
}

static int addtextencoded(const char **str)
{
    const char *p;
    char ch;
    uint32_t r;
    int h1, h2;

    p = *str + 1;
    if(*p != '=' || !*p) {
        return -2;
    }
    p++;

    r = textp - text;
    if(*p == '&' || !*p) {
        *str = p + (*p != 0);
        return -1;
    }

    do {
        ch = *p++;
        if(ch == '+') {
            ch = ' ';
        } else if(ch == '%') {
            if((h1 = tohex(p[0])) >= 0 && (h2 = tohex(p[1])) >= 0) {
                p += 2;
                ch = (h1 << 4) | h2;
                if(ch == '&') {
                    *textp++ = '&';
                    *textp++ = 'a';
                    *textp++ = 'm';
                    *textp++ = 'p';
                    *textp++ = ';';
                    continue;
                }
            }
        }

        if(ch == '<' || ch == '>') {
            *textp++ = '&';
            *textp++ = (ch == '<') ? 'l' : 'g';
            *textp++ = 't';
            *textp++ = ';';
            continue;
        }

        *textp++ = ch; //
    } while(*p != '&' && *p);
    *textp++ = 0;

    *str = p + (*p != 0);
    return r;
}

static char* writepost(char *str, int j, POST *p)
{
    char *s;
    time_t tm;

    tm = p->time;
    s = (p->name == ~0) ? "Anonymous" : (text + p->name);
    str += sprintf(str, post_format, j, s, ctime(&tm), text + p->content);

    return str;
}

static char* homepage(char *str)
{
    char tmp[16];
    int i, j;
    THREAD *t;

    str += sprintf(str, home_head);

    for(i = 0; i != nthread; i++) {
        t = thread[i];
        sprintf(tmp, "%u", t->id);
        str += sprintf(str, thread_head, i, t->npost, tmp, text + t->title);

        str = writepost(str, 0, &t->post[0]);
        if(t->npost > 6) {
            str += sprintf(str, post_omitted, t->npost - 6, tmp);
            j = t->npost - 5;
        } else {
            j = 1;
        }

        for(; j != t->npost; j++) {
            str = writepost(str, j, &t->post[j]);
        }

        str += sprintf(str, thread_end, tmp);
    }

    str += sprintf(str, home_end);
    return str;
}

static char* threadpage(char *str, THREAD *t)
{
    char *s;
    int j;

    s = text + t->title;
    str += sprintf(str, threadmain_head, s, s);

    for(j = 0; j != t->npost; j++) {
        str = writepost(str, j, &t->post[j]);
    }

    str += sprintf(str, threadmain_end);
    return str;
}

int export(nbbs, getpage)(PAGEINFO *data, const char *path, const char *post, int postlen)
{
    char *str;
    int i;
    int name, title, text;
    bool dontbump;
    uint32_t ctime;
    THREAD *t, **tp;
    POST *p;
    FILE *file;

    if(!*path) {
        return homepage(data->buf) - data->buf;
    }

    if(post && !strcmp(path, "post")) {
        name = title = text = -1;

        str = textp;
        while(*post) {
            if(*post == 'n') {
                name = addtextencoded(&post);
                if(name == -2) {
                    return -1;
                }
            } else if(*post == 'p') {
                text = addtextencoded(&post);
                if(text < 0) {
                    return -1;
                }
            } else if(*post == 't') {
                title = addtextencoded(&post);
                if(title < 0) {
                    return -1;
                }
            } else {
                break;
            }
        }

        if(name == -2 || text < 0 || title < 0) {
            textp = str;
            return -1;
        }

        ctime = time(NULL);

        t = malloc(sizeof(THREAD));
        t->id = nthread;
        t->title = title;
        t->npost = 1;
        t->bumptime = ctime;
        p = &t->post[0];
        p->time = ctime;
        p->name = name;
        p->content = text;
        p->upvotes = 0;
        p->downvotes = 0;

        commit_text(str);
        commit_thread(t);


        return sprintf(data->buf, thread_submit);
    }

    i = strtol(path, (char**)&str, 0);
    if(*str++ != '/') {
        return -1;
    }

    if(!(tp = threadbyid(i))) {
        return -1;
    }
    t = *tp;

    if(post && !strcmp(str, "post")) {
        if(t->npost == sizeof(t->post) / sizeof(*t->post)) {
            return -1;
        }

        name = text = -1;
        dontbump = 0;
        str = textp;
        while(*post) {
            if(*post == 'n') {
                name = addtextencoded(&post);
                if(name == -2) {
                    return -1;
                }
            } else if(*post == 'p') {
                text = addtextencoded(&post);
                if(text < 0) {
                    return -1;
                }
            } else if(*post == 'd') {
                dontbump = 1;
                while(*post && *post++ != '&');
            } else {
                break;
            }
        }

        if(name == -2 || text < 0 || (textp - str) > 256) {
            textp = str;
            return -1;
        }

        commit_text(str);

        ctime = time(NULL);
        if(!dontbump) {
            bump(t, tp, ctime);
        }
        p = &t->post[t->npost++];
        p->time = ctime;
        p->name = name;
        p->content = text;
        p->upvotes = 0;
        p->downvotes = 0;

        file = fopen("threads", "rb+");
        if(!file) {
            //fatal error
        }

        /* write npost + bumptime */
        fseek(file, i * sizeof(THREAD) + 8, SEEK_SET);
        fwrite(&t->npost, 1, 8, file);

        /* write the post itself */
        fseek(file, i * sizeof(THREAD) + sizeof(POST) * t->npost, SEEK_SET); //t->npost + 1 intentional
        fwrite(p, 1, sizeof(POST), file);
        fclose(file);


        return sprintf(data->buf, post_submit);
    }

    return threadpage(data->buf, t) - data->buf;
}

void __attribute__ ((constructor)) init(void)
{
    FILE *file;
    THREAD *t;

    file = fopen("text", "rb");
    if(file) {
        textp += fread(text, 1, sizeof(text), file);
        fclose(file);
    }

    file = fopen("threads", "rb");
    if(file) {
        do {
            t = malloc(sizeof(THREAD));
            if(fread(t, sizeof(THREAD), 1, file) != 1) {
                break;
            }

            insertload(t);
        } while(1);
        free(t);
        fclose(file);
    }
}
