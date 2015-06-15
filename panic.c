#include <aura/aura.h>
#include <execinfo.h>
#include <signal.h>

#define TRACE_LEN 36

/* Obtain a backtrace and print it to stdout. */
void aura_panic(struct aura_node *node)
{
  void *array[TRACE_LEN];
  size_t size;
  char **strings;
  size_t i;

  size = backtrace (array, TRACE_LEN);
  strings = backtrace_symbols (array, size);

  aura_transport_dump_usage();

  slog(0, SLOG_DEBUG, "--- Dumping aura stack ---");
  for (i = 0; i < size; i++)
	  slog(0, SLOG_DEBUG, "%s", strings[i]);

  /* TODO: Dump interesting stuff from node var */
  
  free (strings);
  exit(128);
}


static void handler(int sig, siginfo_t *si, void *unused)
{
	slog(0, SLOG_FATAL, "AURA got a segmentation fault, this is bad");
	aura_panic(NULL);
}


void __attribute__((constructor (101))) reg_seg_handler() {	   
	struct sigaction sa;
	sa.sa_flags = SA_SIGINFO;
	sigemptyset(&sa.sa_mask);
	sa.sa_sigaction = handler;
	sigaction(SIGSEGV, &sa, NULL);
}
