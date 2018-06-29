#include <math.h>
#include <syscall.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/utsname.h>

#include <cos_component.h>
#include <cos_kernel_api.h>
#include <cos_types.h>
#include <memmgr.h>


extern void F42_AppMain();
extern void OS_IdleLoop();
extern void do_emulation_setup(spdid_t id);

void cos_init(void)
{
    do_emulation_setup(cos_comp_info.cos_this_spd_id);
    PRINTC("Starting f42 main\n");
    F42_AppMain();
    PRINTC("Ending f42 main\n");
    while(1) OS_IdleLoop();
}

/* HACK: THIS CODE SHOULD NOT BE HERE */
void
DOY2MD(long Year, long DayOfYear, long *Month, long *Day)
{
      long K;

      if (Year % 4 == 0) {
         K = 1;
      }
      else {
         K = 2;
      }

      if (DayOfYear < 32) {
         *Month = 1;
      }
      else {
         *Month = (long) (9.0*(K+DayOfYear)/275.0+0.98);
      }

      *Day = DayOfYear - 275*(*Month)/9 + K*(((*Month)+9)/12) + 30;
}

double
JDToAbsTime(double JD)
{
      return((JD-2451545.0)*86400.0);
}

double
YMDHMS2JD(long Year, long Month, long Day,
          long Hour, long Minute, double Second)
{
      long A,B;
      double JD;

      if (Month < 3) {
         Year--;
         Month+=12;
      }

      A = Year/100;
      B = 2 - A + A/4;

      JD = floor(365.25*(Year+4716)) + floor(30.6001*(Month+1))
           + Day + B - 1524.5;

      JD += ((double) Hour) / 24.0 +
            ((double) Minute) / 1440.0 +
            Second / 86400.0;

      return(JD);
}
