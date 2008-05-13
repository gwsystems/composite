set style line 1 lt 1 lw 3
set style line 2 lt 2 lw 3
set style line 3 lt 3 lw 3
set style line 4 lt 4 lw 3
set style line 5 lt 5 lw 3
set style line 6 lt 7 lw 3

#set xtics 5
#set grid x

set output "linux.eps"
set terminal postscript eps color 22
set data style linespoints
#set logscale y
set key right top
set xlabel "Packets/Sec per Stream Sent"
#set xrange [0:59]
set yrange [0:90000]
set ylabel "Packets Processed"
#set title "Benefit of Diff Algs"
#plot "results_same_rate-linux.dat" using 2:4 title '1 interrupts' w l ls 6, "results_same_rate-linux.dat" using 2:5 title '1 processing' w l ls 2, "results_same_rate-linux.dat" using 2:6 title '2 interrupt' w l ls 3, "results_same_rate-linux.dat" using 2:7 title '2 processing' w l ls 4#, "7_2.dat" using 1:2 title 'ks fine' w l ls 5
plot "results_same_rate-linux.dat" using 2:4 title '1 interrupts', "results_same_rate-linux.dat" using 2:5 title '1 processing', "results_same_rate-linux.dat" using 2:6 title '2 interrupt', "results_same_rate-linux.dat" using 2:7 title '2 processing'#, "7_2.dat" using 1:2 title 'ks fine'

set output "pa.eps"
set terminal postscript eps color 22
set data style linespoints
#set logscale y
set key right top
set xlabel "Packets/Sec per Stream Sent"
#set xrange [0:59]
set yrange [0:90000]
set ylabel "Packets Processed"
#set title "Benefit of Diff Algs"
#plot "results_same_rate-PA.dat" using 2:4 title '1 interrupts' w l ls 6, "results_same_rate-PA.dat" using 2:5 title '1 processing' w l ls 2, "results_same_rate-PA.dat" using 2:6 title '2 interrupt' w l ls 3, "results_same_rate-PA.dat" using 2:7 title '2 processing' w l ls 4#, "7_2.dat" using 1:2 title 'ks fine' w l ls 5
plot "results_same_rate-PA.dat" using 2:4 title '1 interrupts', "results_same_rate-PA.dat" using 2:5 title '1 processing', "results_same_rate-PA.dat" using 2:6 title '2 interrupt', "results_same_rate-PA.dat" using 2:7 title '2 processing'#, "7_2.dat" using 1:2 title 'ks fine'

