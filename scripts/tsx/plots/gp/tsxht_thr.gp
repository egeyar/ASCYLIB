load 'gp/style.gp'
set macros
NOYTICS = "set format y ''; unset ylabel"
YTICS = "set ylabel 'Throughput (Mops/s)' offset 2"
PSIZE = "set size 0.41, 0.6"

set key horiz maxrows 1

set output "eps/tsxht_thr.eps"

set terminal postscript color "Helvetica" 24 eps enhanced
set rmargin 0
set lmargin 3
set tmargin 3
set bmargin 2.5

n_algo = 4

title_offset   = -0.5
top_row_y      = 1.76
top_mid_row_y  = 1.32
mid_row_y      = 0.88
bot_mid_row_y  = 0.44
bottom_row_y   = 0.0
graphs_x_offs  = 0.1
graphs_y_offs  = 0.0
plot_size_x    = 1.65
plot_size_y    = 2.54

DIV              =    1e6
FIRST            =    2
OFFSET           =    1
column_select(i) = column(FIRST + ((i-1)*OFFSET)) / (DIV);

#LINE0 = '"lazy"'
#LINE1 = '"tsx-lazy"'
#LINE2 = '"harris"'
#LINE3 = '"tsx-harris"'
#LINE4 = '"java"'
#LINE5 = '"tsx-java"'
#LINE6 = '"copy"'
#LINE7 = '"tsx-copy"'
#LINE8 = '"clht"'
#LINE9 = '"tsx-clht"'

LINE0 = '"harris"'
LINE1 = '"new-lf"'
LINE2 = '"new-half"'
LINE3 = '"new-full"'

PLOT0 = '"Low Contention\n{(65536 elements)}"'
PLOT1 = '"Medium contention\n{8192 elements)}"'
PLOT2 = '"High contention\n{(512 elements)}"'

# font "Helvetica Bold"
set label 1 "10% Updates" at screen 0.018, screen 0.1+top_row_y      rotate by 90 font ',30' textcolor rgb "red"
set label 2 "20% Updates" at screen 0.018, screen 0.1+top_mid_row_y  rotate by 90 font ',30' textcolor rgb "red"
set label 3 "40% Updates" at screen 0.018, screen 0.1+mid_row_y      rotate by 90 font ',30' textcolor rgb "red"
set label 4 "60% Updates" at screen 0.018, screen 0.1+bot_mid_row_y  rotate by 90 font ',30' textcolor rgb "red"
set label 5 "80% Updates" at screen 0.018, screen 0.1+bottom_row_y   rotate by 90 font ',30' textcolor rgb "red"


# ##########################################################################################
# 10% Updates ##############################################################################
# ##########################################################################################


FILE0 = '"data/data.thr.ht.i65536.u10.w0.dat"'
FILE1 = '"data/data.thr.ht.i8192.u10.w0.dat"'
FILE2 = '"data/data.thr.ht.i512.u10.w0.dat"'

unset xlabel
unset key
set xtics 20 font ",18"
#set ytics 100 font ",18"

set size plot_size_x, plot_size_y
set multiplot layout 5, 2

set size 0.5, 0.6
unset title
set lmargin 3
@PSIZE

set origin 0.0 + graphs_x_offs, top_row_y
set title @PLOT0 offset 0.2, title_offset
set ylabel 'Throughput (Mops/s)' offset 2,-0.5 font ",22"
set ytics 200 font ",18"
plot for [i=1:n_algo] @FILE0 using ($1):(column_select(i)) ls i with linespoints

set origin 0.5 + graphs_x_offs, top_row_y
@PSIZE
set lmargin 4
@YTICS
set ylabel ""
unset ylabel
set title @PLOT1
set ytics 200 font ",18"
plot for [i=1:n_algo] @FILE1 using ($1):(column_select(i)) ls i with linespoints

set origin 1.0 + graphs_x_offs, top_row_y
@PSIZE
set lmargin 4
@YTICS
set ylabel ""
unset ylabel
set title @PLOT2
set ytics 100 font ",18"
plot for [i=1:n_algo] @FILE2 using ($1):(column_select(i)) ls i with linespoints


# ##########################################################################################
# 20% Updates ##############################################################################
# ##########################################################################################

FILE0 = '"data/data.thr.ht.i65536.u20.w0.dat"'
FILE1 = '"data/data.thr.ht.i8192.u20.w0.dat"'
FILE2 = '"data/data.thr.ht.i512.u20.w0.dat"'

unset title

set lmargin 3
@PSIZE
set origin 0.0 + graphs_x_offs, top_mid_row_y
#set title @PLOT0 offset 0.2,title_offset
set ylabel 'Throughput (Mops/s)' offset 2,-0.5 font ",22"
#set ytics 0.2 0.4
plot for [i=1:n_algo] @FILE0 using ($1):(column_select(i)) ls i with linespoints

set origin 0.5 + graphs_x_offs, top_mid_row_y
@PSIZE
set lmargin 4
@YTICS
set ylabel ""
unset ylabel
#set title @PLOT1
#set ytics 0.2 2
plot for [i=1:n_algo] @FILE1 using ($1):(column_select(i)) ls i with linespoints

