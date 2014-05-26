
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

char *helpmsg =
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
  "You may optionally use it to delete such listed files and after\n\t"
  "that delete any consequential empty directories.\n"
  "\nOPTIONS\n"
  "\t-h outputs this help message.\n"
  "\t-aN[MmDd] Choose file age, by default this is 3 years older"
  " than now.\n"
  "\t If N is followed by a suffix [MmDd] the age will be\n"
  "\t interpreted as months or days respectively, otherwise years.\n"
  "\t-o yyyymmdd[hh[mm]] Delete files older than this date.\n"
  "\t-f filename. Output will be sent to this filename, default stdout\n"
  "\t   This option is ignored unless in listing mode.\n"
  "\t-u filename. Update filetimes in files listed in filename to now.\n"
  "\t   The list of files must be in the same format as the list of\n"
  "\t   old files produced by this program.\n"
  "\t-d filename. Delete files listed in filename.\n"
  "\t   The list of files must be in the same format as the list of\n"
  "\t   old files produced by this program.\n"
  "\t-D Delete empty dirs under topdir.\n"
  "\t-s list dangling symlinks under topdir.\n"
  "\t-b filename. Delete the dangling symlinks listed in filename.\n"
;
//Global vars
FILE *fpo;
time_t fileage;
int verbose;
char *opfn;
const int eloop = 5;


struct listitem {
    char *dirname;
    struct listitem *next;
};

struct listitem *head;

struct listitem *newlistitem(void);
void dohelp(int forced);
void recursedir(char *path);
void protect(const char *fn);
void delete(const char *fn);
void listdirs(char *dirname);
int numdiritems(char *testdir);
struct listitem *insertbefore(char *name, struct listitem *head);
void delete_listed(struct listitem *head);
void delete_empties(char *topdir);
void print_listed(struct listitem *head);
void recurseprint(char *topdir);
time_t cutofftimebyage(int age, char aunit);
time_t parsetimestring(const char *dts);
int validday(int yy, int mon, int dd);
int leapyear(int yy);
FILE *dofopen(const char *fn, const char *themode);
void listdanglers(char *topdir);
void delete_danglers(char *actionfn);


