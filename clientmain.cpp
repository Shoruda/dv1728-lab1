#include <stdio.h>
#include <string.h>
#include <stdlib.h>
/* You will to add includes here */
// Enable if you want debugging to be printed, see examble below.
// Alternative, pass CFLAGS=-DDEBUG to make, make CFLAGS=-DDEBUG
#define DEBUG
/*TEST*/

//A
// Included to get the support library
#include <calcLib.h>

#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <ctype.h>
#include "protocol.h"

int calculate(const char* buf)
{
  char operation[8];
  int value1, value2;
  if (sscanf(buf, "%7s %d %d", operation, &value1, &value2) != 3)
  {
    fprintf(stderr, "ERROR: INVALID FORMAT\n");
    return 0;
  }
  printf("ASSIGNMENT: %s", buf);
  if (strcmp(operation, "add") == 0) {
    return value1 + value2;
  } 
  else if (strcmp(operation, "sub") == 0) {
    return value1 - value2;
  } 
  else if (strcmp(operation, "mul") == 0) {
    return value1 * value2;
  } 
  else if (strcmp(operation, "div") == 0) {
    return value1 / value2;
  } 
  else {
    fprintf(stderr, "ERROR: UNKNOWN OPERATION '%s'\n", operation);
    return 0;
  }
}

int calculate_protocol(const char* buf, size_t len, calcProtocol* msg)
{
  if(len < sizeof(calcProtocol)) return -1;

  const calcProtocol* in = (const calcProtocol*)buf;

  msg->type          = ntohs(in->type);
  msg->major_version = ntohs(in->major_version);
  msg->minor_version = ntohs(in->minor_version);
  msg->id            = ntohl(in->id);
  msg->arith         = ntohl(in->arith);
  msg->inValue1      = ntohl(in->inValue1);
  msg->inValue2      = ntohl(in->inValue2);
  msg->inResult      = ntohl(in->inResult);

  if(msg->major_version != 1 || msg->minor_version != 1) return -1;

  int result;
  if(msg->arith == 1)
  {
    result = (msg->inValue1 + msg->inValue2);
    printf("ASSIGNMENT: add %d %d\n", msg->inValue1, msg->inValue2);
  }
  else if(msg->arith == 2)
  {
    result = (msg->inValue1 - msg->inValue2);
    printf("ASSIGNMENT: sub %d %d\n", msg->inValue1, msg->inValue2);
  }
  else if(msg->arith == 3)
  {
    result = (msg->inValue1 * msg->inValue2);
    printf("ASSIGNMENT: mul %d %d\n", msg->inValue1, msg->inValue2);
  }
  else if(msg->arith == 4)
  {
    result = (msg->inValue1 / msg->inValue2);
    printf("ASSIGNMENT: div %d %d\n", msg->inValue1, msg->inValue2);
  }
  msg->inResult = result;
  return result;
}


int setup_connection(const char* host, int port, int socktype)
{
  struct addrinfo hints, *servinfo, *p;
  char portstr[6];
  int rv, sockfd;

  snprintf(portstr, sizeof(portstr), "%d", port);
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = socktype;

  if ((rv = getaddrinfo(host, portstr, &hints, &servinfo)) != 0) {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
    return -1;
  }

  for (p = servinfo; p != NULL; p = p->ai_next) {
    sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
    if (sockfd == -1) 
    {
      perror("socket");
      continue;
    }

    struct timeval time;
    time.tv_sec = 2;
    time.tv_usec = 0;
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &time, sizeof(time));

    if (socktype == SOCK_STREAM)
    {
      //TCP
      if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1)
      {
        perror("tcp connect");
        close(sockfd);
        continue;
      }

    } 
    else if(socktype == SOCK_DGRAM)
    {
      //UDP
      if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
        perror("udp connect");
        close(sockfd);
        continue;
      }
    }

  break;
  }

  freeaddrinfo(servinfo);

  if(p==NULL)
  {
    fprintf(stderr, "setup_connection: failed to connect to %s:%d\n", host, port);
    return -1;
  }

  return sockfd;
}

