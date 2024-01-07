#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <netinet/in.h>
#include <pthread.h>
#include <errno.h>
#include <dirent.h>

#define PORT 8080
typedef int FILE_DESC;

char *handle_post(char *url, char *body) {
    char *ret, *uurl;
    const char *template_200, *template_409;
    FILE *fp;
    uurl = calloc(1,strlen("./available")+strlen(url)+1);
    strcat(uurl, "./available");
    if(*url - '/') strcat(uurl, "/");
    strcat(uurl, url);
    template_409 = "HTTP/1.1 409 Conflict\r\n"
                   "Content-Type: text/html\r\n"
                   "<html><p>error 409 (conflict), post already exists"
                   "</p></html>";
    template_200 = "HTTP/1.1 200 OK\r\n";
    if(!memcmp(uurl, "./available/", 13)) {
        fprintf(stderr, "409 response sending\n");
        ret = malloc(strlen(template_409)+1);
        strcpy(ret, template_409);
        return ret;
    }
    if(!access(uurl, F_OK)) {
        fprintf(stderr, "!!409 response sending\n");
        ret = malloc(strlen(template_409)+1);
        strcpy(ret, template_409);
        return ret;
    }
    printf("%s\n", body);
    fp = fopen(uurl, "w");
    fwrite(body,1,strlen(body),fp);
    fclose(fp);
    ret = malloc(strlen(template_200)+1);
    strcpy(ret, template_200);
    return ret;
}

char *get_response(char *url) {
    const char *template_200, *template_404, *template_500, *template_ls;
    char *ret, *content, *uurl, *tmp;
    FILE *fp;
    DIR  *d;
    struct dirent *dir;
    int fsz;
    uurl = calloc(1,strlen("./available")+strlen(url)+1);
    strcat(uurl, "./available");
    if(*url - '/') strcat(uurl, "/");
    strcat(uurl, url);
    template_200 = "HTTP/1.1 200 OK\r\n"
                   "Content-Type: text/html\r\n"
                   "Connection: Closed\r\n"
                   "Content-Length: %d\r\n"
                   "\r\n"
                   "%s\r\n\0";
    template_ls  = "HTTP/1.1 200 OK\r\n"
                   "Content-Type: text/plain\r\n"
                   "Connection: Closed\r\n"
                   "\r\n"
                   "%s\r\n\0";
    template_404 = "HTTP/1.1 404 Not Found\r\n";
    template_500 = "HTTP/1.1 500 Internal Server Error\r\n";
    if(!memcmp(uurl, "./available/", 13)) {
        free(uurl);
        uurl = malloc(strlen("./home.html")+1);
        strcpy(uurl, "./home.html");
    }
    if(!memcmp(uurl, "./available/list.all", 20)) {
        d = opendir("./available");
        free(uurl);
        if(d) {
            ret = calloc(1,1);
            while((dir=readdir(d)) != NULL) {
                if(dir->d_name[0] == '.') continue;
                ret = realloc(ret, strlen(ret)+strlen(dir->d_name)+2);
                strcat(ret, dir->d_name);
                strcat(ret, "\n");
            }
            tmp = malloc(strlen(ret)+strlen(template_ls)+1);
            sprintf(tmp, template_ls, ret);
            free(ret);
            ret = tmp;
            closedir(d);
            return ret;
        } else {
            fprintf(stderr, "500 response sending\n");
            ret = malloc(strlen(template_500)+1);
            strcpy(ret, template_500);
            return ret;
        }
    }
    if(access(uurl, F_OK)) {
        free(uurl);
        fprintf(stderr, "404 response sending\n");
        ret = malloc(strlen(template_404)+1);
        strcpy(ret, template_404);
        return ret;
    }

    fp=fopen(uurl, "r");
    content = calloc(1,fsz = (fseek(fp, 0L, SEEK_END), ftell(fp)));
    fseek(fp, 0L, SEEK_SET);
    fread(content, 1, fsz, fp);
    fclose(fp);

    ret = calloc(1,strlen(template_200)+strlen(content)+1);
    sprintf(ret, template_200, strlen(content), content);
    free(content);
    free(uurl);
    return ret;
}

