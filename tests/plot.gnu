set style line 1 lt 1 lw 1
set style line 2 lt 2 lw 1
set style line 3 lt 3 lw 3
set style line 4 lt 4 lw 1
set style line 5 lt 5 lw 3
set style line 6 lt 7 lw 2

#set xtics 5
#set grid x

set output "linux.eps"
set terminal postscript eps 22
set data style linespoints
#set logscale y
set key right top
set xlabel "(a) Packets/Sec per Stream Sent"
#set xrange [0:59]
set yrange [0:110000]
set ylabel "Packets Processed"
#set title "Benefit of Diff Algs"
#plot "results_same_rate-linux.dat" using 2:4 title '1 interrupts' w l ls 6, "results_same_rate-linux.dat" using 2:5 title '1 processing' w l ls 2, "results_same_rate-linux.dat" using 2:6 title '2 interrupt' w l ls 3, "results_same_rate-linux.dat" using 2:7 title '2 processing' w l ls 4#, "7_2.dat" using 1:2 title 'ks fine' w l ls 5
plot "results_same_rate-linux.dat" using 2:4 title 'Task 1 & 2 interrupts (prio 0)' ls 1, "results_same_rate-linux.dat" using 2:5 title 'Task 1 processing (prio 1)' ls 3, "results_same_rate-linux.dat" using 2:7 title 'Task 2 processing (prio 2)' ls 5

set output "threaded.eps"
set terminal postscript eps 22
set data style linespoints
#set logscale y
set key right top
set xlabel "(c) Packets/Sec per Stream Sent"
#set xrange [0:59]
set yrange [0:110000]
set ylabel "Packets Processed"
#set title "Benefit of Diff Algs"
plot "results_same_rate-thd.dat" using 2:4 title 'Task 1 & 2 interrupts (prio 0, ds 7/20)' ls 1, "results_same_rate-thd.dat" using 2:5 title 'Task 1 processing (prio 1)' ls 3, "results_same_rate-thd.dat" using 2:7 title 'Task 2 processing (prio 2)' ls 5

set output "pa.eps"
set terminal postscript eps 22
set data style linespoints
#set logscale y
set key right top
set xlabel "(b) Packets/Sec per Stream Sent"
#set xrange [0:59]
set yrange [0:110000]
set ylabel "Packets Processed"
#set title "Benefit of Diff Algs"
#plot "results_same_rate-PA.dat" using 2:4 title 'Task 1 interrupts' w l ls 6, "results_same_rate-PA.dat" using 2:5 title 'Task 1 processing' w l ls 2, "results_same_rate-PA.dat" using 2:6 title 'Task 2 interrupt' w l ls 3, "results_same_rate-PA.dat" using 2:7 title 'Task 2 processing' w l ls 4#, "7_2.dat" using 1:2 title 'ks fine' w l ls 5
plot "results_same_rate-PA.dat" using 2:4 title 'Task 1 interrupts (prio 0)' ls 2, "results_same_rate-PA.dat" using 2:5 title 'Task 1 processing (prio 1)' ls 3, "results_same_rate-PA.dat" using 2:6 title 'Task 2 interrupt (prio 2)' ls 4, "results_same_rate-PA.dat" using 2:7 title 'Task 2 processing (prio 3)' ls 5

set output "new.eps"
set terminal postscript eps 22
set data style linespoints
#set logscale y
set key right top
set xlabel "(d) Packets/Sec per Stream Sent"
#set xrange [0:59]
set yrange [0:110000]
set ylabel "Packets Processed"
#set title "Benefit of Diff Algs"
#plot "results_same_rate-PA.dat" using 2:4 title 'Task 1 interrupts' w l ls 6, "results_same_rate-PA.dat" using 2:5 title 'Task 1 processing' w l ls 2, "results_same_rate-PA.dat" using 2:6 title 'Task 2 interrupt' w l ls 3, "results_same_rate-PA.dat" using 2:7 title 'Task 2 processing' w l ls 4#, "7_2.dat" using 1:2 title 'ks fine' w l ls 5
plot "results_same_rate-new.dat" using 2:4 title 'Task 1 interrupts (prio 0, ds 5/20)' ls 2, "results_same_rate-new.dat" using 2:5 title 'Task 1 processing (prio 1)' ls 3, "results_same_rate-new.dat" using 2:6 title 'Task 2 interrupt (prio 2, ds 2/20)' ls 4, "results_same_rate-new.dat" using 2:7 title 'Task 2 processing (prio 3)' ls 5

set output "threaded-diff.eps"
set terminal postscript eps 22
set data style linespoints
#set logscale y
set key right top
set xlabel "(a) Packets/Sec in Stream 2 Sent, Stream 1 Constant at 24100"
#set xrange [0:59]
set yrange [0:110000]
set ylabel "Packets Processed"
#set title "Benefit of Diff Algs"
plot "results_diff_rate-thd.dat" using 3:4 title 'Task 1 & 2 interrupts (prio 0, ds 7/20)' ls 1, "results_diff_rate-thd.dat" using 3:5 title 'Task 1 processing (prio 1)' ls 3, "results_diff_rate-thd.dat" using 3:7 title 'Task 2 processing (prio 2)' ls 5

