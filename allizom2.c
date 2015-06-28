/*
 * Course Code 350
 *
 * A Multithreaded Tiny HTTP Server
 * Demonstrating Thread Pool Management
 * Following the Thread Pool Management
 * Outline from Unix Network Programming Vol 1
 * by W. Richard Stevens.
 *
 * Project accomplished by: Abu Mohammad Omar Shehab Uddin Ayub
 *                          Reg No. 2000 330 096
 *                          Section B
 *                          3rd Year 2nd Semester
 *                          Dept of CSE, SUST
 *
 * Idea and guided by:      Mahmud Shahriar Hussain
 *                          Lecturer
 *                          Dept of CSE, SUST
 *
 * Course Instructor:       Rukhsana Tarannum Tazin
 *                          Lecturer
 *                          Dept of CSE, SUST
 */

#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define MAX_CLIENT 5
#define MAX_THREAD 3
#define MAX_REQ_HEADER 10
#define MAX         10
#define MAX_LENGTH  6
#define MIME_LENGTH 20

typedef struct {
  pthread_t threadID;
  long threadCount;
}Thread;

typedef struct {
  char hostName[100];
  char filePath[100];
}Request;

typedef struct{
  int code;
  char date[30];
  char server[30];
  char last_modified[30];
  long content_length;
  char content_type[10];
  char file_path[80];
}Reply;

Thread    thrdPool[MAX_THREAD];
Request   request[MAX_THREAD];
Reply     reply[MAX_THREAD];

int clntConnection[MAX_CLIENT], clntGet, clntPut;

pthread_mutex_t clntMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t srvrMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t srvrMutex2 = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t emitMutex2 = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t clntCondition = PTHREAD_COND_INITIALIZER;

static int numThreads;

void *request_handler(void *arg);
int find_method (char *req);
Request parse_request(char *rbuff);
Reply prepare_reply(int code, Request rq);
char *getMimeType(char  *str);
void emit_reply(int conn, Reply rply);


int main(void)
{
  int i, serverSockfd, clientSockfd;
  int serverLen, clientLen;
  struct sockaddr_in serverAddress, clientAddress, tempAddress;

  //defining number of threads
  numThreads = MAX_THREAD;

  // initializing queue parameters
  clntGet = clntPut = 0;

  //creating all the threads
  for(i = 0; i < numThreads; i++)
  {
    pthread_create(&thrdPool[i].threadID, NULL, &request_handler, (void *) i);
  } // end of for loop

  //creating the socket descriptor
  serverSockfd = socket(AF_INET, SOCK_STREAM,0);

  if(serverSockfd < 0)
  {
      error("\nError opening socket");
      exit(1);
  }

  serverAddress.sin_family = AF_INET;
  serverAddress.sin_addr.s_addr = INADDR_ANY;
  serverAddress.sin_port = htons(80);
  serverLen = sizeof(serverAddress);

  if(bind(serverSockfd, (struct sockaddr *)&serverAddress, serverLen) < 0)
  {
     printf("\nError in binding.");
     exit(1);
  }

  listen(serverSockfd, 10);

  for( ; ; )
  {
    fflush(stdout);
    //printf("\nWaiting for clients...");
    clientLen = sizeof(tempAddress);

    //printf("\n\nBlocked on accept()");
    clientSockfd = accept(serverSockfd, (struct sockaddr *)&clientAddress, &clientLen);
    //printf("\nAllocated client descriptor# %d", clientSockfd);

    pthread_mutex_lock(&srvrMutex);
    clntConnection[clntPut] = clientSockfd;
    if(++clntPut == MAX_CLIENT)
          clntPut = 0;

    if(clntPut == clntGet)
    {
      printf("\nCan't handle any more request...\nTerminating...");
      exit(1);
    }

    pthread_cond_signal(&clntCondition);
    pthread_mutex_unlock(&srvrMutex);
    fflush(stdout);

  } // end of infinite for loop

} // end of main

