/*      utimefu.c
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
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <time.h>
#include <sys/stat.h>
#include <utime.h>

char *helpmsg =
  "NAME\n\tutimefu - sets file access and modification times.\n"
  "\nSYNOPSIS"
  "\n\tutimefu filedate file1 ... fileN \n"
  "\tfilenames must refer to existing files and filedate\n"
  "\tmust be in the form yyyymmdd[hh[mm]]\n"
  "\nDESCRIPTION\n"
  "\tIt's necessary to set arbitrary file times to test programs\n"
  "\tIand tht'a the purpose of this program.\n"
  "\nOPTIONS:\n"
  "\t-h outputs this help message.\n"
  ;

// Global vars

void dohelp(int forced);
time_t parsetimestring(const char *dts);
int validday(int yy, int mon, int dd);
int leapyear(int yy);

int main(int argc, char **argv)
{
    int opt, filecount;
    struct stat sb;
    char *fn;
    char dts[16];
    time_t tim;
    struct utimbuf utb;

    while((opt = getopt(argc, argv, ":h")) != -1) {
        switch(opt){
        case 'h':
            dohelp(0);
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

    // Check that a date/time string has been entered
    if (!(argv[optind])) {
        fprintf(stderr, "No date string provided.\n");
        dohelp(1);
    } else {
        strcpy(dts, argv[optind]);
    }

    // Now check that date data is in the right format.
    tim = parsetimestring(dts);

    // Now for the file(s)
    filecount = 0;
    optind++;
    while (argv[optind]) {
        if (stat(argv[optind], &sb) == -1) {
            perror(argv[optind]);
            exit(EXIT_FAILURE);
        } else {
            fn = argv[optind];
            utb.actime = tim;
            utb.modtime = tim;
            utime(fn, &utb);
        }
        filecount++;
        optind++;
    } // while()

    if (!(filecount)) {
        fprintf(stderr, "No filename provided.\n");
        dohelp(1);
    }
    return 0;
}//main()

void dohelp(int forced)
{
  fputs(helpmsg, stderr);
  exit(forced);
}

time_t parsetimestring(const char *timestr)
{
   /*
    * check the string in dts for valid values
    * and return the struct if all ok;
   */
   char dts[16];
   struct tm dt;

   // seconds will never be set here, also minutes & hours may not be.
   dt.tm_sec = 0;
   dt.tm_min = 0;
   dt.tm_hour = 0;

   strcpy(dts, timestr);
   switch(strlen(dts)) {
        case 12:
            dt.tm_min = atoi(&dts[10]);
            if ((dt.tm_min < 0 ) || (dt.tm_min > 59)) {
                fprintf(stderr, "Illegal value for minutes: %d\n",
                        dt.tm_min);
                dohelp(1);
            }
            dts[10] = '\0';
        case 10:
            dt.tm_hour = atoi(&dts[8]);
            if ((dt.tm_hour < 0 ) || (dt.tm_hour > 23)) {
                fprintf(stderr, "Illegal value for hours: %d\n",
                        dt.tm_hour);
                dohelp(1);
            }
            dts[8] = '\0';
        case 8:
            dt.tm_mday = atoi(&dts[6]);
            if ((dt.tm_mday < 1 ) || (dt.tm_mday > 31)) { // rough enough for now
                fprintf(stderr, "Illegal value for days: %d\n",
                        dt.tm_mday);
                dohelp(1);
            }
            dts[6] = '\0';
            dt.tm_mon = atoi(&dts[4]) - 1;
            if ((dt.tm_mon < 1 ) || (dt.tm_mon > 12)) {
                fprintf(stderr, "Illegal value for months: %d\n",
                        dt.tm_mon);
                dohelp(1);
            }
            dts[4] = '\0';

            dt.tm_year = atoi(dts) - 1900;
        break;
        default:
        fprintf(stderr, "%s is not formatted correctly\n", dts);
        dohelp(1);
        break;
        // now test if our days are valid for the month
    } // switch()

    dt.tm_wday = 0;
    dt.tm_yday = 0;
    dt.tm_isdst = 0;    // deal with dailight savings time later

    if (!(validday(dt.tm_year, dt.tm_mon, dt.tm_mday))) {
        fprintf(stderr, "For the year %d, month %d, %d"
                        " days is invalid\n",
                        dt.tm_year, dt.tm_mon, dt.tm_mday);
        exit(EXIT_FAILURE);
    }
    return mktime(&dt);
} // parsetimestring()

int validday(int yy, int mon, int dd)
{
    int daysinmonth[13] = {0,31,28,31,30,31,30,31,31,30,31,30,21};
    if (mon == 2) {
        daysinmonth[2] += leapyear(yy);
    }
    if (dd <= daysinmonth[mon]) return 1;
    return 0;
} // validday()

int leapyear(int yy)
{
    if (yy % 4 != 0) return 0;
    if (yy % 400 == 0) return 1;
    if (yy % 100 == 0) return 0;
    return 1; // yy % 4 == 0
} // leapyear()



