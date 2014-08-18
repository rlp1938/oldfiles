/*      oldfiles.c
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
#include <string.h>
#include <errno.h>
#include <getopt.h>
#include <time.h>
#include <utime.h>
#include <libgen.h>
#include "fileutil.h"
static char *helpmsg =
  "NAME\n\toldfiles - lists old files and optionally deletes them"
  " or renews file\n\ttimes using the generated list."
  "\tAlso provides an option to delete empty\n\tdirectories.\n"
  "SYNOPSIS"
  "\n\toldfiles [option] [topdir]\n"
  "\n\tLists files under topdir which are older than the default age\n"
  "\tof 3 years to stdout, unless other options are used.\n"
  "\tBy default topdir is the user's home directory.\n"
  "\nDESCRIPTION\n"
  "\tOldfiles when run with no options or target directory specified,\n\t"
  "simply lists all files in the users home directory which are\n\t"
  "older than the default age of 3 years.\n\t"
  "\nOPTIONS\n"
  "\t-h outputs this help message.\n"
  "\t-aN[MmDd] Choose file age, by default this is 3 years older"
  " than now.\n"
  "\t If N is followed by a suffix [MmDd] the age will be\n"
  "\t interpreted as months or days respectively, otherwise years.\n"
  "\t-o yyyymmdd[hh[mm]] Delete files older than this date.\n"
;
//Global vars
static FILE *fpo;
static time_t fileage;
static char *opfn;
static const int eloop = 5;
static int oldcount;
static const char *pathend = "!*END*!";	// Anyone who puts shit like
										// that in a filename deserves
										// what happens.


struct listitem {
    char *dirname;
    struct listitem *next;
};

static struct listitem *head;

struct listitem *newlistitem(void);
static void recursedir(char *topdir);
static char *dostrdup(const char *s);
static void dohelp(int forced);
int numdiritems(char *testdir);
struct listitem *insertbefore(char *name, struct listitem *head);
time_t cutofftimebyage(int age, char aunit);
time_t parsetimestring(const char *dts);
int validday(int yy, int mon, int dd);
int leapyear(int yy);
static void dorealpath(char *givenpath, char *resolvedpath);
static void  dosystem(const char *cmd);
static char** workfiles(const char *dir, const char *progname,
						int numfiles);
static void* domalloc(size_t thesize);
static void stripinode(const char *fnamein, const char *fnameout);

int main(int argc, char **argv)
{
    int opt;
    int age;
    char topdir[PATH_MAX];
    char aunit = 'Y';
    struct stat sb;
    char *datestr;
	char command[PATH_MAX];
	char **workfile;

    // set up defaults
    age = 3;
    strcpy(topdir, getenv("HOME"));
    head = newlistitem();
    opfn = (char *)NULL;
    oldcount = 0;
    workfile = workfiles("/tmp/", argv[0], 4);
    fpo=dofopen(workfile[0], "w");

    while((opt = getopt(argc, argv, ":ha:o:")) != -1) {
        switch(opt){
        /* I have no idea what the value of topdir will be during
         * options processing so all I can do is set a task variable
         * to select action after topdir is known or whether it's
         * actually needed.
        */
        case 'h':
            dohelp(0);
        break;
        case 'a': // change default age
            age = strtol(optarg, NULL, 10);
            if (strchr(optarg, 'M')) aunit = 'M';
            if (strchr(optarg, 'm')) aunit = 'M';
            if (strchr(optarg, 'D')) aunit = 'D';
            if (strchr(optarg, 'd')) aunit = 'D';
        break;
        case 'o':   // list files older than input file time
            datestr = strdup(optarg);
            fileage = parsetimestring(datestr);
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

    // 1.See if argv[1] exists.
    if ((argv[optind])) {
       strcpy(topdir, argv[optind]);  // default is /home/$USER
    }

    // Check that the top dir is legitimate.
    if (stat(topdir, &sb) == -1) {
        perror(topdir);
        exit(EXIT_FAILURE);
    }
    // It exists then, but is it a dir?
    if (!(S_ISDIR(sb.st_mode))) {
        fprintf(stderr, "%s is not a directory!\n", topdir);
        exit(EXIT_FAILURE);
    }

	// Convert relative path to absolute if needed.
	if (topdir[0] != '/') dorealpath(argv[optind], topdir);

    fileage = cutofftimebyage(age, aunit);
    recursedir(topdir);
    fclose(fpo);
    if (oldcount > 0) {
		sprintf(command, "sort -u %s > %s", workfile[0],
					workfile[1]);
		dosystem(command);
	} else {
		fprintf(stdout, "No old files found\n");
		unlink(workfile[0]);
	}
	// get rid of the leading inode and sort on pathname
	stripinode(workfile[1], workfile[2]);
	sprintf(command, "sort %s > %s", workfile[2],
					workfile[3]);
	dosystem(command);
	dumpfile(workfile[3], stdout);

    return 0;
}//main()

