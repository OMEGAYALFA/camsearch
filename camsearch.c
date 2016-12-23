/*
 * camsearch - search for ip cameras
 * Copyright (C) 2016 Benjamin Abendroth
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * TODO:
 *    - use poll() instead of threads
 */

#include <err.h>
#include <time.h>
#include <errno.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>

#include <pthread.h>
#include <semaphore.h>

#include <arpa/inet.h>
#include <sys/socket.h>

#define SEARCH_FOR "Server: Hipcam"
#define SERVER_RESPONSE_BUFSIZE 290
#define MIN_SERVER_RESPONSE 30

#define DEFAULT_THREADS 255
#define DEFAULT_TIMEOUT 5

#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)

#define USAGE \
"Usage: %s REQUIRED [OPTIONAL]\n\n"                                    \
"Searches for Hipcams in given IP range. Prints results to STDOUT\n\n" \
"\nREQUIRED\n"                                                         \
" -f <from_ip>                Specify begin of IP range\n"             \
" -t <to_ip>                  Specify end of IP range\n"               \
" -p <port> [-p <port>...]    Specify ports to check\n"                \
"\nOPTIONAL\n"                                                         \
" -c <position>               Continue scan at given position\n"       \
" -r                          Pseudo-randomize IP range\n"             \
" -T <connect-timeout>        Specify connection timeout (default:"STR(DEFAULT_TIMEOUT)")\n"  \
" -n <max-threads>            Specify max number of threads (default:"STR(DEFAULT_THREADS)")\n"

// HTTP header for "http://user:user@<IP>/tmpfs/snap.jpg"
#define REQUEST_HEADER                   \
"GET /tmpfs/snap.jpg HTTP/1.1\r\n"       \
"Authorization: Basic dXNlcjp1c2Vy\r\n"  \
"Accept: */*\r\n\r\n"

#define IP_SEGMENTS(IP) IP.seg.a, IP.seg.b, IP.seg.c, IP.seg.d
#define SPACES "                                                             "
#define r_fprintf(file, fmt, args...) fprintf(file, fmt""SPACES"\r", ##args)

typedef union ip_t {
   uint32_t ip;
   struct {
      uint8_t d;
      uint8_t c;
      uint8_t b;
      uint8_t a;
   } seg;
} ip_t;

typedef struct rand_range_t {
   int range;
   int div;
   int _divided_range;
   int _i;
} rand_range_t;

// forward declarations
static void exit_handler(void);
static void signal_int_handler(int);
static void signal_term_handler(int);
static void signal_tstp_handler(int);
static void signal_cont_handler(int);

static void iterate_ips(union ip_t, union ip_t);
static void randomize_ips(union ip_t, union ip_t);

static void rand_range_init(rand_range_t *r, int range, int div);
static int rand_range_next(rand_range_t *r, int *dest);

// global variables
static int              max_threads = DEFAULT_THREADS;
static sem_t            thread_count;

static struct timeval   socket_timeout = {.tv_sec  = DEFAULT_TIMEOUT, .tv_usec = 0};
static struct linger    socket_linger  = {.l_onoff = 0, .l_linger = 10};

static int              num_ports = 0;
static uint16_t         *ports = NULL;
static int              continue_pos = 0;