set output "new-diff.eps"
set terminal postscript eps 22
set data style linespoints
#set logscale y
set key right top
set xlabel "(b) Packets/Sec in Stream 2 Sent, Stream 1 Constant at 24100"
#set xrange [0:59]
set yrange [0:110000]
set ylabel "Packets Processed"
#set title "Benefit of Diff Algs"
#plot "results_diff_rate-PA.dat" using 3:4 title 'Task 1 interrupts' w l ls 6, "results_diff_rate-PA.dat" using 3:5 title 'Task 1 processing' w l ls 2, "results_diff_rate-PA.dat" using 3:6 title 'Task 2 interrupt' w l ls 3, "results_diff_rate-PA.dat" using 3:7 title 'Task 2 processing' w l ls 4#, "7_2.dat" using 1:2 title 'ks fine' w l ls 5
plot "results_diff_rate-new.dat" using 3:4 title 'Task 1 interrupts (prio 0, ds 5/20)' ls 2, "results_diff_rate-new.dat" using 3:5 title 'Task 1 processing (prio 1)' ls 3, "results_diff_rate-new.dat" using 3:6 title 'Task 2 interrupt (prio 2, ds 2/20)' ls 4, "results_diff_rate-new.dat" using 3:7 title 'Task 2 processing (prio 3)' ls 5

set output "threaded-diff-48.eps"
set terminal postscript eps 22
set data style linespoints
#set logscale y
set key right top
set xlabel "(d) Packets/Sec in Stream 2 Sent, Stream 1 Constant at 48800"
#set xrange [0:59]
set yrange [0:110000]
set ylabel "Packets Processed"
#set title "Benefit of Diff Algs"
plot "results_diff_rate-thd-48.dat" using 3:4 title 'Task 1 & 2 interrupts (prio 0, ds 7/20)' ls 1, "results_diff_rate-thd-48.dat" using 3:5 title 'Task 1 processing (prio 1)' ls 3, "results_diff_rate-thd-48.dat" using 3:7 title 'Task 2 processing (prio 2)' ls 5

set output "new-diff-48.eps"
set terminal postscript eps 22
set data style linespoints
#set logscale y
set key right top
set xlabel "(e) Packets/Sec in Stream 2 Sent, Stream 1 Constant at 48800"
#set xrange [0:59]
set yrange [0:110000]
set ylabel "Packets Processed"
#set title "Benefit of Diff Algs"
#plot "results_diff_rate-PA.dat" using 3:4 title 'Task 1 interrupts' w l ls 6, "results_diff_rate-PA.dat" using 3:5 title 'Task 1 processing' w l ls 2, "results_diff_rate-PA.dat" using 3:6 title 'Task 2 interrupt' w l ls 3, "results_diff_rate-PA.dat" using 3:7 title 'Task 2 processing' w l ls 4#, "7_2.dat" using 1:2 title 'ks fine' w l ls 5
plot "results_diff_rate-new-48.dat" using 3:4 title 'Task 1 interrupts (prio 0, ds 5/20)' ls 2, "results_diff_rate-new-48.dat" using 3:5 title 'Task 1 processing (prio 1)' ls 3, "results_diff_rate-new-48.dat" using 3:6 title 'Task 2 interrupt (prio 2, ds 2/20)' ls 4, "results_diff_rate-new-48.dat" using 3:7 title 'Task 2 processing (prio 3)' ls 5

set output "new-diff-6.3.eps"
set terminal postscript eps 22
set data style linespoints
#set logscale y
set key right top
set xlabel "Packets/Sec in Stream 2 Sent, Stream 1 Constant at 48800"
#set xrange [0:59]
set yrange [0:110000]
set ylabel "Packets Processed"
#set title "Benefit of Diff Algs"
#plot "results_diff_rate-PA.dat" using 3:4 title 'Task 1 interrupts' w l ls 6, "results_diff_rate-PA.dat" using 3:5 title 'Task 1 processing' w l ls 2, "results_diff_rate-PA.dat" using 3:6 title 'Task 2 interrupt' w l ls 3, "results_diff_rate-PA.dat" using 3:7 title 'Task 2 processing' w l ls 4#, "7_2.dat" using 1:2 title 'ks fine' w l ls 5
plot "results_diff_rate-new-6.3.dat" using 3:4 title 'Task 1 interrupts (prio 0, ds 6/20)' ls 2, "results_diff_rate-new-6.3.dat" using 3:5 title 'Task 1 processing (prio 1)' ls 3, "results_diff_rate-new-6.3.dat" using 3:6 title 'Task 2 interrupt (prio 2, ds 3/20)' ls 4, "results_diff_rate-new-6.3.dat" using 3:7 title 'Task 2 processing (prio 3)' ls 5

