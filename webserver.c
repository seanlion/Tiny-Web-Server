/*
tiny.c
static and dynamic content 서빙하는 간단한 웹서버.
*/

#include "csapp.h"
void op_transaction(int fd);
int read_requesthdrs(rio_t *rp, int log, char *method);
int parse_uri(char *uri, char *filename, char *cgiargs);
void serve_static(int fd, char *filename, int filesize, char *method);
void get_filetype(char *filename, char *filetype);
void serve_dynamic(int fd, char *filename, char *cgiargs);
void client_error(int fd, char *cause, char *errnum, 
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
	op_transaction(connfd);                                             //line:netp:tiny:doit
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
void op_transaction(int fd) 
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
 
    Rio_readlineb(&rio, buf, MAXLINE);
    printf("Request headers:\n");
    printf("%s", buf);
    // 파일 경로를 선언하고, Open 함수로 파일 없으면 생성. 그 파일에 buf 내용 넣기
    if ((log = Open("request.txt", O_RDWR | O_TRUNC | O_CREAT, S_IRWXO | S_IRWXU | S_IRWXG)) == -1)
    {
    	printf("error");
    }
    Rio_writen(log, buf, n); 
    printf("buf from Rio fd: %s",buf);
    sscanf(buf, "%s %s %s", method, uri, version);       //line:netp:doit:parserequest
    if ( !(strcasecmp(method, "GET")==0 || strcasecmp(method, "HEAD")==0 || strcasecmp(method,"POST")==0 ) ) {                     //line:netp:doit:beginrequesterr
        client_error(fd, method, "501", "Not Implemented",
                    "Tiny does not implement this method");
        return;
    }                                                    //line:netp:doit:endrequesterr
    int param_len = read_requesthdrs(&rio, log, method);
    Rio_readnb(&rio,buf,param_len); // content_length만큼 버퍼에 보내기
    /* Parse URI from GET request */
    
    is_static = parse_uri(uri, filename, cgiargs);       
    if (stat(filename, &sbuf) < 0) {                     //line:netp:doit:beginnotfound
	    client_error(fd, filename, "404", "Not found",
		    "Tiny couldn't find this file");
	    return;
    }                                                    
    if (is_static) { /* Serve static content */          
	if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)) { //line:netp:doit:readable
	    client_error(fd, filename, "403", "Forbidden",
			"Tiny couldn't read the file");
	    return;
	}
	static_serve(fd, filename, sbuf.st_size, method);        //line:netp:doit:servestatic
    }
    else { /* Serve dynamic content */
	if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode)) { //line:netp:doit:executable
	    client_error(fd, filename, "403", "Forbidden",
			"Tiny couldn't run the CGI program");
	    return;
	}
    // GET으로 할지 POST로 할지
    if (strcasecmp(method,"GET") == 0)
        dynamic_serve(fd, filename, cgiargs);            //line:netp:doit:servedynamic
    else 
        dynamic_serve(fd, filename, buf); // buf에 담긴걸 serve_dynamic 함수에 넘긴다.
    }
}
/* $end doit */

/* $begin read_requesthdrs */
int read_requesthdrs(rio_t *rp, int log, char* method) 
{
    char buf[MAXLINE];
    int len=0;
    size_t n;

    do {
        Rio_readlineb(rp,buf,MAXLINE);
        printf("%s",buf);
        if (strcasecmp(method,"POST")==0 && strncasecmp(buf,"Content-Length:",15)==0) // content-length 뒤에 있는거 숫자 가져오기
            printf("buf: %s",buf);
            printf("len: %d",len);
            sscanf(buf,"Content-length: %d",&len);
    } while(strcmp(buf,"\r\n")); // 버퍼가 한 줄 다 읽을때까지
    return len; // 읽은 길이 리턴
}
/* $end read_requesthdrs */

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

/* $begin serve_static */
void static_serve(int fd, char *filename, int filesize, char* method )
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

/* $begin serve_dynamic */
void dynamic_serve(int fd, char *filename, char *cgiargs) 
{
    char buf[MAXLINE], *emptylist[] = { NULL };
    /* Return first part of HTTP response */
    sprintf(buf, "HTTP/1.0 200 OK\r\n"); 
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Server: Tiny Web Server\r\n");
    Rio_writen(fd, buf, strlen(buf));
    if (Fork() == 0) { /* Child */ //line:netp:servedynamic:fork
	/* Real server would set all CGI vars here */
            setenv("QUERY_STRING", cgiargs, 1); //line:netp:servedynamic:setenv
            // Dup2(fd, STDOUT_FILENO);         /* Redirect stdout to client */ //
            Dup2(fd, STDIN_FILENO); 
            Execve(filename, emptylist, environ); /* Run CGI program */ //line:netp:servedynamic:execve
    }
    Wait(NULL); /* Parent waits for and reaps child */ //line:netp:servedynamic:wait
}
/* $end serve_dynamic */

/* $begin clienterror */
void client_error(int fd, char *cause, char *errnum, 
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
