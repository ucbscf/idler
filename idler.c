/*
 * idler - Displays the name of (hopefully) idle machine(s)
 *
 * Author: Phil Spector <spector@stat.Berkeley.EDU>
 *
 * Usage: idler [ -nNUMBER -tSECONDS -sN -l -m -r ] hostname [ hostnames ... ]
 *
 *  -nNUMBER    Show NUMBER machine with lowest load; default is 1.
 *  -tSECONDS   Set timeout for rpc calls to SECONDS; default is 10.
 *  -sN         Use the Nth value of the load average for sorting.
 *              1 for current; 2 for 5 minute; 3 for 15 minute;
 *              default is 1 for current.
 *  -l          Display the actual load averages; default is to
 *              show only the machine names.
 *  -r          Reverse the sorting to display the highest loaded machines.
 *              
 */

#ifndef lint
static char sccs_id[] = "@(#)idler.c	1.4  (SCF)   01/13/97";
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#ifdef SVR4
/*
 * The symbol PORTMAP should be defined so that the appropriate function
 * declarations for the old interfaces are included through the header
 * files.  see rpc_soc(3N) on solaris 2.x 
 */
#define PORTMAP
#endif
#include <rpc/rpc.h>
#ifndef SVR4
#include <utmp.h>		/* for rusers.h */
#include <sys/param.h>		/* for FSCALE */
#include <sys/time.h>		/* for time() */
#else
#include <time.h>		/* for time() */
#endif
#include <rpcsvc/rstat.h>
#include <rpcsvc/rusers.h>
#include <math.h>

void usage (void);

struct LINFO
{
  struct LINFO *next;
  char *hostname;
  long load[3];
};

struct LINFO *head, *tail, *srch, *best[100], *allocf ();
void replacef (), freef ();
void prhost (struct LINFO *node, int printl);
int randint ();
static int rev;
static char *prog;