void dohelp(int forced)
{
  fputs(helpmsg, stderr);
  exit(forced);
}

struct listitem *newlistitem(void)
{
    struct listitem *lip;
    lip = malloc(sizeof(struct listitem));
    if (!(lip)) {
        perror("newlistitem()");
        exit (EXIT_FAILURE);
    }
    return lip;
} // newlistitem()

int numdiritems(char *testdir)
{   // counts number of dir items other than directories
    DIR *dp;
    int items = 0;
    struct dirent *de;
    dp = opendir(testdir);
    while ((de = readdir(dp))) {
        if ((strcmp(".",de->d_name) == 0) ||
        (strcmp("..",de->d_name) == 0)) continue;
        items++;
    } // while()
    closedir(dp);
    return items;
} // numdiritems()

struct listitem *insertbefore(char *name, struct listitem *head)
{
    struct listitem *li = newlistitem();
    li->dirname = strdup(name);
    li->next = head;
    return li;
} // insertbefore()

time_t cutofftimebyage(int age, char aunit)
{
    // Calculate the file selection date
    time_t fileage;
    struct tm *fatm;

    fileage = time(NULL);
    fatm = localtime(&fileage);
    // printf("Time is: %s\n", asctime(fatm));
    switch (aunit) {
        case 'Y':
            fatm->tm_year -= age;
        break;
        case 'M':
        /*
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
        */
            fatm->tm_mon -= age;
        break;
        case 'D':
            fatm->tm_mday -= age;
        break;
    }
    return mktime(fatm);
} // cutofftimebyage()

