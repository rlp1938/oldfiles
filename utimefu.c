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
    * and return the time if all ok;
   */
   char dts[16];
   struct tm dt;
   int yy, mm, dd, hh, min;
   min = hh = dd = mm = yy = 0;
   // seconds will never be set here, also minutes & hours may not be.

   strcpy(dts, timestr);
   switch(strlen(dts)) {
        case 12:
            min = atoi(&dts[10]);
            if ((min < 0 ) || (min > 59)) {
                fprintf(stderr, "Illegal value for minutes: %d\n",
                        min);
                dohelp(1);
            }
            dts[10] = '\0';
        case 10:
            hh = atoi(&dts[8]);
            if ((hh < 0 ) || (hh > 23)) {
                fprintf(stderr, "Illegal value for hours: %d\n",
                        hh);
                dohelp(1);
            }
            dts[8] = '\0';
        case 8:
            dd = atoi(&dts[6]);
            if ((dd < 1 ) || (dd > 31)) { // rough enough for now
                fprintf(stderr, "Illegal value for days: %d\n",
                        dd);
                dohelp(1);
            }
            dts[6] = '\0';
            mm = atoi(&dts[4]);
            if ((mm < 1 ) || (mm > 12)) {
                fprintf(stderr, "Illegal value for months: %d\n",
                        mm);
                dohelp(1);
            }
            dts[4] = '\0';

            yy = atoi(dts);
        break;
        default:
        fprintf(stderr, "%s is not formatted correctly\n", dts);
        dohelp(1);
        break;
        // now test if our days are valid for the month
    } // switch()

    if (!(validday(yy, mm, dd))) {
        fprintf(stderr, "For the year %d, month %d, %d"
                        " days is invalid\n",
                        yy, mm, dd);
        exit(EXIT_FAILURE);
    }
    // now assign to the tm struct
    dt.tm_sec = 0;          // seconds - not used here
    dt.tm_min = min;        // minutes
    dt.tm_hour = hh;        // hours
    dt.tm_mday = dd;        // day in month
    dt.tm_mon = mm - 1;     // month # 0..11
    dt.tm_year = yy - 1900; // year offset from 1900
    dt.tm_wday = 0;         // week day number - not used here
    dt.tm_yday = 0;         // day # in year - not used here
    dt.tm_isdst = 0;        // daylight savings time - ignored here
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




