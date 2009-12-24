#include <time.h>
#include <ctype.h>
#include <string.h>
#include <strings.h>

static const char*  months [12] = { 
    "January", "February", "March", "April", "May", "June",
    "July", "August", "September", "October", "November", "December"
};

static int getint(const char** s,int max) {
  int i,j;
  for (i=j=0; j<max; ++j) {
    if (!isdigit(**s)) {
      if (j==0) return -1; else break;
    }
    i=**s-'0';
    ++*s;
  }
  return i;
}

char* strptime(const char* s,const char* format, struct tm* tm) {
  int i,j;
  while (*format) {
    switch (*format) {
    case ' ': case '\t':
      /* match zero or more white space in input string */
      while (isblank(*s)) ++s;
      ++format;
      break;
    case '%':
      ++format;
      switch (*format) {
      case '%': if (*s=='%') ++s; else return (char*)s; break;
      case 'a': case 'A': /* weekday; we just skip */
	for (i=0; i<3; ++i)
	  if (isalpha(*s)) ++s;
	break;
      case 'b': case 'B': case 'h':
	for (i=0; i<12; ++i) {
	  if (strncasecmp(s,months[i],j=strlen(months[i])))
	    if (strncasecmp(s,months[i],j=3))
	      j=0;
	  if (j) break;
	}
	if (!j) return (char*)s;
	s+=j;
	tm->tm_mon=i;
	break;
      case 'c':
	s=strptime(s,"%b %a %d %k:%M:%S %Z %Y",tm);
	break;
      case 'C':
	i=getint(&s,2);
	if (i==-1) return (char*)s;
	tm->tm_year=(tm->tm_year%100)+(i*100);
	break;
      case 'd': case 'e':
	i=getint(&s,2);
	if (i==-1 || i>31) return (char*)s;
	tm->tm_mday=i;
	break;
      case 'D':
	s=strptime(s,"%m/%d/%y",tm);
	break;
      case 'H': case 'k':
	i=getint(&s,2);
	if (i==-1 || i>23) return (char*)s;
	tm->tm_hour=i;
	break;
      case 'I': case 'l':
	i=getint(&s,2);
	if (i==-1 || i>12) return (char*)s;
	tm->tm_hour=(tm->tm_hour/12)*12+i;
	break;
      case 'j':
	getint(&s,3);	/* not used */
	break;
      case 'm':
	i=getint(&s,2);
	if (i==-1 || i>12) return (char*)s;
	tm->tm_mon=i;
	break;
      case 'M':
	i=getint(&s,2);
	if (i==-1 || i>59) return (char*)s;
	tm->tm_min=i;
	break;
      case 'n': case 't':
	while (isblank(*s)) ++s;
	break;
      case 'p': case 'P':
	if (*s=='p' || *s=='P') tm->tm_hour=(tm->tm_hour%12)+12;
	s+=2;
	break;
      case 'r':
	s=strptime(s,"%I:%M:%S %p",tm);
	break;
      case 'R':
	s=strptime(s,"%H:%M",tm);
	break;
      case 'S':
	i=getint(&s,2);
	if (i==-1 || i>60) return (char*)s;
	tm->tm_sec=i;
	break;
      case 'T':
	s=strptime(s,"%H:%M:%S",tm);
	break;
      case 'U': case 'W':
	if (getint(&s,2)==-1) return (char*)s;
	break;
      case 'w':
	if (*s<'0' || *s>'6') return (char*)s;
	++s;
	break;
      case 'x':
	s=strptime(s,"%b %a %d",tm);
	break;
      case 'X':
	s=strptime(s,"%k:%M:%S",tm);
	break;
      case 'y':
	i=getint(&s,2);
	if (i==-1) return (char*)s;
	tm->tm_year=(tm->tm_year/100)*100+i;
	break;
      case 'Y':
	i=getint(&s,5);
	if (i==-1) return (char*)s;
	tm->tm_year=i;
	break;
      }
      ++format;
      break;
    default:
      if (*s != *format) return (char*)s;
      ++format; ++s;
      break;
      }
  }
  return (char*)s;
}
