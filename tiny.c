/*
tiny.c
static and dynamic content 서빙하는 간단한 웹서버.
*/

/* $begin tinymain */
/*
 * tiny.c - A simple, iterative HTTP/1.0 Web server that uses the 
 *     GET method to serve static and dynamic content.
 *
 * Updated 11/2019 droh 
 *   - Fixed sprintf() aliasing issue in serve_static(), and clienterror().
 */
#include "csapp.h"
void doit(int fd);
int read_requesthdrs(rio_t *rp, int log, char *method);
int parse_uri(char *uri, char *filename, char *cgiargs);
void serve_static(int fd, char *filename, int filesize, char *method);
void get_filetype(char *filename, char *filetype);
void serve_dynamic(int fd, char *filename, char *cgiargs);
void clienterror(int fd, char *cause, char *errnum, 
		 char *shortmsg, char *longmsg);
void echo(int connfd); 
int main(int argc, char **argv) 
{
    int listenfd, connfd;
    char hostname[MAXLINE], port[MAXLINE];
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;
    /* Check command line args */
    if (argc != 2) {
	fprintf(stderr, "usage: %s <port>\n", argv[0]);
	exit(1);
    }
    listenfd = Open_listenfd(argv[1]);
    while (1) {
	clientlen = sizeof(clientaddr);
	connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen); //line:netp:tiny:accept
        Getnameinfo((SA *) &clientaddr, clientlen, hostname, MAXLINE, 
                    port, MAXLINE, 0);
        printf("Accepted connection from (%s, %s)\n", hostname, port);
	doit(connfd);                                             //line:netp:tiny:doit
	//echo(connfd);
	Close(connfd);                                            //line:netp:tiny:close
    }
}
/* $end tinymain */
void echo(int connfd) 
{
    size_t n; 
    char buf[MAXLINE]; 
    rio_t rio;
    Rio_readinitb(&rio, connfd);
    while((n = Rio_readlineb(&rio, buf, MAXLINE)) != 0) { //line:netp:echo:eof
	printf("server received %d bytes\n", (int)n);
	Rio_writen(connfd, buf, n);
    }
}
/*
 * doit - handle one HTTP request/response transaction
 */
/* $begin doit */
void doit(int fd) 
{
    int is_static;
    struct stat sbuf;
    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    char filename[MAXLINE], cgiargs[MAXLINE];
    rio_t rio;
    int log;
    size_t n;
    /* Read request line and headers */
    Rio_readinitb(&rio, fd);
    // while((n = Rio_readlineb(&rio, buf, MAXLINE)) !=0){ // rio 파일에서 내용 읽고 buf에 카피
    //     printf("Request headers:\n");
    //     printf("내용 : %s",buf);
    //     Rio_writen(fd,buf,n);
    // }
    Rio_readlineb(&rio, buf, MAXLINE);
    printf("Request headers:\n");
    printf("%s", buf);
    // 파일 경로를 선언하고, Open 함수로 파일 없으면 생성. 그 파일에 buf 내용 넣기
    if ((log = Open("request.txt", O_RDWR | O_TRUNC | O_CREAT, S_IRWXO | S_IRWXU | S_IRWXG)) == -1)
    {
    	printf("error");
    }
    Rio_writen(log, buf, n); 
    //printf("buf from Rio fd: %s",buf);
    sscanf(buf, "%s %s %s", method, uri, version);       //line:netp:doit:parserequest
    if ( !(strcasecmp(method, "GET")==0 || strcasecmp(method, "HEAD")==0 || strcasecmp(method,"POST")==0 ) ) {                     //line:netp:doit:beginrequesterr
        clienterror(fd, method, "501", "Not Implemented",
                    "Tiny does not implement this method");
        return;
    }                                                    //line:netp:doit:endrequesterr
    int param_len = read_requesthdrs(&rio, log, method);
    printf("paramlen : %d\n",param_len);
    Rio_readnb(&rio,buf,param_len); // content_length만큼 버퍼에 보내기
    /* Parse URI from GET request */
    
    is_static = parse_uri(uri, filename, cgiargs);       
    if (stat(filename, &sbuf) < 0) {                     //line:netp:doit:beginnotfound
	    clienterror(fd, filename, "404", "Not found",
		    "Tiny couldn't find this file");
	    return;
    }                                                    
    if (is_static) { /* Serve static content */          
	if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)) { //line:netp:doit:readable
	    clienterror(fd, filename, "403", "Forbidden",
			"Tiny couldn't read the file");
	    return;
	}
	serve_static(fd, filename, sbuf.st_size, method);        //line:netp:doit:servestatic
    }
    else { /* Serve dynamic content */
	if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode)) { //line:netp:doit:executable
	    clienterror(fd, filename, "403", "Forbidden",
			"Tiny couldn't run the CGI program");
	    return;
	}
    // GET으로 할지 POST로 할지
    if (strcasecmp(method,"GET") == 0)
        serve_dynamic(fd, filename, cgiargs);            //line:netp:doit:servedynamic
    else 
        serve_dynamic(fd, filename, buf); // buf에 담긴걸 serve_dynamic 함수에 넘긴다.
    }
}
/* $end doit */
/*
 * read_requesthdrs - read HTTP request headers
 */