void *request_handler(void *arg)
{
  int connfd;
  char *reqBuff;
  fflush(stdout);
  //printf("\nThread %d starting", (int) arg);

  for( ; ; )
  {
    pthread_mutex_lock(&clntMutex);
    //printf("\nMutex locked by thread# %d.", (int) arg);
    while(clntGet == clntPut)
     pthread_cond_wait(&clntCondition, &clntMutex);

    connfd = clntConnection[clntGet];
    if(++clntGet == MAX_CLIENT)
          clntGet = 0;

          
    // thread operation
    //printf("\nIn between mutex lock-unlock. Thread# %d, Connection# %d", (int) arg, connfd);
    reqBuff = (char *) malloc (1000);
    read(connfd, reqBuff, 1000);

    int temp;
    temp = find_method(reqBuff);

    if(temp == 0)
    {
      //printf("\nfind_method returned 0");
      request[(int)arg] = parse_request(reqBuff);
      //printf("\nHost: %s\nFile Path: %s", request[(int)arg].hostName, request[(int)arg].filePath);

      reply[(int)arg] = prepare_reply(200, request[(int)arg]);
      printf("\n\nprinting the reply structure");
      //printf("\nCode: %d\nDate: %s\nServer: %s\nlast_modified: %s\ncontent_length: %ld\ncontent type: %s\nfile path: %s", reply[(int)arg].code, reply[(int)arg].date, reply[(int)arg].server, reply[(int)arg].last_modified, reply[(int)arg].content_length, reply[(int)arg].content_type, reply[(int)arg].file_path);
      emit_reply(connfd, reply[(int)arg]);
    }
    else if(temp == 1)
    {
      printf("\nfind_method returned 1");
      reply[(int)arg] = prepare_reply(501, request[(int)arg]);
      emit_reply(connfd, reply[(int)arg]);
    }
    else
    {
      printf("\nfind_method returned -1");
      reply[(int)arg] = prepare_reply(400, request[(int)arg]);
      emit_reply(connfd, reply[(int)arg]);
    }

    pthread_mutex_unlock(&clntMutex);
    //printf("\nMutex unlocked by thread# %d.", (int) arg);
    thrdPool[(int) arg].threadCount++;

    // shahriar bhai told that closing the connection formally is not so important
    // good performance achieved @ closing it formally
    close(connfd);
  }
} // end of request_handler function