int main(int argc, char **argv)
{
    int opt;
    int age, task, action;
    char topdir[PATH_MAX];
    char aunit = 'Y';
    struct stat sb;
    char *datestr, *actionfn;

    // set up defaults
    age = 3;
    strcpy(topdir, getenv("HOME"));
    fpo=stdout;
    verbose = 0;
    head = newlistitem();
    opfn = (char *)NULL;
    action = 0;
    task = 1;

    while((opt = getopt(argc, argv, ":hDa:f:u:d:o:sb:")) != -1) {
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
            age = atoi(optarg);
            if (strchr(optarg, 'M')) aunit = 'M';
            if (strchr(optarg, 'm')) aunit = 'M';
            if (strchr(optarg, 'D')) aunit = 'D';
            if (strchr(optarg, 'd')) aunit = 'D';
        break;
        case 'f':   // named output file, not stdout.
            opfn = strdup(optarg);
        break;
        case 'u':   // update mtime to now for named files in list.
                    // ie protect from being listed by this program.
                task = 3;
                actionfn = strdup(optarg);
                action++;
        break;
        case 'd':   // unlink named files in list.
                task = 4;
                actionfn = strdup(optarg);
                action++;
        break;
        case 'D':   // delete empty dirs under topdir
                task = 5;
                action++;
        break;
        case 'o':   // list files older than input file time
            task = 2;
            datestr = strdup(optarg);
        break;
        case 'e':   // list empty dir under topdir
            task = 6;   // list before named file time
        break;
        case 's':   // list dangling symlinks under topdir.
            task = 7;
        break;
        case 'b':   // delete dangling symlinks listed in named file.
            task = 8;
            actionfn = strdup(optarg);
            action++;
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

    // Check that mutually exclusive options have not been chosen.
    if (action > 1) {
        fprintf(stderr,
        "You may only select one option of -u, -D -d -b in one run!\n");
        dohelp(1);
    }
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

    // Chosse the action to be taken
    switch (task) {
        //struct tm *tim;
        case 1: // list old files by elapsed time.
            fileage = cutofftimebyage(age, aunit);
            if (opfn) fpo = dofopen(opfn, "w");
            recursedir(topdir);
        break;
        case 2: // list old files by user input cut-off date
            fileage = parsetimestring(datestr);
            if (opfn) fpo = dofopen(opfn, "w");
            recursedir(topdir);
        break;
        case 3: // update files listed in user named file to now()
            protect(actionfn);
        break;
        case 4: // delete files listed in user named file.
            delete(actionfn);
        break;
        case 5: // delete empty dirs under topdir.
            delete_empties(topdir);
        break;
        case 6: // list empty dirs under topdir in processing order.
        break;
        case 7: // list dangling symlinks under topdir.
            listdanglers(topdir);
        break;
        case 8: // delete dangling symlinks listed in actionfn.
            delete_danglers(actionfn);
        break;
        default:
        break;
    } // switch(task)
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


void protect(const char *fn)
{
    // fn is a file containing a list of filenames to update mtime
    // the lines in fn MUST be in the format as written out by this
    // program.
    FILE *fpi;
    char buf[PATH_MAX];
    char *dow[] = {" Sun", " Mon", " Tue", " Wed", " Thu", " Fri",
                    " Sat"};
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
        *cp = 0;
        // now have absolute pathname to file, update times
        if (utime(buf, NULL) == -1) {
            // just report the erroneous name, don't abort.
            perror(buf);
        }
    } // while()
} //protect()

void delete(const char *fn)
{
    // fn is a file containing a list of filenames to delete.
    // the lines in fn MUST be in the format as written out by this
    // program.
    FILE *fpi;
    char buf[PATH_MAX];
    char dir[PATH_MAX];
    char *dow[] = {" Sun ", " Mon ", " Tue ", " Wed ", " Thu ",
                    " Fri ", " Sat "};
    char *cp;
    int badformat, i;

    if (!(fpi = fopen(fn, "r"))) {
        perror(fn);
        exit (EXIT_FAILURE);
    }

    // set up directory under consideration
    dir[0] = 0;

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
        *cp = 0;
        // directory processing
        cp = strrchr(buf, '/');
        *cp = 0;
        if (strcmp(dir, buf) != 0) { // have a new dir
            if (strlen(dir)) rmdir(dir);
            // Will fail if not empty so fail silently
            strcpy(dir, buf);   // init new dir
        }
        *cp = '/';  // restore full path name to file

        // now have absolute pathname to file, delete it.
        if (unlink(buf) == -1) {
            // just report the erroneous name, don't abort.
            perror(buf);
        }

    } // while()

} // delete

void listdirs(char *topdir)
{
    // traverse dir starting from topdir and list them
    // in reverse order of depth.
    char newdir[PATH_MAX];
    struct dirent *de;
    DIR *dp;
    if (!(dp = opendir(topdir))) {
        perror(topdir);
        exit(EXIT_FAILURE);
    }
    head = insertbefore(topdir, head);
    //printf("listdirs: %s\n", topdir);
    while ((de = readdir(dp))) {
        if ((strcmp(".",de->d_name) == 0) ||
        (strcmp("..",de->d_name) == 0)) continue;
        if (de->d_type == DT_DIR) {
            strcpy(newdir, topdir);
            if (newdir[strlen(newdir) - 1] != '/') strcat(newdir, "/");
            strcat(newdir, de->d_name);
            listdirs(newdir);
        }
    } // while()
    closedir(dp);
} // listdirs()

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

void delete_empties(char *topdir)
{
    //recurseprint(topdir);
    listdirs(topdir);
    print_listed(head);
    delete_listed(head);
} // delete_empties()

