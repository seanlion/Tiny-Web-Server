#include "../csapp.h"

int main(void){
    char *buf, *p, *p1;
    char arg1[MAXLINE], arg2[MAXLINE],content[MAXLINE];
    int n1=0, n2=0;
    // char n1[MAXLINE], n2[MAXLINE];

    /* extract the two arguments*/
    if ((buf=getenv("QUERY")) != NULL){
    	// p = strchr(buf, '&');
        // *p = '\0';
        // strcpy(arg1, buf);
        // p1 = strchr(arg1, '=');
        // strcpy(arg1, p1+1);
        // strcpy(arg2, p+1);
        // p1 = strchr(arg2, '=');
        // strcpy(arg2, p1+1);
        // printf("\narg1:%s\n", arg1);
        // printf("\naarg2:%s\n", arg2);
        // n1 = atoi(arg1);
        // n2 = atoi(arg2);
        p = strchr(buf, '&');
        *p = '\0';
        sscanf(buf, "n1=%d", &n1);
        sscanf(p+1, "n2=%d", &n2);	
    }

    /* Make the response body*/
    sprintf(content, "QUERY_STRING=%s\n",buf);
    sprintf(content, "%sWelcome to add.com: ",content);
    sprintf(content, "%sTHE Internet addition portal. \r\n<p>", content);
    // sprintf(content,"%sThe answer is: %d + %d= %d\r\n<p>", content, n1,n2,n1+n2);
    sprintf(content,"%sThe answer is: %d + %d=%d\r\n<p>", content, n1,n2, n1+n2);
    sprintf(content,"%sThanks for visiting!\r\n",content);

    /* Generate the HTTP response*/
    printf("Connection: close\r\n");
    printf("Content-length : %d\r\n", (int)strlen(content));
    printf("Content-type: text/html\r\n\r\n");
    printf("%s",content);
    fflush(stdout);

    exit(0);
}
