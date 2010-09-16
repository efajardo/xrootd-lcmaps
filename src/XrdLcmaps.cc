/******************************************************************************/
/*                                                                            */
/*             X r d L c m a p s . c c                                        */
/*                                                                            */
/* (c) 2010. Brian Bockelman, UNL                                             */
/*                                                                            */
/******************************************************************************/

/* ************************************************************************** */
/*                                                                            */
/* Authz integration for LCMAPS                                               */       
/*                                                                            */
/* ************************************************************************** */

#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <pwd.h>

#include "XrdSys/XrdSysPthread.hh"
#include "XrdOuc/XrdOucLock.hh"

#include "lcmaps.h"
#include "lcmaps_account.h"
#include "lcmaps_return_account_from_pem.h"

XrdSysMutex mutex;

int XrdSecgsiAuthzInit(const char *cfg);

//
// Main function
//
extern "C"
{
char *XrdSecgsiAuthzFun(const char *pem_string, int now)
{
   // Call LCMAPS from within a mutex in order to map our user.
   // If now <= 0, initialize the LCMAPS modules.
   char * name = NULL;
   lcmaps_account_info_t paccount_info;

   // Grab the global mutex.
   XrdOucLock lock(&mutex);

   // Init the relevant fields (only once)
   if (now <= 0) {
      if (XrdSecgsiAuthzInit(pem_string) != 0) {
         return (char *)-1;
      }
      return (char *)0;
   }

   /* LCMAPS mapping call */
   if (lcmaps_account_info_init(&paccount_info) != 0) {
	return NULL;
   }

   /* -1 is the mapcounter */
   // Need char, not const char.  Don't know if LCMAPS changes it.
   char * pem_string_copy = strdup(pem_string);
   if (lcmaps_return_account_from_pem(pem_string_copy, -1, &paccount_info) != 0) {
      lcmaps_account_info_clean(&paccount_info);
      return NULL;
   }

   struct passwd * pw = getpwuid(paccount_info.uid);
   if (pw == NULL) {
      return NULL;
   }
   name = strdup(pw->pw_name);

   /* Clean up lcmaps */
   lcmaps_account_info_clean(&paccount_info);

   return name;

}
}

int XrdSecgsiAuthzUsage(int rc) {
   std::cerr << "Usage: --lcmapscfg <filename> [--osg]" << std::endl;
   return rc;
}

//
// Init the relevant parameters from a dedicated config file
//
int XrdSecgsiAuthzInit(const char *cfg)
{
   // Return 0 on success, -1 otherwise
   int osg = 0;
   char * cfg_file;
   char * log_level = NULL;

   // Convert the input string into the typical argc/argv pair
   char * cfg_copy = strdup(cfg);
   int argc = 0;
   char * token = NULL;
   while ((token = strsep(&cfg_copy, " ")) != NULL) {
      argc ++;
   }
   free(cfg_copy);
   argc = 0;
   char **argv = (char **)calloc(sizeof(char *), argc+1);
   cfg_copy = strdup(cfg);
   argv[0] = "XrdSecgsiAuthz";
   while ((token = strsep(&cfg_copy, " ")) != NULL) {
      argc ++;
      argv[argc] = token;
   }
   free(cfg_copy);

   if (argc < 3) {
      return XrdSecgsiAuthzUsage(-1);
   }

   // Use getopt to parse the appropriate options
   char c;
   static struct option long_options[] = {
      {"osg", no_argument, &osg, 1},
      {"lcmapscfg", required_argument, NULL, 'c'},
      {"loglevel", no_argument, NULL, 'l'},
      {0, 0, 0, 0}
   };
   int option_index;
   while ((c = getopt_long(argc, argv, "c:l:", long_options, &option_index)) != -1) {
      switch(c) {
         case 'c':
                  cfg_file = strdup(optarg);
                  break;
         case 'l':
                  log_level = strdup(optarg);
         default:
                  XrdSecgsiAuthzUsage(-1);
      }
   }

   setenv("LCMAPS_VERIFY_TYPE", "uid_pgid", 1);
   if (log_level == NULL) {
      log_level;
   } else {
      setenv("LCMAPS_DEBUG_LEVEL", log_level, 0);
      free(log_level);
   }

/*  This function is not currently exposed out to the world.
   if (osg != 0) {
      lcmaps_disable_voms_attributes_verification();
   }
*/

   if (lcmaps_init(0)) { // Sends the logs to syslog.
      return -1;
   }

   // Done
   return 0;
}