void emit_reply(int conn, Reply rply)
{
  //pthread_mutex_lock(&emitMutex2);
  if(rply.code == 200)
  {
    char *ok_code = "HTTP/1.1 200 OK\r\n";
    write(conn, ok_code, strlen(ok_code));

    char *date;
    date = (char *)malloc(100);
    strcpy(date, "Date: ");
    strcat(date, rply.date);
    strcat(date, "\r\n");
    write(conn, date, strlen(date));

    char *server;
    server = (char *)malloc(100);
    strcpy(server, "Server: ");
    strcat(server, rply.server);
    strcat(server, "\r\n");
    write(conn, server, strlen(server));

    char *last_modified;
    last_modified = (char *)malloc(100);
    strcpy(last_modified, "Last-Modified: ");
    strcat(last_modified, rply.last_modified);
    strcat(last_modified, "\r\n");
    write(conn, last_modified, strlen(last_modified));

    char *content_length;
    content_length = (char *)malloc(100);
    strcpy(content_length, "Content-Length: ");
    int decpnt, sign;
    char *p;
    p = (char *)malloc(20);
    p = ecvt(rply.content_length, 15, &decpnt, &sign);
    //printf("\nConversion-- %s %d %d", p, decpnt, sign);
    
    strncat(content_length, p, decpnt);
    strcat(content_length, "\r\n");
    write(conn, content_length, strlen(content_length));

    char *content_type;
    content_type = (char *)malloc(100);
    strcpy(content_type, "Content-Type: ");
    strcat(content_type, rply.content_type);
    strcat(content_type, "\r\n");
    write(conn, content_type, strlen(content_type));

    write(conn, "\r\n", 2);
    
    int b;
    char c;
    FILE *fp;
    fp = (FILE *) malloc(sizeof(FILE));
    fp = fopen(rply.file_path, "rb");
    while(!feof(fp))
    {
      c = getc(fp);
      if(!feof(fp))
      {
        //printf("%c", c);
        write(conn, &c, 1);
      }
    }
    
    //pthread_mutex_unlock(&emitMutex2);
    return;
  }
  else if(rply.code == 400)
  {
    char *ok_code = "HTTP/1.1 400 Bad Request\r\n";
    write(conn, ok_code, strlen(ok_code));

    char *date;
    date = (char *)malloc(100);
    strcpy(date, "Date: ");
    strcat(date, rply.date);
    strcat(date, "\r\n");
    write(conn, date, strlen(date));

    char *server;
    server = (char *)malloc(100);
    strcpy(server, "Server: ");
    strcat(server, rply.server);
    strcat(server, "\r\n");
    write(conn, server, strlen(server));

    write(conn, "\r\n", 2);

    char *error_400;
    error_400 = (char *)malloc(300);
    strcpy(error_400, "<html><head><title>Error No: 400. Bad Request.</title></head><body><h2>The server cannot parse the request.</h2><br><br><br><br><br><br><br><br><br><br><br><br><br><br><br><br><br><br><br><br><br><br><br><br><br><br><br><hr>For further info mail to shehab_sust@yahoo.com.</body><html>");
    write(conn, error_400, strlen(error_400));
  }
  else if(rply.code == 501)
  {
    char *ok_code = "HTTP/1.1 501 Method Not Implemented\r\n";
    write(conn, ok_code, strlen(ok_code));

    char *date;
    date = (char *)malloc(100);
    strcpy(date, "Date: ");
    strcat(date, rply.date);
    strcat(date, "\r\n");
    write(conn, date, strlen(date));

    char *server;
    server = (char *)malloc(100);
    strcpy(server, "Server: ");
    strcat(server, rply.server);
    strcat(server, "\r\n");
    write(conn, server, strlen(server));

    write(conn, "\r\n", 2);

    char *error_501;
    error_501 = (char *)malloc(300);
    strcpy(error_501, "<html><head><title>Error No: 501. Method Not Implemented.</title></head><body><h2>The server only resolves the GET method.</h2><br><br><br><br><br><br><br><br><br><br><br><br><br><br><br><br><br><br><br><br><br><br><br><br><br><br><br><hr>For further info mail to shehab_sust@yahoo.com.</body><html>");
    write(conn, error_501, strlen(error_501));
  }
  else if(rply.code == 404)
  {
    char *ok_code = "HTTP/1.1 404 File Not Found\r\n";
    write(conn, ok_code, strlen(ok_code));

    char *date;
    date = (char *)malloc(100);
    strcpy(date, "Date: ");
    strcat(date, rply.date);
    strcat(date, "\r\n");
    write(conn, date, strlen(date));

    char *server;
    server = (char *)malloc(100);
    strcpy(server, "Server: ");
    strcat(server, rply.server);
    strcat(server, "\r\n");
    write(conn, server, strlen(server));

    write(conn, "\r\n", 2);

    char *error_404;
    error_404 = (char *)malloc(300);
    strcpy(error_404, "<html><head><title>Error No: 404. File Not Found.</title></head><body><h2>The server could not found the requested url.</h2><br><br><br><br><br><br><br><br><br><br><br><br><br><br><br><br><br><br><br><br><br><br><br><br><br><br><br><hr>For further info mail to shehab_sust@yahoo.com.</body><html>");
    write(conn, error_404, strlen(error_404));
  }
  else if(rply.code == 403)
  {
    char *ok_code = "HTTP/1.1 403 Forbidden\r\n";
    write(conn, ok_code, strlen(ok_code));

    char *date;
    date = (char *)malloc(100);
    strcpy(date, "Date: ");
    strcat(date, rply.date);
    strcat(date, "\r\n");
    write(conn, date, strlen(date));

    char *server;
    server = (char *)malloc(100);
    strcpy(server, "Server: ");
    strcat(server, rply.server);
    strcat(server, "\r\n");
    write(conn, server, strlen(server));

    write(conn, "\r\n", 2);

    char *error_403;
    error_403 = (char *)malloc(300);
    strcpy(error_403, "<html><head><title>Error No: 403. Forbidden.</title></head><body><h2>You are not authorized to access this url.</h2><br><br><br><br><br><br><br><br><br><br><br><br><br><br><br><br><br><br><br><br><br><br><br><br><br><br><br><hr>For further info mail to shehab_sust@yahoo.com.</body><html>");
 
    write(conn, error_403, strlen(error_403));
  }
  
} //end of emit_reply