/* $begin read_requesthdrs */
int read_requesthdrs(rio_t *rp, int log, char* method) 
{
    char buf[MAXLINE];
    char slen[MAXLINE];
    int  len=0;
    char *ptr;
    size_t n;
    // Rio_readlineb(rp, buf, MAXLINE);
    // printf("%s", buf);
    // while(strcmp(buf, "\r\n")) {          //line:netp:readhdrs:checkterm
	// n = Rio_readlineb(rp, buf, MAXLINE);
	// printf("%s", buf);
    // 	Rio_writen(log, buf, n);
    // }
    do {
        Rio_readlineb(rp,buf,MAXLINE);
        printf("%s\n",buf);
        if (strcasecmp(method,"POST")==0 && strncasecmp(buf,"Content-Length:",15)==0){ // content-length 뒤에 있는거 숫자 가져오기
            printf("buf: %s\n",buf);
            ptr = index(buf,':');
	    ptr+=2;
	    printf("ptr:%p\n",ptr);
    	    len=atoi(ptr); 
 	    //sscanf(buf,"Content-length: %d\n",&len);
            printf("len:%p,  %d\n",&len,len);
     }
    }while(strcmp(buf,"\r\n")); // 버퍼가 한 줄 다 읽을때까지
    //slen = atoi(len); 
    return len; // 읽은 길이 리턴
}
/* $end read_requesthdrs */
/*
 * parse_uri - parse URI into filename and CGI args
 *             return 0 if dynamic content, 1 if static
 */
/* $begin parse_uri */
int parse_uri(char *uri, char *filename, char *cgiargs) 
{
    char *ptr;
    if (!strstr(uri, "cgi-bin")) {  /* Static content */ //line:netp:parseuri:isstatic
	strcpy(cgiargs, "");                             //line:netp:parseuri:clearcgi
	strcpy(filename, ".");                           //line:netp:parseuri:beginconvert1
	strcat(filename, uri);                           //line:netp:parseuri:endconvert1
	if (uri[strlen(uri)-1] == '/')                   //line:netp:parseuri:slashcheck
	    strcat(filename, "home.html");               //line:netp:parseuri:appenddefault
	return 1;
    }
    else {  /* Dynamic content */                        //line:netp:parseuri:isdynamic
	ptr = index(uri, '?');                           //line:netp:parseuri:beginextract
	if (ptr) {
	    strcpy(cgiargs, ptr+1);
	    *ptr = '\0';
	}
	else 
	    strcpy(cgiargs, "");                         //line:netp:parseuri:endextract
	strcpy(filename, ".");                           //line:netp:parseuri:beginconvert2
	strcat(filename, uri);                           //line:netp:parseuri:endconvert2
	return 0;
    }
}
/* $end parse_uri */
/*
 * serve_static - copy a file back to the client 
 */
