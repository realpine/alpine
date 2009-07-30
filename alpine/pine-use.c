#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: pine-use.c 761 2007-10-23 22:35:18Z hubert@u.washington.edu $";
#endif

/*
 * ========================================================================
 * Copyright 2006 University of Washington
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * ========================================================================
 */

#include <stdio.h>
#include <pwd.h>
#include <sys/types.h>
#include <sys/stat.h>

#ifndef MAILSPOOLPCTS
#define MAILSPOOLPCTS "/usr/spool/mail/%s"
/* #define MAILSPOOLPCTS "/usr/mail/%s" */
#endif

#define DAYSEC (60*60*24)

main(argc, argv)
     int argc;
     char **argv;
{
    struct      passwd *pw;
    char        filename[100], buf[100], *p;
    struct stat statb;
    long        now, inbox_mess, inboxes, inbox_mess_max;
    int         core_files, c, core_id, count, sig_files;
    int         user_count[6], so_far;
    FILE       *f, *core;

    user_count[0] = 0; /* Last week */
    user_count[1] = 0; /* Last 2 weeks */
    user_count[2] = 0; /* Last month */
    user_count[3] = 0; /* Last year */
    user_count[4] = 0; /* Ever */
    sig_files  = 0;
    core_files = 0;
    inboxes    = 0;
    inbox_mess = 0;
    inbox_mess_max = 0;

    now = time(0);
    core = NULL;

    if(argc > 1) {
        core_id = atoi(argv[1]);
        if(core_id == 0){
            fprintf(stderr, "Bogus core starting number\n");
            exit(-1);
        } else {
            printf("Core collect starting at %d\n", core_id);
            core = fopen("pine-core-collect.sh", "w");
        }
    } 

    so_far = 0;
    while((pw = getpwent()) != NULL) {
        so_far++;
        if((so_far % 200) == 0) {
            printf("%5d users processed so far\n", so_far);
        }

        if(strcmp(pw->pw_dir, "/") == 0)
          continue;

        sprintf(filename, "%s/.pinerc", pw->pw_dir);
        if(stat(filename, &statb) < 0)
          continue;
        if(statb.st_mtime + 7 * DAYSEC > now) 
            user_count[0]++;
        else if(statb.st_mtime + 14 * DAYSEC > now)
          user_count[1]++;
        else if(statb.st_mtime + 30 * DAYSEC > now)
          user_count[2]++;
        else if(statb.st_mtime + 365 * DAYSEC > now) 
          user_count[3]++;
        else
          user_count[4]++;


        if(statb.st_mtime + 30 * DAYSEC >= now) {
            count = mail_file_size(pw->pw_name);
            if(count >= 0){
                inboxes++;
                inbox_mess += count;
                inbox_mess_max = inbox_mess_max > count ? inbox_mess_max:count;
            }
        }

        sprintf(filename, "%s/.signature", pw->pw_dir);
        if(access(filename, 0) == 0)
          sig_files++;

        sprintf(filename, "%s/core", pw->pw_dir);
        if((f = fopen(filename, "r")) != NULL) {
            fflush(stdout);
            while((c = getc(f)) != EOF) {
                if(c == 'P'){
                    p = buf;
                    *p++ = c;
                    while((c = getc(f)) != EOF) {
                        *p++ = c;
                        if(p > &buf[50]) {
                            break;
                        }
                        if(c == ')') {
                            break;
                        }
                    }
                    *p = '\0';
                    if(c == EOF)
                      break;
                    if(strcmp(&buf[strlen(buf) - 13], "(olivebranch)") == 0) {
                        printf("%s\t%s\n", filename, buf + 14);
                        core_files++;
                        if(core != NULL) {
                            fprintf(core, "mv %s core%d.%s\n", filename,
                                    core_id++,pw->pw_name);
                        }
                        break;
                    }
                }
            }
            fclose(f);
        } else {
/*            printf("%s\n", pw->pw_name); */
        }
    }


    printf("%5d: last week\n", user_count[0]);
    printf("%5d: last two weeks (+%d)\n", user_count[1] + user_count[0],
            user_count[1]);
    printf("%5d: last month (+%d)\n", user_count[2] + user_count[1] + user_count[0], user_count[2]);
    printf("%5d: last year\n", user_count[3]);
    printf("%5d: more than a year\n", user_count[4]);
    printf("%5d: core files\n", core_files);
    printf("%5d: Average messages in inbox  (%ld/%d)\n",
           inbox_mess/inboxes, inbox_mess, inboxes);
    printf("%5d: Largest inbox in messages\n", inbox_mess_max);
    printf("%5d: Total users checked\n", so_far);
    printf("%5d: signature files\n", sig_files);
}

          
mail_file_size(user)
     char *user;
{
    int count = 0;
    FILE *f;
    char buf[20480];

    sprintf(buf, MAILSPOOLPCTS, user);

    f = fopen(buf, "r");
    if(f  == NULL)
      return(-1);

    while(fgets(buf, sizeof(buf), f) != NULL) {
        if(strncmp(buf, "From ", 5) == 0)
          count++;
    }
    fclose(f);
/*    printf("%s %d\n", user, count); */
    return(count);
}
    

    
