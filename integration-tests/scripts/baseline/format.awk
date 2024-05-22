BEGIN {
    print "|VM|Method|Baseline CPU median (cores)|Test CPU median (cores)|CPU P-value|Baseline Memory median (MiB)|Test Memory median (MiB)|Memory P-value|";
    print "|---|---|---|---|---|---|---|---|"
}
{
    cpu_warn = ($5 < 0.6) ? ":red_circle:" : ":green_circle:";
    mem_warn = ($8 < 0.6) ? ":red_circle:" : ":green_circle:";
    printf "|%s|%s|%s|%s|%s|%s|%s|%s|%s",$1,$2,$3,$4,cpu_warn,$6,$7,mem_warn,ORS
}