int tcp_text_handler(const char* host, int port)
{ 
  int sockfd = setup_connection(host, port, SOCK_STREAM);
  if (sockfd < 0) return -1;
  
  char recv_buffer[1024];
  ssize_t bytes_received; 

  char send_buffer[32];
  ssize_t bytes_sent;


  if ((bytes_received = recv(sockfd, recv_buffer, sizeof(recv_buffer) -1, 0)) == -1)
  {
    perror("TCP + TEXT: recv");
    exit(1);
  }

  if (strncmp(recv_buffer, "TEXT TCP 1.1", 12) != 0) {
    printf("ERROR\n");
    close(sockfd);
    return -1;
  }

  snprintf(send_buffer, sizeof(send_buffer), "TEXT TCP 1.1 OK\n");
  if (send(sockfd, send_buffer, strlen(send_buffer), 0) == -1) {
    perror("TCP + TEXT: send");
    close(sockfd);
    return -1;
  }

  if ((bytes_received = recv(sockfd, recv_buffer, sizeof(recv_buffer) -1, 0)) == -1)
  {
    perror("TCP + TEXT: recv");
    exit(1);
  }

  recv_buffer[bytes_received] = '\0';

  int result = calculate(recv_buffer);
  snprintf(send_buffer, sizeof(send_buffer), "%d\n", result);
  if (send(sockfd, send_buffer, strlen(send_buffer), 0) == -1) {
    perror("TCP + TEXT: send result");
    close(sockfd);
    return -1;
  }
  memset(recv_buffer, 0, sizeof(recv_buffer));
  if ((bytes_received = recv(sockfd, recv_buffer, sizeof(recv_buffer) - 1, 0)) <= 0) {
    perror("TCP + TEXT: recv response");
    close(sockfd);
    return -1;
  }

  recv_buffer[bytes_received] = '\0';
  if (strncmp(recv_buffer, "OK", 2) == 0) {
    printf("OK (myresult=%d)\n", result);
  }
  close(sockfd);
  return 0;
}

int tcp_binary_handler(const char* host, int port)
{
  int sockfd = setup_connection(host, port, SOCK_STREAM);
  if (sockfd < 0) return -1;

  calcProtocol msg;
  ssize_t bytes_received;
  char recv_buffer[1024];

  char send_buffer[32];
  ssize_t bytes_sent;
  memset(&send_buffer, 0, sizeof(send_buffer));


  if ((bytes_received = recv(sockfd, recv_buffer, sizeof(recv_buffer) -1, 0)) == -1)
  {
    perror("TCP + TEXT: recv");
    exit(1);
  }

  snprintf(send_buffer, sizeof(send_buffer), "BINARY TCP 1.1 OK\n");
  bytes_sent = send(sockfd, send_buffer, strlen(send_buffer), 0);
  if (bytes_sent == -1) {
    perror("TCP + TEXT: send");
    close(sockfd);
    return -1;
  }

  

  bytes_received = recv(sockfd, &msg, sizeof(msg), 0);
  if (bytes_received != sizeof(msg)) {
    printf("ERROR\n");
    close(sockfd);
    return -1;
  }

  int result = calculate_protocol((char*)&msg, bytes_received, &msg);
  msg.inResult = result;

  msg.type = 2;

  calcProtocol send_msg = msg;
  send_msg.type          = htons(msg.type);
  send_msg.major_version = htons(msg.major_version);
  send_msg.minor_version = htons(msg.minor_version);
  send_msg.id            = htonl(msg.id);
  send_msg.arith         = htonl(msg.arith);
  send_msg.inValue1      = htonl(msg.inValue1);
  send_msg.inValue2      = htonl(msg.inValue2);
  send_msg.inResult      = htonl(msg.inResult);

  // Send binary response
  bytes_sent = send(sockfd, &send_msg, sizeof(send_msg), 0);
  if (bytes_sent != sizeof(send_msg)) {
    perror("TCP + BINARY: send");
    close(sockfd);
    return -1;
  }
  calcMessage recv_msg;

  bytes_received = recv(sockfd, &recv_msg, sizeof(recv_msg), 0);
  if ( bytes_received <= 0) {
    perror("TCP + TEXT: recv response");
    close(sockfd);
    return -1;
  }

  if(ntohl(recv_msg.message) == 1)
    printf("OK (myresult=%d)\n", msg.inResult);
  close(sockfd);
  return 0;
}