void *help_client(void *cfdp) {
    int cfd, urllen, i, j, numval, cont_len;
    char *buf, *url, *version, *ourl, *body, tmp, *sd, num[3] = {0};
    ssize_t bcount;
    ourl = calloc(1,1);
    buf = calloc(1,BUFSIZ+1);
    cfd = *((int*)cfdp);

    bcount = recv(cfd, buf, BUFSIZ, 0);
    if(bcount > 4) {
        if(memcmp(buf, "GET", 3)) goto post;
        for(urllen=0; buf[urllen] && buf[urllen+4] != ' '; urllen++);
        if(!buf[urllen]) {
            send(cfd, "HTTP/1.1 400 Bad Request\r\n", 26, 0);
            goto finish;
        }
        url = buf+4;
        ourl = realloc(ourl, urllen+1);
        ourl[urllen]=0;
        for(i=0,j=0;i<urllen;i++,j++) {
            if(url[i] == '%') {
                if(url[i+1] < '0' || (url[i+1] > '9' && url[i+1] < 'a')
                        || (url[i+1] > 'f' && url[i+1] < 'A') ||
                        (url[i+1] > 'F')) {
                    ourl[j]=url[i];
                    continue;
                } 
                num[0] = url[++i];
                num[1] = url[++i];
                numval = strtol(num, NULL, 16);
                ourl[j] = (char)numval;
                continue;
            }
            ourl[j]=url[i];
        }
        sd = get_response(ourl);
        send(cfd, sd, strlen(sd), 0);
        free(sd);
        goto finish;
    post:

        if(memcmp(buf, "POST", 4)) goto finish;
        printf("%s\n", buf);
        for(urllen=0; buf[urllen+5] && buf[urllen+5] != ' '; urllen++);
        if(!buf[urllen]) {
            send(cfd, "HTTP/1.1 400 Bad Request\r\n", 26, 0);
            goto finish;
        }
        url = buf+5;
        ourl = realloc(ourl, urllen+1);
        for(i=0,j=0;i<urllen;i++,j++) {
            if(url[i]=='%') {
                if(url[i+1] < '0' || (url[i+1] > '9' && url[i+1] < 'a')
                        || (url[i+1] > 'f' && url[i+1] < 'A') ||
                        (url[i+1] > 'F')) {
                    ourl[j]=url[i];
                    continue;
                } 
                num[0] = url[++i];
                num[1] = url[++i];
                numval = strtol(num, NULL, 16);
                ourl[j] = (char)numval;
                continue;
            }
            ourl[j] = url[i];
        }
        printf("%s", ourl);
        for(i=0; i<BUFSIZ; i++) {
            if(!memcmp(buf+i+1, "Sec-Fetch-Site: ", 16)) {
                i+=30;
                break;
            }
        }
        printf("%s\n", buf+i);
        body = calloc(1,1);
        do {
            for(; i < BUFSIZ; i++) {
                body = realloc(body, strlen(body)+2);
                body[j=strlen(body)] = *(buf+i);
                body[j+1] = 0;
                if(!memcmp(buf+i, "</html>", 7)) {
                    body[j]=0;
                    body = realloc(body, strlen(body)+7);
                    strcat(body, "</html>");
                    break;
                }
            }
            if(i == BUFSIZ) {
                i=0;
                recv(cfd, buf, BUFSIZ, 0);
                continue;
            }
            break;
        } while(0);

        sd = handle_post(ourl, body);
        send(cfd, sd, strlen(sd), 0);
        free(sd);        

        goto finish; 
    }
finish:
    close(cfd);
    free(cfdp);
    free(ourl);
    free(buf);
    return NULL;
}

void *chkout(void*nar) {
    char c;
    if(read(STDIN_FILENO, &c, 1)) {
        if(!(c-'q')) {
            fprintf(stderr, "manual shutdown\n");
            close(*(int*)nar);
            exit(0);
        }
    }
}

int main(int argc, char **argv) {
    FILE_DESC sfd, *cfd;
    int succ;
    char c;
    socklen_t caddrlen;
    pthread_t ptid;
    struct sockaddr_in saddr, caddr;

    sfd=socket(AF_INET, SOCK_STREAM, 0);
    if(sfd < 0) {
        fprintf(stderr, "socket could not open\n");
        exit(1);
    }
    saddr.sin_family = AF_INET;
    saddr.sin_addr.s_addr = INADDR_ANY;
    saddr.sin_port = htons(PORT);

    succ = bind(sfd, (struct sockaddr*)&saddr, sizeof(saddr));
    if(succ < 0) {
        fprintf(stderr, "could not bind socket\n%s\n", strerror(errno));
        exit(1);
    }
    
    succ = listen(sfd, 16);
    if(succ < 0) {
        fprintf(stderr, "could not listen\n");
        exit(1);
    }

    pthread_create(&ptid, NULL, chkout, (void*)&sfd);
    pthread_detach(ptid);

    caddrlen = sizeof(caddr);
    while(1) {
        cfd = malloc(sizeof(int));
        *cfd = accept(sfd, (struct sockaddr*)&caddr, &caddrlen);
        if(*cfd < 0) {
            fprintf(stderr, "failed socket accept\n");
            exit(1);
        }
        printf("cfd\n");
        pthread_create(&ptid, NULL, help_client, (void*)cfd);
        pthread_detach(ptid);
    }

    fprintf(stderr, "unexplained termination\n");
}