char *getMimeType(char  *str)
{
  printf("\nentered mime type");
  int i;
  //defined here the file extension
  char *fileExt[MAX];
  fileExt[0] = "html";
  fileExt[1] = "htm";
  fileExt[2] = "txt";
  fileExt[3] = "gif";
  fileExt[4] = "jpg";
  fileExt[5] = "qt";

  //defined here the mime type
  char *mType[MAX];
  mType[0] = "text/html";
  mType[1] = "text/html";
  mType[2] = "text/plain";
  mType[3] = "image/gif";
  mType[4] = "image/jpeg";
  mType[5] = "video/quicktime";

  for(i = 0; i < 6; i++)
  {
    if(strcmp(str, fileExt[i]) == 0)
    {
      printf("\nmatched");
      return mType[i];
    }
  } //end of for loop
  printf("\nreturning ERROR");
  char *er = "ERROR";
  return er;
} //end of getMimetype

Reply prepare_reply(int code, Request rq)
{
  // extracting the file extension
  char *drive, *direc, *fname, *ext;

  drive = (char *) malloc (3);
  direc = (char *) malloc (66);
  fname = (char *) malloc (9);
  ext = (char *) malloc (5);

  Reply rpl;

  if(code == 200)
  {
    rpl.code = 200;
    
    time_t lt;
    lt = time(NULL);
    strcpy(rpl.date, ctime(&lt));
    rpl.date[strlen(rpl.date) - 1] = '\0';
    
    strcpy(rpl.server, "TinyThreadedServer/0.1a");
    
    char dir[100];
    getcwd(dir, 100);
    //printf("\ncurrent dir is %s", dir);
    //printf("\nfile path is %s", rq.filePath);
    strcat(dir, "/");
    strcat(dir, rq.filePath);
    //printf("\nabsolute file path is %s", dir);
    char *path, *p;
    path = (char *) malloc (80);
    p = (char *) malloc (80);
    path = strtok(dir, "/");
    strcpy(p, "/");
    strcat(p, path);
    do{
      path = strtok('\0', "/");
      if(path)
      {
        strcpy(ext, path);
        strcat(p, "/");
        strcat(p, path);
      }
    } while(path);
    //printf("\nthe c++ file path %s", p);

    char *e;
    e = (char *)malloc(10);
    e = strtok(ext, ".");
    e = strtok('\0', ".");
    //printf("\nfile extension is %s", e);
        
    FILE *fp;
    fp = (FILE *) malloc(sizeof(FILE));
    if((fp = fopen(p, "r")) == NULL)
    {
      printf("\nFILE NOT FOUND.\nERROR CODE 404");
      rpl.code = 404;
      return rpl;
    }

    struct stat buff;
    stat(p, &buff);
    //printf("\nSize: %ld\ntime of last modification: %s", buff.st_size, ctime(&buff.st_mtime));

    strcpy(rpl.last_modified, ctime(&buff.st_mtime));
    rpl.last_modified[strlen(rpl.last_modified) - 1] = '\0';
    
    rpl.content_length = buff.st_size;

    if(strcmp(getMimeType(e), "ERROR") == 0)
    {
      rpl.code = 403;
      fclose(fp);
      return rpl;
    }
    strcpy(rpl.content_type, getMimeType(e));

    strcpy(rpl.file_path, p);

    fclose(fp);
  }
  else if (code == 400)
  {
    rpl.code = 400;

    time_t lt;
    lt = time(NULL);
    strcpy(rpl.date, ctime(&lt));
    rpl.date[strlen(rpl.date) - 1] = '\0';

    strcpy(rpl.server, "TinyThreadedServer/0.1a");
  }
  else if (code == 501)
  {
    rpl.code = 501;

    time_t lt;
    lt = time(NULL);
    strcpy(rpl.date, ctime(&lt));
    rpl.date[strlen(rpl.date) - 1] = '\0';

    strcpy(rpl.server, "TinyThreadedServer/0.1a");
  }

  return rpl;
} //end of prepare_reply