int main(int argc, char *argv[])
{
   union ip_t start = (union ip_t)(uint32_t) 0;
   union ip_t stop  = (union ip_t)(uint32_t) 0;

   void (*iterate_function)(union ip_t, union ip_t) = &iterate_ips;

   {
    opterr = 0;
    int c;
    while ((c = getopt(argc, argv, "c:n:f:t:p:T:hr")) != -1)
       switch (c) {
          case 'h':
             return printf(USAGE, argv[0]), 0;
          case 'c':
             if (! (continue_pos = atoi(optarg)))
                errx(1, "invalid parameter for -c: %s\n", optarg);
             break;
          case 'n':
             if (! (max_threads = atoi(optarg)))
                errx(1, "invalid parameter for -n: %s\n", optarg);
             break;
          case 'f':
             if (start.ip = inet_network(optarg), (start.ip == 0 || start.ip == -1))
                errx(1, "invalid start ip: %s\n", optarg);
             break;
          case 't':
             if (stop.ip = inet_network(optarg), (stop.ip == 0 || stop.ip == -1))
                errx(1, "invalid stop ip: %s\n", optarg);
             break;
          case 'p':
             if (! (ports = realloc(ports, 1 + num_ports * sizeof(char*))))
                return perror("realloc()"), 1;
             if (! (ports[num_ports] = htons(atoi(optarg))))
                errx(1, "invalid port: %s\n", optarg);
             ++num_ports;
             break;
          case 'T':
             if (! (socket_timeout.tv_sec = atoi(optarg)))
                errx(1, "invalid parameter for -T: %s\n", optarg);
             break;
          case 'r':
             iterate_function = &randomize_ips;
             break;
          case '?':
             fprintf(stderr, USAGE, argv[0]);
             fprintf(stderr, "\n");

             if (strchr("cnftpT", optopt))
                fprintf(stderr, "Option -%c requires an argument.\n", optopt);
             else if (isprint(optopt))
                fprintf(stderr, "Unknown option `-%c'.\n", optopt);
             else
                fprintf(stderr, "Unknown option character `\\x%x'.\n", optopt);

             return 1;
          default:
             abort();
       }

    if (optind < argc)
       errx(1, "unneeded arguments provided");
    if (! start.ip)
       errx(1, "must specify -f <from_ip>");
    if (! stop.ip)
       errx(1, "must specify -t <to_ip>");
    if (! num_ports)
       errx(1, "must specify at least one -p <port>");
   }

   // when piping to files, we want to see our results immediately, too
   setbuf(stdout, NULL);

   sem_init(&thread_count, 0, max_threads);

   printf("# from:%u.%u.%u.%u to:%u.%u.%u.%u threads:%d timeout:%ld ports:%d",
           IP_SEGMENTS(start), IP_SEGMENTS(stop), max_threads,
           socket_timeout.tv_sec, ntohs(ports[0]));
   for (int i = 1; i < num_ports; printf(",%u", ntohs(ports[i++])));
   printf("\n");

   atexit(exit_handler);
   signal(SIGINT,  signal_int_handler);
   signal(SIGTERM, signal_term_handler);
   signal(SIGTSTP, signal_tstp_handler);
   signal(SIGCONT, signal_cont_handler);

   (*iterate_function)(start, stop);
}

static
void *disco_thread(void *void_ip)
{
   /* ip is passed by-value through the void pointer */
   union ip_t ip = (union ip_t)(uint32_t)(uintptr_t)void_ip;

   int sock;
   struct sockaddr_in server;
   char reply[SERVER_RESPONSE_BUFSIZE];

   server.sin_family      = AF_INET;
   server.sin_addr.s_addr = htonl(ip.ip);

   for (int port_i = num_ports; port_i--; ) {
      server.sin_port = ports[port_i]; // getopt did htons() 

      if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
         perror("socket()");
         sleep(1);
         continue;
      }

      setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char*)&socket_timeout, sizeof(socket_timeout));
      setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (char*)&socket_timeout, sizeof(socket_timeout));
      setsockopt(sock, SOL_SOCKET, SO_LINGER,   (void*)&socket_linger,  sizeof(socket_linger));  
      
      if (connect(sock, (struct sockaddr *)&server, sizeof(server)) < 0) {
         close(sock);

         if (errno == EHOSTUNREACH)
            { break; } // host unreachable? do not try other ports!
         //else if (errno != ECONNREFUSED && errno != EINPROGRESS)
         //   { perror("connect()"); }
         else 
            { continue; }
      }

      if (send(sock, REQUEST_HEADER, sizeof(REQUEST_HEADER)-1, 0) < 0)
         { goto CLOSE_CONNECTION; }

      if (recv(sock, reply, SERVER_RESPONSE_BUFSIZE, 0) < MIN_SERVER_RESPONSE)
         { goto CLOSE_CONNECTION; }

      /* check for "200" in "HTTP/1.0 200 OK"
      if (reply[9] != '2' || reply[10] != '0' || reply[11] != '0')
         { goto CLOSE_CONNECTION; }
      */

      // +16, skip http-status in search
      if (strstr(reply+16, SEARCH_FOR)) {
         flockfile(stdout); // don't mess up our output
         r_fprintf(stderr, "Match %03d.%03d.%03d.%03d:%d", IP_SEGMENTS(ip), ntohs(server.sin_port));
         printf("%d.%d.%d.%d:%d\n", IP_SEGMENTS(ip), ntohs(server.sin_port));
         funlockfile(stdout);
      }

      CLOSE_CONNECTION:
      shutdown(sock, SHUT_RDWR);
      close(sock);
   }

   sem_post(&thread_count);
   return NULL;
}