set origin 1.0 + graphs_x_offs, top_mid_row_y
@PSIZE
@YTICS
set ylabel ""
unset ylabel
#set title @PLOT2
#set ytics 0.2 3
plot for [i=1:n_algo] @FILE2 using ($1):(column_select(i)) ls i with linespoints


# ##########################################################################################
# 40% Updates ##############################################################################
# ##########################################################################################

FILE0 = '"data/data.thr.ht.i65536.u40.w0.dat"'
FILE1 = '"data/data.thr.ht.i8192.u40.w0.dat"'
FILE2 = '"data/data.thr.ht.i512.u40.w0.dat"'

unset title

set lmargin 3
@PSIZE
set origin 0.0 + graphs_x_offs, mid_row_y
#set title @PLOT0 offset 0.2,title_offset
set ylabel 'Throughput (Mops/s)' offset 2,-0.5 font ",22"
#set ytics 0.2 0.4
plot for [i=1:n_algo] @FILE0 using ($1):(column_select(i)) ls i with linespoints

set origin 0.5 + graphs_x_offs, mid_row_y
@PSIZE
set lmargin 4
@YTICS
set ylabel ""
unset ylabel
#set title @PLOT1
#set ytics 0.2 2
plot for [i=1:n_algo] @FILE1 using ($1):(column_select(i)) ls i with linespoints

set origin 1.0 + graphs_x_offs, mid_row_y
@PSIZE
@YTICS
set ylabel ""
unset ylabel
#set title @PLOT2
#set ytics 0.2 3
plot for [i=1:n_algo] @FILE2 using ($1):(column_select(i)) ls i with linespoints


# ##########################################################################################
# 60% Updates ##############################################################################
# ##########################################################################################

FILE0 = '"data/data.thr.ht.i65536.u60.w0.dat"'
FILE1 = '"data/data.thr.ht.i8192.u60.w0.dat"'
FILE2 = '"data/data.thr.ht.i512.u60.w0.dat"'

unset title

set lmargin 3
@PSIZE
set origin 0.0 + graphs_x_offs, bot_mid_row_y
#set title @PLOT0 offset 0.2,title_offset
set ylabel 'Throughput (Mops/s)' offset 2,-0.5 font ",22"
#set ytics 0.2 0.4
plot for [i=1:n_algo] @FILE0 using ($1):(column_select(i)) ls i with linespoints

set origin 0.5 + graphs_x_offs, bot_mid_row_y
@PSIZE
set lmargin 4
@YTICS
set ylabel ""
unset ylabel
#set title @PLOT1
#set ytics 0.2 2
plot for [i=1:n_algo] @FILE1 using ($1):(column_select(i)) ls i with linespoints

set origin 1.0 + graphs_x_offs, bot_mid_row_y
@PSIZE
@YTICS
set ylabel ""
unset ylabel
#set title @PLOT2
#set ytics 0.2 3
plot for [i=1:n_algo] @FILE2 using ($1):(column_select(i)) ls i with linespoints


# ##########################################################################################
# 80% Updates ##############################################################################
# ##########################################################################################

FILE0 = '"data/data.thr.ht.i65536.u80.w0.dat"'
FILE1 = '"data/data.thr.ht.i8192.u80.w0.dat"'
FILE2 = '"data/data.thr.ht.i512.u80.w0.dat"'

set xlabel "# Threads" offset 0.0, 0.51 font ",22"

unset title

set lmargin 3
@PSIZE
set origin 0.0 + graphs_x_offs, bottom_row_y
#set title @PLOT0 offset 0.2,title_offset
set ylabel 'Throughput (Mops/s)' offset 2,-0.5 font ",22"
#set ytics 0.2 0.4
plot for [i=1:n_algo] @FILE0 using ($1):(column_select(i)) ls i with linespoints

set origin 0.5 + graphs_x_offs, bottom_row_y
@PSIZE
set lmargin 4
@YTICS
set ylabel ""
unset ylabel
#set title @PLOT1
#set ytics 0.2 2
plot for [i=1:n_algo] @FILE1 using ($1):(column_select(i)) ls i with linespoints

set origin 1.0 + graphs_x_offs, bottom_row_y
@PSIZE
@YTICS
set ylabel ""
unset ylabel
#set title @PLOT2
#set ytics 0.2 3
plot for [i=1:n_algo] @FILE2 using ($1):(column_select(i)) ls i with linespoints

unset origin
unset border
unset tics
unset xlabel
unset label
unset arrow
unset title
unset object

#Now set the size of this plot to something BIG
set size plot_size_x, plot_size_y #however big you need it
set origin 0.0, 1.1

#example key settings
# set key box 
#set key horizontal reverse samplen 1 width -4 maxrows 1 maxcols 12 
#set key at screen 0.5,screen 0.25 center top
set key font ",28"
set key spacing 1.5
set key horiz
set key width -1
set key samplen 2.5
set key at screen 0.75, screen 2.53 center top

#We need to set an explicit xrange.  Anything will work really.
set xrange [-1:1]
@NOYTICS
set yrange [-1:1]
plot \
     NaN title @LINE0 ls 1 with linespoints, \
     NaN title @LINE1 ls 2 with linespoints, \
     NaN title @LINE2 ls 3 with linespoints, \
     NaN title @LINE3 ls 4 with linespoints

#</null>
unset multiplot  #<--- Necessary for some terminals, but not postscript I don't thin
