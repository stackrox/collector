function abs(v) {return v < 0 ? -v : v}

BEGIN {
    print "|VM|Method|Baseline CPU median (%)|Test CPU median (%)|CPU P-value|";
    print "|---|---|---|---|---|"
}
{
    # Values might be distinctly different, but if the difference is small, we
    # don't bother. Currently the cut-off line is 1%, which means any
    # difference in median values less than 1% will be reported as green.
    diff = abs($3 - $4);

    cpu_warn = ($5 < 0.8 && diff > 1) ? ":red_circle:" : ":green_circle:";
    printf "|%s|%s|%s|%s|%s|%s",$1,$2,$3,$4,cpu_warn,ORS
}