Request parse_request(char *rbuff)
{
  char *fileName;
  char *hostName;
  int j, k, l, m, n;
  Request r;
  
  fileName = (char *) malloc (100);
  hostName = (char *) malloc (100);

  for(j = 5, k = 0; ;j++, k++)
  {
    if(rbuff[j] == ' ') break;
    fileName[k] = rbuff[j];
  } //end of for loop
  fileName[k]= '\0';
  //printf("\nThe valid file path is %s", fileName);
  strcpy(r.filePath, fileName);

  //finding the host
  char *p;
  p = strstr(rbuff, "Host: ");

  for(l = 0; l < 6; l++)
  {
    p++;
  } //end of for loop

  for(m = 0, n = 0; ; m++, n++)
  {
    if(*p == '\r' || *p == '\n') break;
    hostName[n] = *p;
    p++;
  } //end of for loop
  hostName[n]= '\0';

  fflush(stdout);
  strcpy(r.hostName, hostName);

  return r;
} // end of parse_request

int find_method (char *req)
{
  pthread_mutex_lock(&srvrMutex2);
  //printf("\nin the find_method function\n%s\nthe size of the request is: %d", req, strlen(req));

  char *method_list[6];
  method_list[0] = (char *) malloc (10);
  strcpy(method_list[0], "PUT");
  method_list[1] = (char *) malloc (10);
  strcpy(method_list[1], "HEAD");
  method_list[2] = (char *) malloc (10);
  strcpy(method_list[2], "POST");
  method_list[3] = (char *) malloc (10);
  strcpy(method_list[3], "DELETE");
  method_list[4] = (char *) malloc (10);
  strcpy(method_list[4], "TRACE");
  method_list[5] = (char *) malloc (10);
  strcpy(method_list[5], "CONNECT");
  
  int i, j = 0;
  char *r;
  r = (char *) malloc (10);
  for(i = 0; i < strlen(req); i++)
  {
    if(req[i] == ' ') break;
    r[i] = req[i];
  } //end of for loop
  r[i] = '\0';
  //printf("\nExtracted method is %s", r);

  if(strcmp(r, "GET") == 0)
  {
    //printf("\nit's a GET method !!!");
    pthread_mutex_unlock(&srvrMutex2);
    return 0;
  }
  
  for(i = 0; i < 7; i++)
  {
    if(strcmp(r, method_list[i]) == 0)
    {
      printf("\n%s matched with %s", r, method_list[i]);
      j = 1;
      break;
    }
  } //end of for loop

  if(j == 1)
  {
    pthread_mutex_unlock(&srvrMutex2);
    return 1;
  }
  else
  {
    pthread_mutex_unlock(&srvrMutex2);
    return -1;
  }
} // end of find_method
