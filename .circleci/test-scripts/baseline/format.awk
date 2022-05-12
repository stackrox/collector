BEGIN {
    print "|Kernel|Method|Without Collector Time (secs)| With Collector Time (secs)|Baseline median (secs)|Collector median (secs)|PValue|";
    print "|---|---|---|---|---|---|---|"
}
{
    warn = ($7==1) ? ":green_circle:" : ":red_circle:";
    printf "|%s|%s|%s|%s|%s|%s|%s|%s",$1,$2,$3,$4,$5,$6,warn,ORS
}