void delete_listed(struct listitem *head)
{   // requires that the empty dirs are in order from lowest to highest
    while (head->dirname) {
        sync(); // required most likely
        if (rmdir(head->dirname) == -1) {
            perror(head->dirname);   // don't abort just inform
        } // if()
        head = head->next;
    } // while()
} // delete_listed()

void print_listed(struct listitem *head)
{   // requires that the empty dirs are in order from lowest to highest
    while (head->dirname) {
        printf("%s\n", head->dirname);
        head = head->next;
    } // while()
} // delete_listed()

void recurseprint(char *topdir)
{
    // just for testing recursive traversal
    DIR *dp;
    struct dirent *de;
    char newdir[PATH_MAX];
    if (!(dp = opendir(topdir))) {
        perror(topdir);
        exit(EXIT_FAILURE);
    }
    printf("Dir is:%s\n", topdir);
    while((de = readdir(dp))) {
        if (strcmp(".", de->d_name) == 0 ) continue;
        if (strcmp("..", de->d_name) == 0 ) continue;
        if (de->d_type == DT_DIR) {
            strcpy(newdir, topdir);
            if (newdir[strlen(newdir)-1] != '/') strcat(newdir, "/");
            strcat(newdir, de->d_name);
            recurseprint(newdir);
        }
    } // while()
    closedir(dp);
}

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

FILE *dofopen(const char *fn, const char *themode)
{
    FILE *f = fopen(fn, themode);
    if(f) return f;
    perror(fn);
    exit(EXIT_FAILURE);
}

void listdanglers(char *topdir)
{
    /*
     * Search from topdir recursively to find dangling symlinks
    * */
    char newpath[PATH_MAX];
    struct dirent *de;
    DIR *dp;

    dp = opendir(topdir);
    if (!(dp)) {
        perror(topdir);
        exit(EXIT_FAILURE);
    }
    while ((de = readdir(dp))) {
        struct stat sb;
        char filepath[PATH_MAX];
        time_t thisfiletime;
        if (strcmp(de->d_name, ".") == 0) continue;
        if (strcmp(de->d_name, "..") == 0) continue;
        switch (de->d_type) {
            case DT_DIR:
            // process this dir
            strcpy(newpath, topdir);
            if (newpath[strlen(newpath)-1] != '/') strcat(newpath, "/");
            strcat(newpath, de->d_name);
            listdanglers(newpath);
            break;
            case DT_LNK:
            // process symlink only
            strcpy(filepath, topdir);
            if (filepath[strlen(filepath)-1] != '/') strcat(filepath, "/");
            strcat(filepath, de->d_name);
            if (stat(filepath, &sb) == -1) {
                if (lstat(filepath, &sb) == -1) {
                    perror(filepath);
                } else {
                    // link has no target
                thisfiletime = sb.st_mtime;
                fprintf(fpo, "%s %s" ,filepath,
                            asctime(localtime(&thisfiletime)) );
                } // if (lstat
            } // if(stat..
            break;
            default:
            // ignore everything else
            continue;
            break;
        } // switch()
    }
    closedir(dp);
} // listdanglers()