time_t parsetimestring(const char *timestr)
{
   /*
    * check the string in dts for valid values
    * and return the time if all ok;
   */
   char dts[16];
   struct tm dt;

   // seconds will never be set here, also minutes & hours may not be.
   dt.tm_sec = 0;
   dt.tm_min = 0;
   dt.tm_hour = 0;
   dt.tm_wday = 0;
   dt.tm_yday = 0;

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
            if ((dt.tm_mon < 0 ) || (dt.tm_mon > 11)) {
                fprintf(stderr, "Illegal value for months: %d\n",
                        dt.tm_mon+1);
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


    if (!(validday(dt.tm_year+1900, dt.tm_mon+1, dt.tm_mday))) {
        fprintf(stderr, "For the year %d, month %d, %d"
                        " days is invalid\n",
                        dt.tm_year+1900, dt.tm_mon+1, dt.tm_mday);
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

void recursedir(char *path)
{
    /*
     * Output a list of old files if such exist
    */
    struct dirent *de;
    DIR *dp;

    dp = opendir(path);
    if (!(dp)) {
        perror(path);
        exit(EXIT_FAILURE);
    }
    while ((de = readdir(dp))) {
        time_t thisfiletime;
        struct stat sb;
        char newpath[PATH_MAX];
        if (strcmp(de->d_name, ".") == 0) continue;
        if (strcmp(de->d_name, "..") == 0) continue;
        strcpy(newpath, path);
        if (newpath[strlen(newpath)-1] != '/') strcat(newpath, "/");
        switch (de->d_type) {
            case DT_DIR:
            // process this dir
            strcat(newpath, de->d_name);
            recursedir(newpath);
            break;
            case DT_REG:
            strcat(newpath, de->d_name);
            if (stat(newpath, &sb) == -1) {
                perror(newpath);   // just note the error, don't abort.
                break;
            }
            // do the file m time check
            thisfiletime = sb.st_mtime;
            if (thisfiletime < fileage) {
				// report the thing
                fprintf(fpo, "%.16lx %s%s %s" ,sb.st_ino, newpath,
					pathend, asctime(localtime(&thisfiletime)));
				oldcount++;
            }
            break;
            case DT_LNK:
            /* symlink processing.
             I once did have separate processing for errors ELOOP
             and ENOENT but circular links are simply reported as
             ENOENT along with missing links. So I'll just let perror
             take care of it all. */

			/* stat() gives me times applicable to the target not the
			 link, but unlink() will remove the link, not the target.
			 That much is fine because, if old, I want to remove the
			 link as well as the target. */

            if (stat(newpath, &sb) == -1) {
                perror(newpath);   // just note the error, don't abort.
                break;
			}
            // do the file m time check
            thisfiletime = sb.st_mtime;
            if (thisfiletime < fileage) {
				/* NB if link or links are within the given search dir,
				 * the target will be reported more than once. Sort -u
				 * will take care of such happenings.*/

				char target[PATH_MAX];
                // report the symlink
                fprintf(fpo, "%.16lx %s%s %s" ,sb.st_ino, newpath, pathend,
                            asctime(localtime(&thisfiletime)) );
                 oldcount++;
				// Dealt with the link, now report the target of the link
				dorealpath(newpath, target);
				fprintf(fpo, "%.16lx %s%s %s" ,sb.st_ino, target, pathend,
                            asctime(localtime(&thisfiletime)) );
                 oldcount++;
            }
            break;

           default:
            continue;   // ignore all other d_types
            break;


		}
	}
    closedir(dp);
} // recursedir()

void dorealpath(char *givenpath, char *resolvedpath)
{	// realpath() witherror handling.
	if(!(realpath(givenpath, resolvedpath))) {
		perror("realpath()");
		exit(EXIT_FAILURE);
	}
} // dorealpath()

void dosystem(const char *cmd)
{
    const int status = system(cmd);

    if (status == -1) {
        fprintf(stderr, "system to execute: %s\n", cmd);
        exit(EXIT_FAILURE);
    }

    if (!WIFEXITED(status) || WEXITSTATUS(status)) {
        fprintf(stderr, "%s failed with non-zero exit\n", cmd);
        exit(EXIT_FAILURE);
    }

    return;
} // dosystem()

char *dostrdup(const char *s)
{
	/*
	 * strdup() with in built error handling
	*/
	char *cp = strdup(s);
	if(!(cp)) {
		perror(s);
		exit(EXIT_FAILURE);
	}
	return cp;
} // dostrdup()

char** workfiles(const char *dir, const char *progname, int numfiles)
{
	// return a list of workfile[0].. workfile[numfiles-1] containing
	// $USERprogname<0> .. $USERprogname<numfiles-1>.
	// workfile[numfiles] will be (char*)NULL.
	char **workfile;
	char progfile[256];
	char username[256];
	char work[PATH_MAX];
	int i;

	strcpy(work, progname);
	strcpy(progfile, basename(work));
	strcpy(username, getenv("USER"));
	workfile = domalloc(sizeof(char *) * (numfiles + 1));

	for(i=0; i<numfiles; i++){
		sprintf(work, "%s%s%s%d", dir, username, progfile, i);
		workfile[i] = dostrdup(work);
	} // for()
	workfile[numfiles] = (char *)NULL;
	return workfile;
} // worfiles()

static void* domalloc(size_t thesize)
{	// malloc() with error handling
	void *ptr = malloc(thesize);
	if (!(ptr)) {
		perror("malloc()");
		exit(EXIT_FAILURE);
	}
	return ptr;
}

void stripinode(const char *fnamein, const char *fnameout)
{
	FILE *fpo;
	struct fdata fdat;
	char *bol, *eol;

	fpo = dofopen(fnameout, "w");
	fdat = readfile(fnamein, 0, 1);
	bol = fdat.from +17;
	while(bol < fdat.to) {
		eol = memchr(bol, '\n', PATH_MAX);
		fwrite(bol, 1, eol-bol+1, fpo);
		bol = eol + 18;
	} // while()
	fclose(fpo);
	free (fdat.from);
} // stripinode()

