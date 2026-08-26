# Benchmark STALL V4 R2

Benchmarkul este read-only și separat de MobileGL. Nu pornește thread de
sampling, nu citește stackul altui thread și nu face census GL. Hot hooks citesc
două ceasuri și scriu în tablouri primitive bounded; sortarea, proof I/O și
raportarea se fac la ieșirea din lume.

Raportează:

- intervale de frame: average FPS, 1% low, 0.1% low, p50/p95/p99/p99.9/max;
- hitch counts la 16/33/50/100 ms și top 8 hitch-uri;
- render wall și CPU pe threadul curent;
- `GPU_OR_DRIVER_OR_SYNC_WAIT` ca **inferență** `wall - currentThreadCpu`, nu
  hardware GPU timer;
- ferestre de 10 secunde, astfel încât zona aglomerată să nu fie ascunsă de
  FPS-ul mare din zona liberă;
- 15 stage timers pentru chunk/FBO/texture/lighting/physics/path;
- GC, heap peak și temperatură best-effort numai la marginile sesiunii;
- configurația și dovada de execuție OPT-LAB.

Self-testul și buildul Java reproducibil trebuie să treacă înainte de ambalare.
Verdictul de device se ia în primul rând din worst-window 1% low, p99/max și
hitch-uri în aceeași zonă aglomerată, apoi din average FPS.

