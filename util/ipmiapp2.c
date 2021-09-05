/*
 * ipmi_sample.c
 *
 * A sample IPMI utility, to which more commands can be added.
 *
 * 02/27/06 Andy Cress - created 
 * 02/25/11 Andy Cress - added get_chassis_status 
 */
/*M*
Copyright (c) 2007, Kontron America, Inc.
All rights reserved.

Redistribution and use in source and binary forms, with or without 
modification, are permitted provided that the following conditions are met:

  a.. Redistributions of source code must retain the above copyright notice, 
      this list of conditions and the following disclaimer. 
  b.. Redistributions in binary form must reproduce the above copyright notice,
      this list of conditions and the following disclaimer in the documentation 
      and/or other materials provided with the distribution. 
  c.. Neither the name of Kontron America, Inc. nor the names of its contributors 
      may be used to endorse or promote products derived from this software 
      without specific prior written permission. 

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND 
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED 
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE 
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR 
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES 
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; 
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON 
ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS 
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *M*/
#ifdef WIN32
#include <windows.h>
#include <stdio.h>
#include "getopt.h"
#else
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#if defined(HPUX)
/* getopt is defined in stdio.h */
#elif defined(MACOS)
/* getopt is defined in unistd.h */
#include <unistd.h>
#else
#include <getopt.h>
#endif
#endif
#include <string.h>
#include "ipmicmd.h"
#include <sys/stat.h> 
/*
 * Global variables 
 */
static char * progname  = "ipmiapp2";
static char * progver   = "1.0";
static char   fdebug    = 0;
static char   fset_mc  = 0;
static uchar  g_bus  = PUBLIC_BUS;
static uchar  g_sa   = BMC_SA;
static uchar  g_lun  = BMC_LUN;
static uchar  g_addrtype = ADDR_SMI;
static char  *mytag = NULL;
static char  *sdrfile = NULL;

static int get_chassis_status(uchar *rdata, int rlen)
{
   uchar idata[4];
   uchar ccode;
   int ret;

   ret = ipmi_cmdraw( CHASSIS_STATUS, NETFN_CHAS, g_sa,g_bus,g_lun,
                        idata,0, rdata,&rlen,&ccode, fdebug);
   if (ret == 0 && ccode != 0) ret = ccode;
   return(ret);
}  /*end get_chassis_status()*/
#define MAX_PERMITTED_KEY_SIZE 0x2000
#define NETFN_AMI              0xD0
#define CMD_SET_DEBUG_INFO     0x43
#define CTRL_UPLOAD_CERTIFICATE 1
static int upload_certificate(char *keyfile)
{
   uchar idata[MAX_PERMITTED_KEY_SIZE+1];
   int idata_len;
   uchar rdata[255];
   int rlen;
   uchar ccode;
   int ret;
   struct stat finfo;
   char *pBuffer = NULL;
   FILE *fp;

   if(strstr(keyfile, ".pem") == NULL)
   {
        printf("Invalid Public Key File format\n");
        return -1;
   } else 
   if(0 != stat(mytag, &finfo))
   {
        printf("Public Key File not Found.\n");
        return -1;
   } else 
   if(finfo.st_size > MAX_PERMITTED_KEY_SIZE)
   {
        printf("Key File size is exceeded\n");
        return -1;
   } 
    pBuffer = malloc(finfo.st_size);
    if( pBuffer == NULL )
    {
        printf("Error while allocating the memory for pubkey file\n");
        return -1;
    }

    fp = fopen(mytag, "r");
    if( fp == NULL)
    {
        printf("Error while opening the pubkey file\n");
        free(pBuffer);
        return -1;
    }
    if(finfo.st_size != fread(pBuffer, 1, finfo.st_size, fp))
    {
        fclose(fp);
        printf("Error while reading the pubkey file\n");
        free(pBuffer);
        return -1;
    }
    fclose(fp);

    idata[0] = CTRL_UPLOAD_CERTIFICATE;
    memcpy(idata+1, pBuffer, finfo.st_size);
    free(pBuffer);
    idata_len = finfo.st_size+1;
   ret = ipmi_cmdraw( CMD_SET_DEBUG_INFO, NETFN_AMI, g_sa,g_bus,g_lun,
                        idata, idata_len, rdata,&rlen,&ccode, fdebug);
   if (ret == 0 && ccode != 0) ret = ccode;
   return(ret);
}  /*end get_chassis_status()*/


#ifdef WIN32
int __cdecl
#else
int
#endif
/**
 * use -t to mark the pem file
 */