int
main (int argc, char **argv)
{
  char **machines = NULL;
  char *hostnow;
  double useval;
  struct LINFO *lnow;
  int i, j, k, k1, nmach = 0, done, count, tout = 10;
  int printl = 0, luse = 0, nlist = 1;
  struct statstime st;
  enum clnt_stat clnt_stat;
  struct hostent *hp;
  struct timeval timeout1, timeout2;
  struct sockaddr_in server_addr;
  int sock = RPC_ANYSOCK;
  register CLIENT *client;
  long iy;

  /*
   * get basename 
   */
  if ((prog = strrchr (argv[0], '/')) == NULL)
    prog = argv[0];
  else
    prog++;

  rev = 0;
  for (i = 1; i < argc && argv[i][0] == '-'; i++)
    {
      switch (argv[i][1])
	{
	case 'l':
	  printl = 1;
	  break;
	case 'r':
	  rev = 1;
	  break;
	case 'n':
	  if (argv[i][2])
	    nlist = atoi (argv[i] + 2);
	  else if (i == argc - 1)
	    nlist = 3;
	  else
	    nlist = atoi (argv[++i]);

	  if (nlist < 1)
	    {
	      fprintf (stderr,
		       "%s: Value for -n should be positive. Option ignored.\n",
		       prog);
	      nlist = 1;
	    }
	  break;
	case 's':
	  if (argv[i][2])
	    luse = atoi (argv[i] + 2) - 1;
	  else if (i == argc - 1)
	    luse = 1;
	  else
	    luse = atoi (argv[++i]) - 1;

	  if (luse < 1 || luse > 3)
	    {
	      fprintf (stderr,
		       "%s: Value for -s must be either 1, 2, or 3. Option ignored.\n",
		       prog);
	      luse = 1;
	    }
	  break;
	case 't':
	  if (argv[i][2])
	    tout = atoi (argv[i] + 2);
	  else if (i == argc - 1)
	    tout = 10;
	  else
	    tout = atoi (argv[++i]);
	  break;
	default:
	  fprintf (stderr, "%s: Unrecognized option \"%s\" ignored.\n", prog,
		   argv[i]);
	  break;
	}			/* switch */
    }				/* for */

  if (i < argc)
    {				/* user gave machines list on command line 
				 */
      machines = &argv[i];
      nmach = argc - i;
    }
  else
    {				/* use default machine list */
      usage ();
      exit (0);
    }				/* use default machine list */

  timeout1.tv_sec = tout;
  timeout1.tv_usec = 0;
  timeout2.tv_sec = tout;
  timeout2.tv_usec = 0;

  count = 0;
  k = k1 = -1;

  for (i = 0; i < nmach; i++)
    {
      hostnow = machines[i];

      if ((hp = gethostbyname (hostnow)) == NULL)
	{
	  fprintf (stderr,
		   "%s: Warning: Can't find address for %s; skipping.\n",
		   prog, hostnow);
	  continue;
	}

      strncpy ((caddr_t) & (server_addr.sin_addr.s_addr), hp->h_addr,
	       hp->h_length);

      server_addr.sin_family = AF_INET;
      server_addr.sin_port = 0;
      if ((client =
	   (CLIENT *) clntudp_create (&server_addr, RSTATPROG, RSTATVERS_TIME,
				      timeout1, &sock)) == NULL)
	{
	  clnt_pcreateerror (hostnow);
	  continue;
	}

      clnt_stat =
	clnt_call (client, RSTATPROC_STATS, xdr_void, 0, xdr_statstime,
		   (char *) &st, timeout2);



      if (clnt_stat != RPC_SUCCESS && clnt_stat == RPC_PROGVERSMISMATCH)
	{
	  fprintf (stderr,
		   "%s: rpc program version mismatch detected on host %s\n",
		   prog, hostnow);
	}

      if (clnt_stat != RPC_SUCCESS)
	{
	  fprintf (stderr, "%s: Warning: rpc call failed for host %s\n", prog,
		   hostnow);
	  continue;
	}

      if (head == NULL)
	{
	  head = tail = allocf (hostnow, st.avenrun, (struct LINFO *) NULL);
	  count = 1;
	}
      else if (count < nmach)
	{
	  srch = head;
	  while (srch->next != NULL)
	    {
	      if (rev ? srch->next->load[luse] < st.avenrun[luse]
		  : srch->next->load[luse] > st.avenrun[luse])
		break;
	      srch = srch->next;
	    }
	  if (srch == head)
	    {
	      if (rev ? st.avenrun[luse] > head->load[luse]
		  : st.avenrun[luse] < head->load[luse])
		head = allocf (hostnow, st.avenrun, head);
	      else
		{
		  head->next = lnow =
		    allocf (hostnow, st.avenrun, head->next);
		  if (head == tail)
		    tail = lnow;
		}
	    }
	  else if (srch->next == NULL)
	    tail = (tail->next =
		    allocf (hostnow, st.avenrun, (struct LINFO *) NULL));
	  else
	    srch->next = allocf (hostnow, st.avenrun, srch->next);
	  count++;
	}
      else if (rev ? st.avenrun[luse] > tail->load[luse]
	       : st.avenrun[luse] < tail->load[luse])
	{
	  srch = head;
	  while (srch->next != NULL)
	    {
	      if (rev ? srch->next->load[luse] < st.avenrun[luse]
		  : srch->next->load[luse] > st.avenrun[luse])
		break;
	      srch = srch->next;
	    }
	  if (srch->next == NULL)
	    {
	      replacef (hostnow, st.avenrun, tail);
	      continue;
	    }
	  else if (srch == head)
	    {
	      if (rev ? st.avenrun[luse] > head->load[luse]
		  : st.avenrun[luse] < head->load[luse])
		head = allocf (hostnow, st.avenrun, head);
	      else
		head->next = allocf (hostnow, st.avenrun, srch->next);
	    }
	  else
	    srch->next = allocf (hostnow, st.avenrun, srch->next);

#ifdef OLD
	  while (srch != NULL)
	    {
	      if (srch->next == tail)
		break;
	      srch = srch->next;
	    }
	  freef (tail);
	  tail = srch;
	  tail->next = NULL;
#endif	   /*OLD*/
	}
    }

  srch = head;
  if (srch == NULL)
    {
      fprintf (stderr, "%s: error: no machines reporting!\n", prog);
      exit (1);
    }
  k1 = 0;
  if (nlist == 1)
    {
      useval = srch->load[luse];
      while (srch != NULL)
	{
	  if (srch->load[luse] == useval)
	    best[k1++] = srch;
	  else
	    break;
	  srch = srch->next;
	}

      if (k1 > 1)
	{
	  iy = (long) time ((time_t *) NULL) * 124458771;
	  j = randint ((long) k1, &iy);
	  prhost (best[j], printl);
	}
      else
	prhost (head, printl);
    }
  else
    while (srch != NULL)
      {
	if (k1 == nlist)
	  break;
	prhost (srch, printl);
	k1++;
	srch = srch->next;
      }

  exit (0);
}