void recursedir(char *path)
{
    /*
     * Output a list of old files if such exist
    */
    char newpath[PATH_MAX];
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
        char filepath[PATH_MAX];
        char newfilepath[PATH_MAX];
        if (strcmp(de->d_name, ".") == 0) continue;
        if (strcmp(de->d_name, "..") == 0) continue;

        switch (de->d_type) {
            case DT_DIR:
            // process this dir
            strcpy(newpath, path);
            if (newpath[strlen(newpath)-1] != '/') strcat(newpath, "/");
            strcat(newpath, de->d_name);
            recursedir(newpath);
            break;
            case DT_REG:
            case DT_LNK:
            // common processing for files and symlinks
            strcpy(filepath, path);
            if (filepath[strlen(filepath)-1] != '/')
                strcat(filepath, "/");
            strcat(filepath, de->d_name);
            break;
            default:
            continue;   // ignore all other d_types
            break;
        } // switch()
        // processing regular files and symlinks diverges here
        switch(de->d_type) {
            case DT_REG:
            if (stat(filepath, &sb) == -1) {
                perror(filepath);   // just note the error, don't abort.
            } else {
                // do the file file m time check
                thisfiletime = sb.st_mtime;
                if (thisfiletime < fileage) {
                    // report the thing
                    fprintf(fpo, "%s %s" ,filepath,
                                asctime(localtime(&thisfiletime)) );
                }
            }
            break;
            case DT_LNK:
            if (stat(filepath, &sb) == -1) {
                char namebuf[PATH_MAX];
                ssize_t ret;
                struct stat sb;
                switch (errno) {
                    case ELOOP:
                    /* Well I can't know how many links are in
                     * the chain but there must be at least 2
                     * so I'll send both of those names to stderr */
                     ret = readlink(filepath, namebuf, PATH_MAX-1);
                     if (ret == -1) {
                         perror(filepath);
                     } else {
                         fprintf(stderr, "%s\n", filepath);
                     }
                    break;
                    case ENOENT:
                    /* No target somewhere in the chain but I
                     * can follow the chain and send all of it to
                     * stderr */
                    /* NB stat() is bugged in that it sets errno to
                     *  ENOENT instead of ELOOP when a circular symlink
                     * is encountered. Consequently I have implemented
                     * my own test for circularity. */
                    // first up, output this name and time to stderr.
                    if ((lstat(filepath, &sb)) == -1) {
                        perror(filepath);
                    } else {
                        int loopcounter = 0;
                        thisfiletime = sb.st_mtime;
                        fprintf(stderr, "%s %s" ,filepath,
                                asctime(localtime(&thisfiletime)) );
                        strcpy(newfilepath, filepath);
                        while ((ret = readlink(newfilepath,
                                namebuf, PATH_MAX-1)) != -1) {
                            loopcounter++;
                            if (loopcounter > eloop) break;
                            namebuf[ret] = '\0';
                            if (lstat(namebuf, &sb) == -1) {
                                perror(namebuf);
                            } else {
                                thisfiletime = sb.st_mtime;
                                fprintf(stderr, "%s %s" ,namebuf,
                                    asctime(localtime(&thisfiletime)) );
                            }
                            strcpy(newfilepath, namebuf);
                        } // while()
                    } // if/else(lstat..
                    break;
                    default:
                    perror(filepath);   // just report it
                    break;
                }
            } else {
                // stat() succeeded so the link target exists
                thisfiletime = sb.st_mtime;
                if (thisfiletime < fileage) {
                    // report the thing
                    fprintf(fpo, "%s %s" ,filepath,
                                asctime(localtime(&thisfiletime)) );
                }
            }
            break;
        } // switch()
    }
    closedir(dp);
} // recursedir()

void delete_danglers(char *fn)
{
    /* */
    char buf[PATH_MAX];
    char dir[PATH_MAX];
    char *dow[] = {" Sun ", " Mon ", " Tue ", " Wed ", " Thu ",
                    " Fri ", " Sat "};
    char *cp;
    int badformat, i;
    FILE *fpi;
    if (!(fpi = fopen(fn, "r"))) {
        perror(fn);
        exit (EXIT_FAILURE);
    }

    // set up directory under consideration
    dir[0] = 0;

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
        *cp = 0;
        // directory processing
        cp = strrchr(buf, '/');
        *cp = 0;
        if (strcmp(dir, buf) != 0) { // have a new dir
            if (strlen(dir)) rmdir(dir);
            // Will fail if not empty so fail silently
            strcpy(dir, buf);   // init new dir
        }
        *cp = '/';  // restore full path name to file

        // now have absolute pathname to file, delete it.
        if (unlink(buf) == -1) {
            // just report the erroneous name, don't abort.
            perror(buf);
        }
    } // while()
}// delete_danglers()





