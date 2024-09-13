#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "types.h"
#include "log.h"
#include "setting.h"

static void help()
{
    char msg[] = "Forwarder, transmit any packets to any where as you want.\n" \
    "./forwarder -p [port] -d" \
    "  -p port, service port" \
    "  -d , enable printing debug message \n";
    printf("%s", msg);
}

static int parse_argv(int argc, char **argv)
{
  int c;
  int ret = 0;

  opterr = 0;

  while ((c = getopt (argc, argv, "dp:")) != -1) {
    switch (c){
      case 'd':
        print_level = PRINT_DEBUG;
        break;
      case 'p':
        listen_port = atoi(optarg);
        if( (listen_port<1) || (listen_port>=65535))  {
            ret = 1;
            print_err("Wrong input: -%c %s", c, optarg);
        }
        break;
      default:
        ret = 1;
    }
    if( ret ) {
        help();
        return 1;
    }
  }   
  return 0;
}

extern void run_tcp_daemon_ipv4(u16 port);

int main(int argc, char **argv)
{
    if( parse_argv(argc, argv) ) {
        abort();
    }
    run_tcp_daemon_ipv4(listen_port);
}