main(int argc, char **argv)
{
   int ret = 0;
   char c;
   uchar devrec[16];
   char *s1;
   int loops = 1;
   int nsec = 10;
   char *nodefile = NULL;
   int done = 0;
   FILE *fp = NULL;
   char nod[40]; char usr[24]; char psw[24];
   char drvtyp[10];
   char biosstr[40];
   int n;
#ifdef GET_SENSORS
   uchar *sdrlist;
#endif

   printf("%s ver %s\n", progname,progver);

   while ((c = getopt( argc, argv,"i:l:m:p:f:s:t:xEF:N:P:R:T:U:V:YZ:?")) != EOF ) 
      switch(c) {
          case 'm': /* specific IPMB MC, 3-byte address, e.g. "409600" */
                    g_bus = htoi(&optarg[0]);  /*bus/channel*/
                    g_sa  = htoi(&optarg[2]);  /*device slave address*/
                    g_lun = htoi(&optarg[4]);  /*LUN*/
                    g_addrtype = ADDR_IPMB;
                    if (optarg[6] == 's') {
                             g_addrtype = ADDR_SMI;  s1 = "SMI";
                    } else { g_addrtype = ADDR_IPMB; s1 = "IPMB"; }
                    fset_mc = 1;
                    ipmi_set_mc(g_bus,g_sa,g_lun,g_addrtype);
                    printf("Use MC at %s bus=%x sa=%x lun=%x\n",
                            s1,g_bus,g_sa,g_lun);
                    break;
          case 'f': nodefile = optarg; break; /* specific sensor tag */
          case 'l': loops = atoi(optarg); break; 
          case 'i': nsec = atoi(optarg); break;  /*interval in sec*/
          case 's': sdrfile = optarg; break; 
          case 't': mytag = optarg; break; /* specific sensor tag */
          case 'x': fdebug = 1;     break;  /* debug messages */
          case 'p':    /* port */
          case 'N':    /* nodename */
          case 'U':    /* remote username */
          case 'P':    /* remote password */
          case 'R':    /* remote password */
          case 'E':    /* get password from IPMI_PASSWORD environment var */
          case 'F':    /* force driver type */
          case 'T':    /* auth type */
          case 'V':    /* priv level */
          case 'Y':    /* prompt for remote password */
          case 'Z':    /* set local MC address */
                parse_lan_options(c,optarg,fdebug);
		if (c == 'F') strncpy(drvtyp,optarg,sizeof(drvtyp));
                break;
	  default:
                printf("Usage: %s [-filmstx -NUPREFTVY]\n", progname);
                printf(" where -x       show eXtra debug messages\n");
                printf("       -f File  use list of remote nodes from File\n");
                printf("       -i 10    interval for each loop in seconds\n");
                printf("       -l 10    loops sensor readings 10 times\n");
		printf("       -m002000 specific MC (bus 00,sa 20,lun 00)\n");
                printf("       -s File  loads SDRs from File\n");
                printf("       -t tag   search for 'tag' in SDRs\n");
		print_lan_opt_usage(1);
                exit(1);
      }
   /* Rather than parse_lan_options above, the set_lan_options function 
    * could be used if the program already knows the nodename, etc. */
 ret = get_BiosVersion(biosstr);
 if (ret == 0) printf("BIOS Version: %s\n",biosstr);

 while(!done)
 {

   if (nodefile != NULL) {
      /* This will loop for each node in the file if -f was used.
       * The file should contain one line per node: 
       *    node1 user1 password1
       *    node2 user2 password2
       */
      if (fp == NULL) {
         fp = fopen(nodefile,"r");
         if (fp == NULL) {
            printf("Cannot open file %s\n",nodefile);
            ret = ERR_FILE_OPEN;
	    goto do_exit;
         }
         if (fdebug) printf("opened file %s ok\n",nodefile);
      }
      n = fscanf(fp,"%s %s %s", nod, usr, psw);
      if (fdebug) printf("fscanf returned %d \n",n);
      if (n == EOF || n <= 0) {
        fclose(fp);
	    done = 1;
	    goto do_exit;
      }
      printf("Using -N %s -U %s -P %s ...\n",nod,usr,psw);
      if (n > 0) parse_lan_options('N',nod,0);
      if (n > 1) parse_lan_options('U',usr,0);
      if (n > 2) parse_lan_options('P',psw,0);
      if (drvtyp != NULL) parse_lan_options('F',drvtyp,0);
   }
   if ( mytag == NULL) {
       printf( "use -t to specify the certificate file name\n");
       done = 1;
       continue;
   } 
 
   ret = ipmi_getdeviceid(devrec,16,fdebug);
   if (ret != 0) {
	printf("Cannot do ipmi_getdeviceid, ret = %d\n",ret);
	goto do_exit;
   } else {  /*success*/
       uchar ipmi_maj, ipmi_min;
       ipmi_maj = devrec[4] & 0x0f;
       ipmi_min = devrec[4] >> 4;
       show_devid( devrec[2],  devrec[3], ipmi_maj, ipmi_min);
   }

   ret = upload_certificate(mytag);
   if (ret == 0) {
 	   printf("Upload Certificate ok\n");
   }

   ipmi_close_();
   if (nodefile == NULL) done = 1;
  } /*end while not done */

do_exit:
   show_outcome(progname,ret);
   exit (ret);
}  /* end main()*/

/* end ipmi_sample.c */
