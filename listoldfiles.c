
/*      listoldfiles.c
 *
 *  Copyright 2011 Bob Parker <rlp1938@gmail.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *  MA 02110-1301, USA.
*/


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <getopt.h>
#include <time.h>
#include <utime.h>

char *helpmsg = "\n\tUsage: listoldfiles [option] [dir]\n"
  "\nBy default the starting dir is the user's home directory\n"
  "\n\tOptions:\n"
  "\t-h outputs this help message.\n"
  "\t-aN[MmDd] By default this is 3 years older than now.\n"
  "\t If N is followed by a suffix [MmDd] the age will be\n"
  "\t interpreted as months or days respectively\n"
  "\t-f filename. Output will be sent to this filename, default stdout\n"
;
void dohelp(int forced);
void recursedir(char *path);
void protect(const char *fn);

//Global vars
FILE *fpo;
time_t fileage;


int main(int argc, char **argv)
{
    int opt;
    int age;
    char topdir[PATH_MAX];
    char aunit = 'Y';
    struct tm *fatm;
    struct stat sb;

    // set up defaults
    age = 3;
    strcpy(topdir, getenv("HOME"));
    fpo=stdout;

    while((opt = getopt(argc, argv, ":ha:f:p:")) != -1) {
        switch(opt){
        case 'h':
            dohelp(0);
        break;
        case 'a': // change default age
            age = atoi(optarg);
            if (strchr(optarg, 'M')) aunit = 'M';
            if (strchr(optarg, 'm')) aunit = 'M';
            if (strchr(optarg, 'D')) aunit = 'D';
            if (strchr(optarg, 'd')) aunit = 'D';
        break;
        case 'f':   // named output file, not stdout.
            fpo = fopen(optarg, "w");
            if (!(fpo)) {
                perror(optarg);
                exit(EXIT_FAILURE);
            }
        case 'p':   // update mtime to now for named files in list.
                    // ie protect from being listed by this program.
                protect(optarg);
                exit(EXIT_SUCCESS);
        break;
        case ':':
            fprintf(stderr, "Option %c requires an argument\n",optopt);
            dohelp(1);
        break;
        case '?':
            fprintf(stderr, "Illegal option: %c\n",optopt);
            dohelp(1);
        break;
        } //switch()
    }//while()
    // now process the non-option arguments

    // 1.Check that argv[1] exists.
    if ((argv[optind])) {
       strcpy(topdir, argv[optind]);  // default is /home/$USER
    }

/*
struct tm {
               int tm_sec;         // seconds
               int tm_min;         // minutes
               int tm_hour;        // hours
               int tm_mday;        // day of the month
               int tm_mon;         // month
               int tm_year;        // year
               int tm_wday;        // day of the week
               int tm_yday;        // day in the year
               int tm_isdst;       // daylight saving time
           };
*/
    // Calculate the file selection date
    fileage = time(NULL);
    fatm = localtime(&fileage);
    printf("Time is: %s\n", asctime(fatm));
    switch (aunit) {
        int tmp;
        time_t tim;
        case 'Y':
            fatm->tm_year -= age;
        break;
        case 'M':
            tmp = age / 12;
            fatm->tm_year -= tmp;
            tmp = age % 12;
            if (tmp < fatm->tm_mon) {
                fatm->tm_mon -= tmp;
            } else {
                tmp = 12 - tmp;
                fatm->tm_year--;
                fatm->tm_mon += tmp;
            }
        break;
        case 'D':
            fatm->tm_mday -= age;
            tim = mktime(fatm);
            fatm = localtime(&tim);
        break;
    }
    printf("New time is: %s\n", asctime(fatm));
    fileage = mktime(fatm); // now set to the required time in the past

    // Check that the top dir is legitimate then recurse dirs
    if (stat(topdir, &sb) == -1) {
        perror(topdir);
        exit(EXIT_FAILURE);
    }
    // It exists then, but is it a dir?
    if (!(S_ISDIR(sb.st_mode))) {
        fprintf(stderr, "%s is not a directory!\n", topdir);
        exit(EXIT_FAILURE);
    }
    // now recurse
    recursedir(topdir);

    return 0;
}//main()

void dohelp(int forced)
{
  fputs(helpmsg, stderr);
  exit(forced);
}

void recursedir(char *path)
{
    char newpath[PATH_MAX];
    struct dirent *de;
    DIR *dp;

    dp = opendir(path);
    if (!(dp)) {
        perror(path);
        exit(EXIT_FAILURE);
    }
    while ((de = readdir(dp))) {
        struct stat sb;
        char filepath[PATH_MAX];
        if (strcmp(de->d_name, ".") == 0) continue;
        if (strcmp(de->d_name, "..") == 0) continue;
        if (de->d_type == DT_DIR) {
            // process this dir
            strcpy(newpath, path);
            if (newpath[strlen(newpath)-1] != '/') strcat(newpath, "/");
            strcat(newpath, de->d_name);
            recursedir(newpath);
        } else if (de->d_type == DT_REG) {
            // process regular file
            strcpy(filepath, path);
            if (filepath[strlen(filepath)-1] != '/') strcat(filepath, "/");
            strcat(filepath, de->d_name);
            if (stat(filepath, &sb) == -1) {
                perror(filepath);   // just note the error, don't abort.
            } else {
                // do the file file m time check
                time_t thisfiletime;
                thisfiletime = sb.st_mtime;
                if (thisfiletime < fileage) {
                    // report the thing
                    fprintf(fpo, "%s %s" ,filepath,
                                asctime(localtime(&thisfiletime)) );
                }

            }

        } else {
            continue;   // whatever it is, ignore it
        }

    }
    closedir(dp);
} // recursedir()

void protect(const char *fn)
{
    // fn is a file containing a list of filenames to update mtime
    // the lines in fn MUST be in the format as written out by this
    // program.
    FILE *fpi;
    char buf[PATH_MAX];
    char *dow[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    char *cp;
    int badformat, i;

    if (!(fpi = fopen(fn, "r"))) {
        perror(fn);
        exit (EXIT_FAILURE);
    }

    while (fgets(buf, PATH_MAX, fpi)) {
        // find the end of the filename
        badformat = 1;
        for (i=0; i < 7; i++) {
            cp = strstr(buf, dow[i]);
            if (cp) {
                badformat = 0;
                break;
            }
        } // for(i=...
        if (badformat) {
            fprintf(stderr, "Mal formed data line: %s\n", buf);
            exit(EXIT_FAILURE);
        }
        cp--;   // the space before DOW
        *cp = 0;
        // now have absolute pathname to file, update times
        if (utime(buf, NULL) == -1) {
            // just report the erroneous name, don't abort.
            perror(buf);
        }

    } // while()
} //protect()




