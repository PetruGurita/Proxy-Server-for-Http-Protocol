#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <utility>
#include <netdb.h>
#include <unordered_map>
#define MAX_CLIENTS	101
#define MAXLEN 500
using namespace std;

void error(char *msg)
{
  perror(msg);
  exit(1);
}


pair<char*, char*> get_absolute_relative_path(char* buffer) {
  int startingPosition = 0;
  char *find = strstr(buffer, "https://");
  if (find != NULL && find - buffer == 0)
  startingPosition = 8;

  find = strstr(buffer, "http://");
  if (find != NULL && find - buffer == 0)
  startingPosition = 7;

  int size = strlen(buffer);
  int i = startingPosition;
  while (i < size) {
    if(buffer[i] == '/') break;
    ++i;
  }

  char *absolute = (char *) calloc(strlen(buffer), sizeof(char));
  strncpy(absolute, buffer + startingPosition, i - startingPosition);
  char *relative = (char *) calloc(strlen(buffer), sizeof(char));
  strcpy(relative, buffer + i);
  if (strlen(relative) == 0)
  relative[0] = '/';
  return make_pair(absolute, relative);
}

int get_port_number(char *buffer) {
  char *url = get_absolute_relative_path(buffer).first;
  int size = strlen(url);
  int i = 0;
  int port = 0;
  while (i < size) {
    if (buffer[i] == ':') {
      ++i;
      while (i < size && buffer[i] <= '9' && buffer[i] >= '0') {
        port = port * 10 + (buffer[i] - '0');
        ++i;
      }
    }
    ++i;
  }
  return port == 0 ? (80) : (port);
}

struct sockaddr_in create_sock_structure(int port) {

  struct sockaddr_in sock_str;
  memset((char *) &sock_str, 0, sizeof(sock_str));
  sock_str.sin_family = AF_INET;
  sock_str.sin_addr.s_addr = INADDR_ANY;
  sock_str.sin_port = htons(port);
  return sock_str;
}

pair<pair<char*, char*>, char*> get_command(char buffer[]) {

  char* command = (char *) calloc(100, sizeof(char));
  char* link = (char *) calloc(100, sizeof(char));
  char* http_version = (char *) calloc(100, sizeof(char));
  int i = 0;
  int idx_command = 0;
  int idx_link = 0;
  int idx_http = 0;

  while (buffer[i] != 0 && buffer[i] != ' ' && buffer[i] != '\n') {
    command[idx_command++] = buffer[i++];
  }
  ++i; //skip blank
  while (buffer[i] != 0 && buffer[i] != ' ' && buffer[i] != '\n') {
    link[idx_link++] = buffer[i++];
  }
  ++i; //skip blank
  while (buffer[i] != 0 && buffer[i] != ' ' && buffer[i] != '\n') {
    http_version[idx_http++] = buffer[i++];
  }
  if (strcmp(command, "GET") != 0 && strcmp(command, "POST") != 0)
  return make_pair(make_pair(nullptr, nullptr), nullptr);
  http_version[8] = 0;

  if (strcmp(http_version, "HTTP/1.1") != 0 &&
  strcmp(http_version, "HTTP/1.0") != 0)
  return make_pair(make_pair(nullptr, nullptr), nullptr);

  struct hostent *host = gethostbyname(get_absolute_relative_path(link).first);
  if (host == NULL)
  return make_pair(make_pair(nullptr, nullptr), nullptr);

  return make_pair(make_pair(command, link), http_version);

}

