#include <stdio.h>
#include <sys/types.h>          /* See NOTES */
#include <sys/socket.h>
#include <string.h>
#include <netdb.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>

char *response_content_get(const char *response)
{
  char *status_code = NULL;
  char *content = NULL;

  if(response == NULL)
  {
    fprintf(stderr, "response is null\n");
    goto exit;
  }

  status_code = strchr(response, ' ');
  if(status_code == NULL)
  {
    fprintf(stderr, "response content error!\n");
    goto exit;
  }

  status_code++;

  if(strncmp(status_code, "200", 3) != 0)
  {
    fprintf(stderr, "request error, http status code = %c%c%c!\n",
        *status_code, *(status_code + 1), *(status_code + 2));
    goto exit;
  }

  content = strstr(response, "\r\n\r\n");
  if(content == NULL)
  {
    fprintf(stderr, "response content is null!\n");
    goto exit;
  }

  content += 4;

  printf("content = '%s'\n", content);

exit:
  return content;
}


int connect_to_server(const char *dstaddr, const char *dstport)
{
  struct addrinfo hints;
  struct addrinfo *res = NULL, *cur = NULL;
  struct sockaddr_in *addr = NULL;
  int ret = -1;
  int fd_server = -1;
  struct timeval timeout = {10, 0};

  fd_server = socket(AF_INET, SOCK_STREAM, 0);
  if(fd_server == -1)
  {
    perror("socket create!\n");
    goto exit;
  }

  if(setsockopt(fd_server, SOL_SOCKET, SO_SNDTIMEO, &timeout,
        sizeof(struct timeval)) != 0)
  {
    perror("set socket opt error!\n");
    goto exit;
  }

  if(setsockopt(fd_server, SOL_SOCKET, SO_RCVTIMEO, &timeout,
        sizeof(struct timeval)) != 0)
  {
    perror("set socket opt error!\n");
    goto exit;
  }

  memset(&hints, 0, sizeof(struct addrinfo));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;

  ret = getaddrinfo(dstaddr, dstport, &hints, &res);
  if(ret != 0) 
  {
    perror("getaddrinfo");
    goto exit;
  }

  for(cur = res; cur != NULL; cur = cur->ai_next) 
  {
    addr = (struct sockaddr_in *)cur->ai_addr;
    printf("server ip:%s\n", inet_ntoa(addr->sin_addr));

    ret = connect(fd_server, (struct sockaddr *)addr, sizeof(struct sockaddr_in));
    if(ret != 0)
    {
      perror("connect");
    }
    else 
    {
      break;
    }
  }

  if(ret != 0)
  {
    fprintf(stderr, "connect to server failed!\n");
    goto exit;
  }

  ret = 0;

exit:
  if(res)
    freeaddrinfo(res);

  if (ret != 0)
  {
    close(fd_server);
    return -1;
  }
  else
    return fd_server;
}

int main(int argc, char **argv)
{
  char request[1024] = "";
  char recv_buf[1024] = "";
  int fd_server = -1;
  int result = 0;
  int len = 0;
  char *http_content = NULL;

  if(argc < 2)
    goto exit;

  printf("func:%s, line:%d\n", __func__, __LINE__);

  len = snprintf(request, 1024, 
      "POST /ip.php HTTP/1.1\r\n"
      "Host: xfqxk.lzlsy.com\r\n"
      "User-Agent: Mozilla/5.0 (Windows NT 6.1; WOW64; rv:43.0) Gecko/20100101 Firefox/43.0\r\n"
      "Accept: */*\r\n"
      "Accept-Language: zh-CN,zh;q=0.8,en-US;q=0.5,en;q=0.3\r\n"
      "Accept-Encoding: gzip, deflate\r\n"
      "DNT: 1\r\n"
      "Content-Type: application/x-www-form-urlencoded; charset=UTF-8\r\n"
      "X-Requested-With: XMLHttpRequest\r\n"
      "Referer: http://xfqxk.lzlsy.com/zt/124_2016392212.html?from=timeline&isappinstalled=0\r\n"
      "Content-Length: 34\r\n"
      "Connection: keep-alive\r\n"
      "If-Modified-Since: *\r\n"
      "\r\n"
      "ip=122.%s.%s.148&cid=124&tid=3765\r\n",
      argv[1], argv[2]);

  fd_server = connect_to_server("xfqxk.lzlsy.com", "80");
  if(fd_server == -1)
  {
    printf("Content to server failed!\n");
    goto exit;
  }

  result = send(fd_server, request, len, 0);
  if(result != len)
  {
    printf("Send failed!\n");
    goto exit;
  }

  result = recv(fd_server, recv_buf, 1024, 0);
  if(result == -1)
  {
    printf("recv failed!\n");
    goto exit;
  }

  recv_buf[result] = '\0';

  http_content = response_content_get(recv_buf);
  if (http_content == NULL)
  {
    printf("Get http content failed!\n");
    goto exit;
  }

  close(fd_server);

exit:
  return 0;
}
