#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/time.h> //needed?
#include <sys/select.h> //needed?

#include <time.h>

//#define BUFFERSIZE 1024
#define MAX_PKT_NUM 255

/* control flag values */
#define SYN 1
#define SYN_ACK 2
#define ACK 3
#define RST 4

/* Packet data structure */
typedef struct {
  int seq; /* Sequence number */
  int ack; /* Acknowledgement number */
  int flag; /* Control flag. Indicate type of packet */
  char payload; /* Data that server sends to the client */
} Packet;

int main(int argc, char const *argv[]) {
  int sock;
  int listening_port;
  struct sockaddr_in server_address, client_address;
  int addrlen;

  int bytes_sent, bytes_recv;

  /* Check argument */
  if (argc != 2) {
    printf("Usage: %s <listen_port>\n", argv[0]);
    exit(EXIT_FAILURE);
  }
  listening_port = atoi(argv[1]);
  if (listening_port < 1025 || listening_port > 65535) {
    printf("Invalid port number (1025-65535)\n");
    exit(EXIT_FAILURE);
  }

  /* Creating socket file descriptor */
  if ((sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
    printf("Socket creation error\n");
    exit(EXIT_FAILURE);
  }

  /* Setting up the server address structure */
  server_address.sin_family = AF_INET;
  server_address.sin_addr.s_addr = INADDR_ANY;     /* Local address */
  server_address.sin_port = htons(listening_port); /* Port */

  /* Binding the socket */
  if (bind(sock, (struct sockaddr *)&server_address, sizeof(server_address)) <
      0) {
    printf("Bind failed\n");
    close(sock);
    exit(EXIT_FAILURE);
  }

  printf("Server listening on port %d\n", listening_port);

/*Step 1: 3 Way Handshake*/
//wait for SYN packet from client 

  addrlen = sizeof(client_address);
  Packet recv_packet;
  bytes_recv = 
      recvfrom(sock, &recv_packet, sizeof(Packet), 0,
               (struct sockaddr *)&client_address, (socklen_t *)&addrlen);
  printf("Recieved SYN\n");
  if (bytes_recv < 0) {
    printf("Receive error");
    close(sock);
    exit(EXIT_FAILURE);
  }

  /* Send a SYN-ACK packet in response */
  int cur_seq = recv_packet.seq; //this is the seq # of recvd pkt

  Packet send_packet;
  send_packet.flag = SYN_ACK;
  send_packet.seq = 0;
  send_packet.ack = 0;//client checks it recvd ack=0
  send_packet.payload = 0;
    bytes_sent = sendto(sock, &send_packet, sizeof(Packet), 0,
                        (struct sockaddr *)&client_address, addrlen);
  printf("Sent SYN-ACK\n");
    if (bytes_sent < 0) {
      printf("Send error");
      close(sock);
      exit(EXIT_FAILURE);
    }
    printf("Sent: pkt%d\n", send_packet.seq);

    /* Wait for ACK */
    /* receive a Packet, The flag should be
     * ACK and ack = ack server just sent */

    //do we need to do a while loop here like in client?
    recvfrom(sock, &recv_packet, sizeof(Packet), 0,
               (struct sockaddr *)&client_address, (socklen_t *)&addrlen);
    printf("Recieved ACK, handshake complete\n");

  /*Step 2: Transmission Setup*/
  //retrieve 1st pkt for window size
  //retrieve 2nd pkt for number of packets 
  int window_size, byte_count;
  recvfrom(sock, &recv_packet, sizeof(Packet), 0,
               (struct sockaddr *)&client_address, (socklen_t *)&addrlen);
  window_size = recv_packet.payload;

  recvfrom(sock, &recv_packet, sizeof(Packet), 0,
               (struct sockaddr *)&client_address, (socklen_t *)&addrlen);
  byte_count = recv_packet.payload;
  printf("Window size: %d\nTotal packets: %d\n", window_size, byte_count);

  fd_set read_fds;

  struct timeval timer;

  int ret = 0; //select(sock + 1, &read_fds, NULL, NULL, &timer);
  
  int base = 1;
  int nextseqnum = 1;
  int max_window = window_size;
  int total_packets = byte_count;
  
  int unacked_packets = 0;
  int noloss_count = 0;
  int curr_ack = recv_packet.seq;
  int correctly_acked = 0;
  
  while (base <= total_packets){
    printf("\n\nCurrent window = %d\n", window_size);
    //printf("base = %d, nextseqnum = %d\n", base, nextseqnum); 
    while(nextseqnum <= total_packets && nextseqnum < base + window_size) {
      send_packet.seq = nextseqnum;
      send_packet.flag = ACK;
      send_packet.payload = 0; 
      send_packet.ack = curr_ack;
      bytes_sent = sendto(sock, &send_packet, sizeof(Packet), 0,
                        (struct sockaddr *)&client_address, addrlen);
      //printf("base = %d, nextseqnum = %d\n", base, nextseqnum);

      printf("Sent packet seq=%d, ack%d\n", send_packet.seq, send_packet.ack);

      nextseqnum++;
      unacked_packets++;
    }
    FD_ZERO(&read_fds);
    FD_SET(sock, &read_fds);
    timer.tv_sec = 2;
    timer.tv_usec = 0;
    ret = select(sock + 1, &read_fds, NULL, NULL, &timer);
    
    //printf("ret = %d\n", ret);
    if (ret == 0) {
      nextseqnum = base;
      if (window_size > 1) {
        window_size/=2;
        //printf("window size reduced\n");
        noloss_count = 0;
      }

      printf("Timeout occured, resending packets from %d\n", nextseqnum);
    }
    else if (ret > 0 && FD_ISSET(sock, &read_fds)){
      recvfrom(sock, &recv_packet, sizeof(Packet), 0,
               (struct sockaddr *)&client_address, (socklen_t *)&addrlen);
      curr_ack = recv_packet.seq;

      printf("Recieved ACK for packet %d\n", recv_packet.ack);
      //printf("last correctly acked pkt=%d\n", correctly_acked);

      if (recv_packet.ack < base) {
        //it is indicating that it is corrupted
        continue;
      }
      
      correctly_acked = recv_packet.ack;
      //slide window
      
      if (window_size < max_window){
        if (base >= recv_packet.ack) {
          if (base == recv_packet.ack)
            noloss_count++;
          else {
            noloss_count += (recv_packet.ack - base) + 1;
          } 
        } 
        //printf("wndw = %d\n",window_size); 
        if (noloss_count >= window_size*2 && window_size*2 <= max_window){
          window_size *= 2;
          //printf("window size restored after 2 successful no pkt loss\n");
          noloss_count = 0;
        }
      }

      base = recv_packet.ack + 1;
    }
  }
  send_packet.seq = nextseqnum;
  send_packet.ack = curr_ack;
  send_packet.flag = RST;
  send_packet.payload = 0;
  sendto(sock, &send_packet, sizeof(Packet), 0, 
                  (struct sockaddr *)&client_address, addrlen);
  printf("closing connection\n");

  close(sock);
  return EXIT_SUCCESS;
}
