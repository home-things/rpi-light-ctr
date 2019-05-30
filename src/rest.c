// #include <stdio.h>
// #include <string.h>
// #include <stdlib.h>
// #include <unistd.h>
// #include <fcntl.h>

// #include <netinet/tcp.h>
// #include <sys/socket.h>
// #include <sys/types.h>
// #include <netinet/in.h>
// #include <netdb.h>

// int socket_connect(char *host, in_port_t port)
// {
//   struct hostent *hp;
//   struct sockaddr_in addr;
//   int on = 1, sock;

//   if ((hp = gethostbyname(host)) == NULL)
//   {
//     herror("gethostbyname");
//     exit(1);
//   }
//   bcopy(hp->h_addr_list[0], &addr.sin_addr, hp->h_length);
//   addr.sin_port = htons(port);
//   addr.sin_family = AF_INET;
//   sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
//   setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (const char *)&on, sizeof(int));

//   if (sock == -1)
//   {
//     perror("setsockopt");
//     exit(1);
//   }

//   if (connect(sock, (struct sockaddr *)&addr, sizeof(struct sockaddr_in)) == -1)
//   {
//     perror("connect");
//     exit(1);
//   }
//   return sock;
// }

// #define BUFFER_SIZE 1024

// void http_get(const char *host, const char *path, const char *headers)
// {
//   int fd;
//   char buffer[BUFFER_SIZE];
//   char req[256];
//   strcat(req, "GET ");
//   strcat(req, path);
//   if (headers != NULL)
//   {
//     strcat(req, "\n");
//     strcat(req, headers);
//   }
//   strcat(req, "\r\n\r\n");

//   fd = socket_connect(host, 80);
//   write(fd, req, strlen(req));
//   bzero(buffer, BUFFER_SIZE);

//   while (read(fd, buffer, BUFFER_SIZE - 1) != 0)
//   {
//     fprintf(stderr, "%s", buffer);
//     bzero(buffer, BUFFER_SIZE);
//   }

//   shutdown(fd, SHUT_RDWR);
//   close(fd);
// }
