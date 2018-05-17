#!/bin/sh

NSTUBS=$(( $1 - 1))
CSTUBFILE=rk_stub
CPOPT="cp"
RMOPT="rm"
MAKEMULTI=Makefile.multi

OBJS="RK_APP_OBJ=${CSTUBFILE}0.o"
BINS="RK_APP_OUT=${CSTUBFILE}0.out"

rm -f $MAKEMULTI

for i in $(seq 1 $NSTUBS);
do
	OBJSTR="${CSTUBFILE}${i}"
	if [ "$2" = "$CPOPT" ]; then
		OBJS="${OBJS} ${OBJSTR}.o"
		BINS="${BINS} ${OBJSTR}.out"
		cp $CSTUBFILE.c $OBJSTR.c
	else
		rm -f $OBJSTR.c
	fi
done

echo $OBJS >> $MAKEMULTI
echo $BINS >> $MAKEMULTI