int udp_text_handler(const char* host, int port)
{
  int sockfd = setup_connection(host, port, SOCK_DGRAM);
  if (sockfd < 0) return -1;  

  char recv_buffer[1024];
  ssize_t bytes_received; 

  char send_buffer[32];
  ssize_t bytes_sent;

  const char *msg = "TEXT UDP 1.1\n";
  bytes_sent = send(sockfd, msg, strlen(msg), 0);
  if (bytes_sent == -1) {
    perror("UDP + TEXT: send");
    close(sockfd);
    return -1;
  }

  bytes_received = recv(sockfd, recv_buffer, sizeof(recv_buffer) - 1, 0);
  if (bytes_received == -1) {
    perror("UDP + TEXT: recv");
    close(sockfd);
    return -1;
  }

  recv_buffer[bytes_received] = '\0';
  int result = calculate(recv_buffer);
  snprintf(send_buffer, sizeof(send_buffer), "%d\n", result);
  if (send(sockfd, send_buffer, strlen(send_buffer), 0) == -1) {
    perror("TCP + TEXT: send result");
    close(sockfd);
    return -1;
  }

  if ((bytes_received = recv(sockfd, recv_buffer, sizeof(recv_buffer) - 1, 0)) <= 0) {
    perror("TCP + TEXT: recv response");
    close(sockfd);
    return -1;
  }

  recv_buffer[bytes_received] = '\0';
  if (strncmp(recv_buffer, "OK", 2) == 0) {
    printf("OK (myresult=%d)\n", result);
  }
  close(sockfd);
  return 0;
}
int udp_binary_handler(const char* host, int port)
{
    int sockfd = setup_connection(host, port, SOCK_DGRAM);
    if (sockfd < 0) return -1;
    calcMessage init_msg;
    init_msg.type          = htons(22);
    init_msg.message       = htonl(0);
    init_msg.protocol      = htons(17);
    init_msg.major_version = htons(1);
    init_msg.minor_version = htons(1);
    calcProtocol msg;
    ssize_t bytes_received, bytes_sent;
    char send_buffer[32];

    bytes_sent = send(sockfd, &init_msg, sizeof(init_msg), 0);
    if (bytes_sent <= 0) {
        perror("UDP + BINARY: SEND");
        close(sockfd);
        return -1;
    }

    bytes_received = recv(sockfd, &msg, sizeof(msg), 0);
    if (bytes_received <= 0) {
        perror("UDP + BINARY: RECV CALC");
        close(sockfd);
        return -1;
    }

    int result = calculate_protocol((char*)&msg, bytes_received, &msg);

    msg.type = 2;
    calcProtocol send_msg = msg;
    send_msg.type          = htons(msg.type);
    send_msg.major_version = htons(msg.major_version);
    send_msg.minor_version = htons(msg.minor_version);
    send_msg.id            = htonl(msg.id);
    send_msg.arith         = htonl(msg.arith);
    send_msg.inValue1      = htonl(msg.inValue1);
    send_msg.inValue2      = htonl(msg.inValue2);
    send_msg.inResult      = htonl(msg.inResult);

    bytes_sent = send(sockfd, &send_msg, sizeof(send_msg), 0);
    if (bytes_sent != sizeof(send_msg)) {
        perror("UDP + BINARY: send result");
        close(sockfd);
        return -1;
    }

    calcMessage recv_msg;
    bytes_received = recv(sockfd, &recv_msg, sizeof(recv_msg), 0);
    if (bytes_received <= 0) {
        perror("UDP + BINARY: recv final response");
        close(sockfd);
        return -1;
    }
    if(ntohl(recv_msg.message) == 1)
      printf("OK (myresult=%d)\n", msg.inResult);
    close(sockfd);
    return 0;
}