static
void check_ip(union ip_t ip)
{
   static pthread_t t;
   sem_wait(&thread_count);

   while (pthread_create(&t, NULL, &disco_thread, (void*)(uintptr_t)ip.ip))
      perror("pthread_create()"), sleep(1);

   pthread_detach(t);
   ++continue_pos;
}

static
void iterate_ips(union ip_t start, union ip_t stop)
{
   start.ip += continue_pos;

   while (start.ip++ <= stop.ip) {
      if (start.seg.b == 255 || start.seg.c == 255 || start.seg.d == 255)
         { continue; }
      if (start.seg.d == 0)
         { continue; }

      if (start.ip % 155 == 0) // don't print too much!
         r_fprintf(stderr, "Probing %03d.%03d.%03d.%03d", IP_SEGMENTS(start));

      check_ip(start);
   }
}

static
void randomize_ips(union ip_t start, union ip_t stop)
{
   int range = stop.ip - start.ip;
   int range_result;
   ip_t current_ip;
   rand_range_t rand_range;
   rand_range_init(&rand_range, range, 100);

   for (int i = 0; i < continue_pos && rand_range_next(&rand_range, &range_result); ++i);

   while (rand_range_next(&rand_range, &range_result)) {
      current_ip.ip = start.ip + range_result;

      if (current_ip.seg.b == 255 || current_ip.seg.c == 255 || current_ip.seg.d == 255)
         { continue; }
      if (current_ip.seg.d == 0)
         { continue; }

      //printf("Probing %03d.%03d.%03d.%03d\n", IP_SEGMENTS(current_ip));

      if (rand_range._i % 155 == 0) // don't print too much!
         r_fprintf(stderr, "Probing %03d.%03d.%03d.%03d [% 9d IPs left]",
            IP_SEGMENTS(current_ip),
            (range - rand_range._i)
         );

      check_ip(current_ip);
   }
}

static
int rand_range_next(rand_range_t *r, int *dest) {
   if (r->_i >= r->range)
      return 0;

   // we cannot devide the range into section, just count up
   if (r->div >= r->range) {
      *dest = (r->_i)++;
      return 1;
   }

   const int section = r->_i % r->div;
   *dest = (r->_divided_range * section + (r->_i / r->div));

   if (*dest >= r->range) {
      *dest -= (r->_i % r->div) + 1;
   }

   return ++(r->_i);
}

static
void rand_range_init(rand_range_t *r, int range, int div) {
   r->range = range;
   r->div = div;
   r->_divided_range = (range / div) + (!!(range % div));
   r->_i = 0;
}

static
void sem_wait_all() {
   // waiting for owning all semaphore locks, meaning there are no more threads
   for (int i = max_threads; i--; sem_wait(&thread_count))
      if ((i >= 300 && i % 64 == 0) || // limit output
          (i <  200 && i % 16 == 0))
         r_fprintf(stderr, "Waiting for %d threads", i);
}

static inline
void sem_post_all() {
   for (int i = max_threads; i--; sem_post(&thread_count));
}

static inline
void print_continue_message() {
   fprintf(stderr, "You can continue the scan by passing '-c %d' to the program\n", continue_pos);
}

static
void signal_int_handler(int sig) {
   print_continue_message();
   signal(SIGINT, SIG_DFL);
   fprintf(stderr, "Waiting for threads to terminate (press ^C again to force quit) ...\n");
   exit(130); // exit_handler takes care of cleanup
}

static
void signal_term_handler(int sig) {
   exit(0);
}

static
void signal_cont_handler(int sig) {
   fprintf(stderr, "Waking up...\n");
   sem_post_all();
}

static
void signal_tstp_handler(int sig) {
   print_continue_message();
   sem_wait_all();
   fprintf(stderr, "Sleeping...\n");
   raise(SIGSTOP); // note: SIGSTOP, not SIGTSTP
}

static
void exit_handler(void) {
   sem_wait_all();
   sem_destroy(&thread_count);
   free(ports);
}
