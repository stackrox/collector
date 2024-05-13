BEGIN {
    print "|VM|Method|Baseline Memory median (MiB)|Test Memory median (MiB)|Memory P-value|";
    print "|---|---|---|---|---|"
}
{
    mem_warn = ($8 < 0.6) ? ":red_circle:" : ":green_circle:";
    printf "|%s|%s|%s|%s|%s|%s",$1,$2,$6,$7,mem_warn,ORS
}
