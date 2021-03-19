/*

  multicast2unicast.c

  This program receives a multicast stream, and resends as a unicast stream

*/

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <errno.h>
#include <net/if.h>

void showargs()
{
  fprintf(stderr, "multicast2unicast - multicast to unicast bridge\n\n");
  fprintf(stderr, "Syntax: -m multicast_group:port -u unicast_address[:port] [-o outgoing_ip]\n");
}

int validate_multicast_addr(const char *mcstr)
{
  in_addr_t mcaddr;

  // Check for blank address
  if (mcstr[0]==0) return 0;

  // Convert address to number
  mcaddr=inet_network(mcstr);

  // Compare address to multicast ranges
  if ((mcaddr<inet_network("224.0.0.0")) || (mcaddr>inet_network("239.255.255.255")))
    return 0;

  return 1;
}

int main(int argc, char **argv)
{
  struct sockaddr_in inaddr;
  int insock;
  socklen_t inaddrlen;
  ssize_t incnt;
  char message[9000];

  struct sockaddr_in outaddr;
  int outsock;
  socklen_t outaddrlen;
  ssize_t outcnt;

  int argn=0;
  char ip_unicast[INET_ADDRSTRLEN+1]="";
  char ip_out[INET_ADDRSTRLEN+1]="";
  char multicast_addr[INET_ADDRSTRLEN+1]="";
  unsigned int port_in;
  unsigned int port_out;

	struct ip_mreq group;

  /*****************************************************************/
  /* Parse command line */

  // Check for no arguments
  if (argc==1)
  {
    showargs();
    return 1;
  }

  // Check arguments
  while (argn<argc)
  {
    if ((strcmp(argv[argn], "-m")==0) && ((argn+1)<argc))
    {
      char *colonpos;

      ++argn;

      colonpos=strstr(argv[argn], ":");
      if (colonpos!=NULL)
      {
        colonpos[0]=0;

        if (strlen(argv[argn])<INET_ADDRSTRLEN)
        {
          strcpy(multicast_addr, argv[argn]); /* */

          if (sscanf(&colonpos[1], "%5u", &port_in)==0)
            port_in=0;
        }
        else
        {
          showargs();
          return 1;
        }
      }
      else
      {
        showargs();
        return 1;
      }
    }
    else
    if ((strcmp(argv[argn], "-u")==0) && ((argn+1)<argc))
    {
      char *colonpos;

      ++argn;

      colonpos=strstr(argv[argn], ":");
      if (colonpos!=NULL)
      {
        colonpos[0]=0;

        if (strlen(argv[argn])<INET_ADDRSTRLEN)
        {
          strcpy(ip_unicast, argv[argn]);

          if (sscanf(&colonpos[1], "%5u", &port_out)==0)
            port_out=0;
        }
        else
        {
          showargs();
          return 1;
        }
      }
      else
      {
        if (strlen(argv[argn])<INET_ADDRSTRLEN)
        {
          strcpy(ip_unicast, argv[argn]);
        }
        else
        {
          showargs();
          return 1;
        }
      }
    }
    else
    if ((strcmp(argv[argn], "-o")==0) && ((argn+1)<argc))
    {
      ++argn;

      if (strlen(argv[argn])<INET_ADDRSTRLEN)
      {
        strcpy(ip_out, argv[argn]);
      }
      else
      {
        showargs();
        return 1;
      }
    }

    ++argn;
  }

  /* validate arguments */
  if ((port_in==0) || (ip_unicast[0]==0) || (validate_multicast_addr(multicast_addr)==0))
  {
    showargs();
    return 1;
  }

  /*****************************************************************/
  /* set up multicast INPUT https://www.tenouk.com/Module41c.html
   * 	struct sockaddr_in inaddr;
		int insock;
		socklen_t inaddrlen;
   */

	/* Create a datagram socket on which to receive. */
	insock=socket(AF_INET, SOCK_DGRAM, 0);
	if (insock<0)
	{
		perror("incoming socket");
		exit(1);
	}
	
	/* Enable SO_REUSEADDR to allow multiple instances of this */
	int reuse = 1;
	if(setsockopt(insock, SOL_SOCKET, SO_REUSEADDR, (char *)&reuse, sizeof(reuse)) < 0)
	{
		perror("Setting SO_REUSEADDR error");
		close(insock);
		exit(1);
	}
 
	/* Bind to the proper port number with the IP address */
	bzero((char *)&inaddr, sizeof(inaddr));
	inaddr.sin_family = AF_INET;
	inaddr.sin_port = htons(port_in);
	inaddr.sin_addr.s_addr = INADDR_ANY;

	if (bind(insock, (struct sockaddr *) &inaddr, sizeof(inaddr))<0)
	{
		perror("incoming bind");
		exit(1);
	}
  
	/* join the group */
	group.imr_multiaddr.s_addr = inet_addr("239.10.0.1");
	group.imr_interface.s_addr = inet_addr("127.0.0.1");
	if(setsockopt(insock, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char *)&group, sizeof(group)) < 0)
	{
		perror("Adding multicast group error");
		close(insock);
		exit(1);
	}
	else
		printf("Adding multicast group...OK.\n");
  

  /*****************************************************************/
  /* set up unicast output */

  outsock=socket(AF_INET, SOCK_DGRAM, 0);
  if (outsock<0)
  {
    perror("outgoing socket");
    exit(1);
  }

  bzero((char *)&outaddr, sizeof(outaddr));
  outaddr.sin_family=AF_INET;
  // outaddr.sin_addr.s_addr=inet_addr(multicast_addr);
  outaddr.sin_addr.s_addr=inet_addr(ip_unicast);
  outaddr.sin_port=htons(port_out==0?port_in:port_out);
  outaddrlen=sizeof(outaddr);

  // Check for multicast output interface override NOT NOT NOT
  if (ip_out[0]!=0)
  {
    struct in_addr iface;

    iface.s_addr=inet_addr(ip_out);
    if (setsockopt(outsock, IPPROTO_IP, IP_MULTICAST_IF, (char *)&iface, sizeof(iface))<0)
    {
      perror("setting multicast interface");
      exit(1);
     }
  }

  /*****************************************************************/
  /* main loop */

  while (1)
  {
    incnt=recvfrom(insock, message, sizeof(message), 0, (struct sockaddr *) &inaddr, &inaddrlen);

    if (incnt<0)
    {
      perror("recvfrom");
      exit(1);
    }
    else
      if (incnt==0)
      {
          break;
      }

    outcnt=sendto(outsock, message, incnt, 0, (struct sockaddr *) &outaddr, outaddrlen);
    if (outcnt<0)
    {
      perror("sendto");
      exit(1);
    }
  }

  return 0;
}