int main(int argc, char *argv[]){
  
  
  
  if (argc < 2) {
    fprintf(stderr, "Usage: %s protocol://server:port/path.\n", argv[0]);
    exit(EXIT_FAILURE);
  }
  

    
  /*
    Read first input, assumes <ip>:<port> syntax, convert into one string (Desthost) and one integer (port). 
     Atm, works only on dotted notation, i.e. IPv4 and DNS. IPv6 does not work if its using ':'. 
  */
    char protocolstring[6], hoststring[2000],portstring[6], pathstring[7];

    char *input = argv[1];
    
    /* Some error checks on string before processing */
    // Check for more than two consequtive slashes '///'.

    if (strstr(input, "///") != NULL ){
      printf("Invalid format: %s.\n", input);
      return 1;
    }
    

    // Find the position of "://"
    char *proto_end = strstr(input, "://");
    if (!proto_end) {
        printf("Invalid format: missing '://'\n");
        return 1;
    }

     // Extract protocol
    size_t proto_len = proto_end - input;
    if (proto_len >= sizeof(protocolstring)) {
        fprintf(stderr, "Error: Protocol string too long\n");
        return 1;
    }
    
    // Copy protocol
    strncpy(protocolstring, input, proto_end - input);
    protocolstring[proto_end - input] = '\0';

    // Move past "://"
    char *host_start = proto_end + 3;

    // Find the position of ":"
    char *port_start = strchr(host_start, ':');
    if (!port_start || port_start == host_start) {
	printf("Error: Port is missing or ':' is misplaced\n");
        return 1;
    }

    // Extract host
    size_t host_len = port_start - host_start;
    if (host_len >= sizeof(hoststring)) {
        printf("Error: Host string too long\n");
        return 1;
    }
    
    // Copy host
    strncpy(hoststring, host_start, port_start - host_start);
    hoststring[port_start - host_start] = '\0';

        // Find '/' which starts the path
    char *path_start = strchr(host_start, '/');
    if (!path_start || *(path_start + 1) == '\0') {
        fprintf(stderr, "Error: Path is missing or invalid\n");
        return 1;
    }

    // Extract path
    if (strlen(path_start + 1) >= sizeof(pathstring)) {
        fprintf(stderr, "Error: Path string too long\n");
        return 1;
    }
    strcpy(pathstring, path_start + 1);

    // Extract port


    size_t port_len = path_start - port_start - 1;
    if (port_len >= sizeof(portstring)) {
        fprintf(stderr, "Error: Port string too long\n");
        return 1;
    }
    strncpy(portstring, port_start + 1, port_len);
    portstring[port_len] = '\0';

    // Validate port is numeric
    for (size_t i = 0; i < strlen(portstring); ++i) {
        if (portstring[i] < '0' || portstring[i] > '9') {
            fprintf(stderr, "Error: Port must be numeric\n");
            return 1;
        }
    }


    
    char *protocol, *Desthost, *Destport, *Destpath;
    protocol=protocolstring;
    Desthost=hoststring;
    Destport=portstring;
    Destpath=pathstring;
      
  // *Desthost now points to a sting holding whatever came before the delimiter, ':'.
  // *Dstport points to whatever string came after the delimiter. 


    
  /* Do magic */
  int port=atoi(Destport);
  if (port <= 1 or port >65535) {
    printf("Error: Port is out of server scope.\n");
    if ( port > 65535 ) {
      printf("Error: Port is not a valid UDP or TCP port.\n");
    }
    return 1;
  }

  #ifdef DEBUG 
  printf("Protocol: %s Host %s, port = %d and path = %s.\n",protocol, Desthost,port, Destpath);
  #endif

  for (int i = 0; protocolstring[i]; i++) {
    protocolstring[i] = tolower(protocolstring[i]);
  }

  for (int i = 0; pathstring[i]; i++) {
    pathstring[i] = tolower(pathstring[i]);
  }
  
  if(strcmp(protocol, "tcp") == 0) 
  {
    if(strcmp(Destpath, "text") == 0) 
    {
      tcp_text_handler(Desthost, port);
    }
    else if(strcmp(Destpath, "binary") == 0)
    {
      tcp_binary_handler(Desthost, port);
    }
  }
  else if (strcmp(protocol, "udp") == 0)
  {
    if(strcmp(Destpath, "text") == 0) 
    {
      udp_text_handler(Desthost, port);
    }
    else if(strcmp(Destpath, "binary") == 0)
    {
      udp_binary_handler(Desthost, port);
    }
  }
  else
  {
    printf("INGET MATCHANDE PROTOCOL ELLER PATH");
    return -1;
  }
}