int main(int argc, char *argv[])
{
  int sockfd, newsockfd, portno;
  socklen_t clilen;
  char buffer[MAXLEN];
  struct sockaddr_in serv_addr, cli_addr;
  int n, i, j;
  unordered_map<string, pair<int, int>> cache;
  FILE *fp;
  fd_set read_fds;
  fd_set tmp_fds;
  int fdmax;
  if (argc < 2) {
    fprintf(stderr,"Usage : %s port\n", argv[0]);
    exit(1);
  }

  FD_ZERO(&read_fds);
  FD_ZERO(&tmp_fds);

  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0)
  error((char*)"ERROR opening socket");

  portno = atoi(argv[1]);

  memset((char *) &serv_addr, 0, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = INADDR_ANY;
  serv_addr.sin_port = htons(portno);

  if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(struct sockaddr)) < 0)
  error((char*)"ERROR on binding");

  listen(sockfd, MAX_CLIENTS);
  FD_SET(sockfd, &read_fds);
  fdmax = sockfd;

  while (1) {
    tmp_fds = read_fds;
    if (select(fdmax + 1, &tmp_fds, NULL, NULL, NULL) == -1)
    error((char*)"ERROR in select");

    for(i = 0; i <= fdmax; i++) {
      if (FD_ISSET(i, &tmp_fds)) {

        if (i == sockfd) {
          clilen = sizeof(cli_addr);
          if ((newsockfd = accept(sockfd,
            (struct sockaddr *)&cli_addr, &clilen)) == -1) {
              error((char*)"ERROR in accept");
            }
            else {
              FD_SET(newsockfd, &read_fds);
              if (newsockfd > fdmax) {
                fdmax = newsockfd;
              }
            }
          }
          else {
            memset(buffer, 0, MAXLEN);
            if ((n = recv(i, buffer, sizeof(buffer), 0)) <= 0) {
              if (n == 0) {
                printf("selectserver: socket %d hung up\n", i);
              } else {
                error((char*)"ERROR in recv");
              }
              close(i);
              FD_CLR(i, &read_fds);
            }

            else {
              pair<pair<char*, char*>, char*> comanda = get_command(buffer);
              if (comanda.first.first == nullptr  ||
                comanda.first.second == nullptr ||
                comanda.second == nullptr) {
                  send(i, "400 : BAD REQUEST\n", 19, 0);
                  close(i);
                  FD_CLR(i, &read_fds);
                }
                else {
                  pair<char*, char*> paths =
                  get_absolute_relative_path(comanda.first.second);
                  char *url = paths.first;
                  char *relative = paths.second;
                  bool in_cache = false;
                  string string_buffer(buffer);
                  for (auto it = cache.cbegin(); it != cache.cend(); ++it) {
                    if (it->first.compare(string_buffer) == 0) {
                      in_cache = true;
                      break;
                    }
                  }
                  if (in_cache == false) {
                    fp = fopen("cache.txt", "a");
                    int data_written = 0;
                    int get_port = get_port_number(comanda.first.second);
                    struct sockaddr_in site_addr =
                    create_sock_structure(get_port);
                    struct hostent *host = gethostbyname(url);
                    struct in_addr **addr_list;
                    addr_list = (struct in_addr **) host->h_addr_list;
                    char* ip = (char *) calloc(16, sizeof(char));
                    if (addr_list[0] != NULL)
                    ip = inet_ntoa(*(addr_list[0]));
                    else {
                      printf("Can not access IP address\n");
                      return -1;
                    }
                    site_addr.sin_addr.s_addr = inet_addr(ip);
                    int host_sock = socket(AF_INET, SOCK_STREAM, 0);
                    if (host_sock < 0) {
                      error((char *)"ERROR opening socket");
                    }
                    if (connect(host_sock, (struct sockaddr * ) & site_addr,
                    sizeof(site_addr)) < 0) {

                      printf("Error at connecting\n");
                      exit(-1);
                    }

                    if (strcmp( comanda.first.first, "GET") == 0) {
                      memset(buffer, 0, sizeof(buffer));
                      sprintf(buffer,
                        "%s %s %s\r\nHost: %s\r\nConnection: close\r\n\r\n",
                        comanda.first.first, relative, comanda.second, url);
                      }
                      int data = send(host_sock, buffer, strlen(buffer), 0);
                      if (data < 0) {
                        printf("Could not connect to host\n");
                        return -1;
                      }
                      fseek(fp, 0, SEEK_END);
                      int size = ftell(fp);
                      rewind(fp);
                      char http_request_code[13];
                      memset(http_request_code, 0, 13);
                      data = recv(host_sock, http_request_code, 12, 0);
                      //recv does not have to be verified because the response has minimum the header's dimension
                      if (strstr(http_request_code, "200") == NULL) {
                        send(i, "400 : BAD REQUEST\n", 19, 0);
                        close(host_sock);
                        fclose(fp);
                        close(i);
                        FD_CLR(i, &read_fds);
                        break;
                      }
                      if (send(i, http_request_code,
                        data, 0) <= 0) {
                          return -1;
                        }
                        data_written += data;
                        fprintf(fp, "%s", http_request_code);
                        while ((data = recv(host_sock, buffer, MAXLEN - 1, 0))){
                          if (send(i, buffer, data, 0) <= 0) {
                            return -1;
                          }
                          fprintf(fp, "%s", buffer);
                          data_written += data;
                          memset(buffer, 0, MAXLEN);
                        }

                        cache[string_buffer] = make_pair(data_written, size);
                        close(host_sock);
                      }
                      else {
                        fp = fopen("cache.txt", "r");
                        fseek(fp, cache[string_buffer].second, SEEK_SET);
                        char *c = (char *)
                        calloc(cache[string_buffer].first + 1, sizeof(char));
                        fread(c, sizeof(char), cache[string_buffer].first, fp);
                        if (send(i, c, cache[string_buffer].first, 0) <= 0) {
                          return -1;
                        }
                      }
                      fclose(fp);
                      close(i);
                      FD_CLR(i, &read_fds);
                    }
                  }
                }
              }
            }
          }
          fclose(fp);
          close(sockfd);
          return 0;
        }
