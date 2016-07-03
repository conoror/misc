/*
 *  ggetopt.h: Gumbo getopt() implementation
 */

#ifndef GUMBO_GGETOPT_H_
#define GUMBO_GGETOPT_H_

extern int   Optind;    /* next index of argv[] to be evaluated     */
extern char *Optarg;    /* pointer to any option argument           */
extern int   Optopt;    /* If ? returned this is the problem option */

extern int gumbo_getopt(int argc, char *argv[], const char *optstring);

#endif  /* GUMBO_GGETOPT_H_ */

