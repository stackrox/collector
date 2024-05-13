BEGIN {
    print "|VM|Method|Baseline CPU median (cores)|Test CPU median (cores)|CPU P-value|";
    print "|---|---|---|---|---|"
}
{
    cpu_warn = ($5 < 0.6) ? ":red_circle:" : ":green_circle:";
    printf "|%s|%s|%s|%s|%s|",$1,$2,$3,$4,cpu_warn,ORS
}