void
prhost (struct LINFO *node, int printl)
{
  printf ("%s", node->hostname);

  if (printl)
    {
      printf ("\t");
      if ((int) strlen (node->hostname) < 8)
	printf ("\t");
      printf ("%6.2f%6.2f%6.2f",
	      (double) node->load[0] / FSCALE,
	      (double) node->load[1] / FSCALE,
	      (double) node->load[2] / FSCALE);
    }
  printf ("\n");
}


struct LINFO *
allocf (char *name, long *load, struct LINFO *next)
{

  struct LINFO *lnow;
  int i;

  if ((lnow = (struct LINFO *) calloc ((unsigned) 1,
				       (unsigned) sizeof (struct LINFO))) ==
      NULL)
    {
      fprintf (stderr, "s: internal error in allocation; exiting.\n");
      exit (1);
    }

  lnow->hostname = malloc ((unsigned) strlen (name) + 1);
  strcpy (lnow->hostname, name);
  for (i = 0; i < 3; i++)
    lnow->load[i] = load[i];
  lnow->next = next;

  return (lnow);
}

void
replacef (char *name, long *load, struct LINFO *lnow)
{
  int i;

  if ((int) strlen (name) > (int) strlen (lnow->hostname))
    {
      free (lnow->hostname);
      lnow->hostname = malloc ((unsigned) strlen (name) + 1);
    }
  strcpy (lnow->hostname, name);
  for (i = 0; i < 3; i++)
    lnow->load[i] = load[i];
}

void
freef (struct LINFO *lnow)
{
  free (lnow->hostname);
  free (lnow);
}

int
randint (long lim, long *iy)
{
  double urand ();

  if (!(*iy % 2))
    (*iy)++;

  return ((int) (lim * urand (iy)));
}

double
urand (long *iy)
{
  /*
   * c implementation of FMM program urand 
   */

  static long m2 = 0;
  long ia, ic, mic;
  double s, halfm;
  long m;
  m = (long) 1;

  if (m2 == 0)
    {
      while (m > 0 && m > m2)
	{
	  m2 = m;
	  m = (long) 2 *m2;
	  halfm = m2;
	}

      ia = 8 * (long) (halfm * atan (1.) / 8.) + 5;
      ic = 2 * (long) (halfm * (.5 - sqrt (3.) / 6.)) + 1;
      mic = (m2 - ic) + m2;

      s = .5 / halfm;
    }


  *iy *= ia;
  if (*iy > mic)
    *iy = (*iy - m2) - m2;
  *iy += ic;
  if (*iy / 2 > m2)
    *iy = (*iy - m2) - m2;
  if (*iy < 0)
    *iy = (*iy + m2) + m2;

  return ((double) *iy * s);
}

void
usage (void)
{
  fputs ("Usage: idler [ OPTIONS ] HOSTNAME [ HOSTNAME ... ]\n\
Displays the name of an idle machine\n\
\n\
    -nNUMBER        Show NUMBER machine with lowest load; default is 1.\n\
    -tSECONDS       Set timeout for rpc calls to SECONDS; default is 10.\n\
    -sN             Use the Nth value of the load average for sorting.\n\
                    1 for current; 2 for 5 minute; 3 for 15 minute;\n\
                    default is 1 for current.\n\
    -l              Display the actual load averages; default is to\n\
                    show only the machine names.\n\
    -r              Reverse the sorting to display the highest loaded machines.\n\
", stdout);

  return;
}
