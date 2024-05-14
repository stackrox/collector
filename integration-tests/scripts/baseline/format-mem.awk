function abs(v) {return v < 0 ? -v : v}

BEGIN {
    print "|VM|Method|Baseline Memory median (MiB)|Test Memory median (MiB)|Memory P-value|";
    print "|---|---|---|---|---|"
}
{
    # Values might be distinctly different, but if the difference is small, we
    # don't bother. Currently the cut-off line is 10MiB, which means any
    # difference in median values less than 10MiB will be reported as green.
    diff = abs($6 - $7);
    mem_warn = ($8 < 0.8 && diff > 10) ? ":red_circle:" : ":green_circle:";
    printf "|%s|%s|%s|%s|%s|%s",$1,$2,$6,$7,mem_warn,ORS
}