/* $begin serve_static */
void serve_static(int fd, char *filename, int filesize, char* method )
{
    int srcfd;
    char *srcp, filetype[MAXLINE], buf[MAXBUF];
    /* Send response headers to client */
    get_filetype(filename, filetype);    //line:netp:servestatic:getfiletype
    sprintf(buf, "HTTP/1.0 200 OK\r\n"); //line:netp:servestatic:beginserve
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Server: Tiny Web Server\r\n");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-length: %d\r\n", filesize);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-type: %s\r\n\r\n", filetype);
    Rio_writen(fd, buf, strlen(buf));    //line:netp:servestatic:endserve
    if (!(strcasecmp(method, "GET"))){
    /* Send response body to client */
    srcfd = Open(filename, O_RDONLY, 0); //line:netp:servestatic:open
    // char * mymem = (char*)malloc(filesize);
    // Rio_readn(srcfd, mymem, filesize);
    srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0); //line:netp:servestatic:mmap
    Close(srcfd);                       //line:netp:servestatic:close
    Rio_writen(fd, srcp, filesize);  
  
    // Rio_writen(fd, mymem, filesize);     //line:netp:servestatic:write
    Munmap(srcp, filesize);             //line:netp:servestatic:munmap
    // free(mymem);
}
}
/*
 * get_filetype - derive file type from file name
 */
void get_filetype(char *filename, char *filetype) 
{
    if (strstr(filename, ".html"))
	strcpy(filetype, "text/html");
    else if (strstr(filename, ".gif"))
	strcpy(filetype, "image/gif");
    else if (strstr(filename, ".png"))
	strcpy(filetype, "image/png");
    else if (strstr(filename, ".jpg"))
	strcpy(filetype, "image/jpeg");
    else if (strstr(filename, ".mp4"))
    strcpy(filetype,"video/mp4");
    else
	strcpy(filetype, "text/plain");
}  
/* $end serve_static */
/*
 * serve_dynamic - run a CGI program on behalf of the client
 */
/* $begin serve_dynamic */
void serve_dynamic(int fd, char *filename, char *cgiargs) 
{
    char buf[MAXLINE], *emptylist[] = { NULL };
    /* Return first part of HTTP response */
    sprintf(buf, "HTTP/1.0 200 OK\r\n"); 
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Server: Tiny Web Server\r\n");
    Rio_writen(fd, buf, strlen(buf));
    if (Fork() == 0) { /* Child */ //line:netp:servedynamic:fork
	/* Real server would set all CGI vars here */
            setenv("QUERY", cgiargs, 1); //line:netp:servedynamic:setenv
            // Dup2(fd, STDOUT_FILENO);         /* Redirect stdout to client */ //
            Dup2(fd, STDOUT_FILENO); 
            Execve(filename, emptylist, environ); /* Run CGI program */ //line:netp:servedynamic:execve
    }
    Wait(NULL); /* Parent waits for and reaps child */ //line:netp:servedynamic:wait
}
/* $end serve_dynamic */
/*
 * clienterror - returns an error message to the client
 */
/* $begin clienterror */
void clienterror(int fd, char *cause, char *errnum, 
		 char *shortmsg, char *longmsg) 
{
    char buf[MAXLINE];
    /* Print the HTTP response headers */
    sprintf(buf, "HTTP/1.1 %s %s\r\n", errnum, shortmsg);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-type: text/html\r\n\r\n");
    Rio_writen(fd, buf, strlen(buf));
    /* Print the HTTP response body */
    sprintf(buf, "<html><title>Tiny Error</title>");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "<body bgcolor=""ffffff"">\r\n");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "%s: %s\r\n", errnum, shortmsg);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "<p>%s: %s\r\n", longmsg, cause);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "<hr><em>The Tiny Web server</em>\r\n");
    Rio_writen(fd, buf, strlen(buf));
}
/* $end clienterror */
