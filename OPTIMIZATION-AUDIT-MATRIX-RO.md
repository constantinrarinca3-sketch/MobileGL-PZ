# Audit complet al listei de optimizare — OPT-LAB V3

Lista primită la 2026-08-24 este checklist-ul autoritativ. `SHIPPED` înseamnă
cod real și selectabil; `EXISTING` înseamnă că sursa îl avea deja și nu este
rebranduit ca optimizare nouă; `GATED` înseamnă că telemetria trebuie să arate
un cost real pe tabletă; `BLOCKED` înseamnă că implementarea fără dovadă ar
încălca exact guardrail-urile listei.

## Date device care schimbă prioritatea V3

- V2 `ALL_SAFE_ON` nu a adus un câștig relevant în zona aglomerată; nu este
  promovat ca set stabil.
- Telemetria 006/009 a arătat ring UBO la aproximativ 9,9% high-water, fără
  pressure/wait/`glFinish` fallback; respecify buffer a fost aproximativ 0,048%.
- Worst-window-ul a rămas dominat de CPU (`FBO_CHUNK_RENDER` și
  `WORLD_STREAM_CHUNK` în benchmarkul jocului), cu readback rar de circa 43–45 ms
  și temperatură observată până la 105,8 °C.
- Din acest motiv V3 nu mărește ringul UBO, nu scoate fences și nu adaugă PBO
  orbește. Mai întâi izolează timpul nativ cu 018 și testează două reduceri
  transparente de lucru per draw: 019 și 020.

## Candidați V3 noi

| ID | Verdict V3 | Implementare / guardrail |
|---|---|---|
| 018 | SHIPPED DIAGNOSTIC | 1/64 draws, max. 2048; 13 etape + total; ceasurile și proof-ul sunt în afara rulărilor FPS |
| 019 | SHIPPED SAFE CANDIDATE | evită numai apeluri native `glVertexAttrib4fv/I4iv/I4uiv` identice; cheie exactă tip+biți+generație context; invalidare la make-current/destroy |
| 020 | SHIPPED SAFE CANDIDATE | sare peste probe per-textură numai după listă/pairing/scope verificat clean și epoch neschimbat; orice mutație relevantă reia verificarea |
| render-state group epochs | DEFERRED BY 018 | versiunea globală și delta sync există; se fragmentează doar dacă 018 dovedește cost relevant |
| draw-path registry cache suplimentar | AUDITED/ALREADY EXISTING | memo-urile owner-safe pentru program/VAO/FBO/VBO există; nu se adaugă cache duplicat |

## Baseline și candidați principali

| Candidat | Verdict curent | Implementare / motiv |
|---|---|---|
| identitate exactă baseline | SHIPPED | source ID, build ID, SHA-256 și două variante LTO în manifest |
| fără census/health în FPS | SHIPPED | toate trace-urile PZF/CP2/Tracy OFF; log level FATAL |
| ThinLTO | SHIPPED BUILD | ELF separat THINLTO; NOLTO este controlul compile-time |
| texture sync program-aware | SHIPPED 002 | mask sampler per program și fallback complet fără program |
| texture binding program-aware | SHIPPED 003 | compare/capture numai rândurile sample-uite |
| changed texture-unit dirty bitset | PARTIAL/EXISTING | binding epoch + sampling generation elimină steady-state; un bitset explicit pe mutații cere auditarea tuturor writerilor înainte de a fi sigur |
| cache/eliminare registry lookup | EXISTING | VAO/program twin memo și backend twin lângă resursele buffer; nu se dublează |
| dynamic buffer orphan/id swap | SHIPPED 010 | ID swap numai cu pensionare pe serial/fence; experimental |
| UBO high-water/ring telemetry | SHIPPED 006 | allocate/grow/pressure/high-water/wait/finish metrics |
| eliminare UBO `glFinish` | GATED | fallbackul este ultima plasă de siguranță la capătul de 64 MiB; se schimbă numai dacă 006 îl dovedește |
| GC incremental | SHIPPED 013 | buget 4 intrări la cadență 64, fază staggered per registry |
| real `glInvalidateFramebuffer` | SHIPPED 008 | forward exact al cererii aplicației, fără discard ghicit |
| cache program binary | SHIPPED 007 | atomic, LRU 64 MiB, cheie driver/capabilități/build/SPIR-V/state, fallback la source |
| cache ESSL transpilat | SHIPPED 016 | atomic, LRU 32 MiB, cache al sursei finale; XFB block-flattening rămâne fallback |
| query-uri `glGet*` rămase | PARTIAL | probele diagnostice sunt compile-out; 017 elimină query/bind redundant RBO; emulările rare își păstrează query-urile până au shadow complet demonstrat |
| used vertex attribute mask | SHIPPED 014 | VBO sync folosește active attribute mask; VAO frontend rămâne complet |
| compile-out logging | SHIPPED BUILD | DEBUG/INFO/WARN/ERROR nu există în hot path; FATAL păstrează proof-ul bounded |

## Candidați secundari

| Candidat | Verdict curent | Implementare / motiv |
|---|---|---|
| UBO circular/frame-mark queue | SHIPPED 015 | head index + compactare amortizată, versus erase/memmove baseline |
| buffer pool size classes | GATED | pool exact-size există; clasele pot crește memoria și trebuie alimentate de distribuția reală |
| adaptive buffer-pool budget | GATED | cere high-water/lifetime pe device |
| stagger GC | SHIPPED 013 | fază diferită per registry și work budget fix |
| safe auto-invalidate înainte de clear | BLOCKED SAFETY | 008 forwardează numai cereri reale; scissor/masks/coverage nu sunt ghicite |
| persistent PBO upload ring | GATED 009 | bytes/apeluri texture și client staging sunt măsurate înainte de alocarea ringului |
| PBO threshold | GATED 009 | se alege din distribuția reală de bytes, nu arbitrar |
| batch dirty rects în PBO | GATED 009 | depinde de verdictul PBO |
| Adreno dirty-rect calibration | DEVICE MATRIX | scatter/union există; 8/16/32/48 necesită tabletă |
| cost-aware dirty-rect merge | GATED | codul are heuristica de arie; costul apelului trebuie măsurat |
| texture ID swap | BLOCKED RISK | necesită lifetime/fence separat și cazuri immutable/compressed |
| cache SPIR-V intermediar | PARTIAL | RAM/dedup există; 016 cache-uiește produsul ESSL final, mai aproape de consumator |
| cache preprocessing mai mare | DEVICE MATRIX | hit/eviction trebuie măsurate |
| 2/3/4 async workers | EXISTING TUNABLE | `MOBILEGL_ASYNC_SHADER_COMPILE_THREADS`; nu este duplicat ca ID |
| allocation-free uniform lookup | AUDITED, NOT SHIPPED | risc mic dar valoare necunoscută față de draw/stutter; necesită profil de loading |
| sampler/uniform intern pool | AUDITED, NOT SHIPPED | aceeași condiție |
| renderbuffer binding shadow | SHIPPED 017 | toate bindurile runtime trec prin shadow; delete/context/probe îl invalidează |
| read/draw-buffer shadows | EXISTING | BackendFramebufferObject păstrează și sincronizează ambele |
| scratch resource pools | PARTIAL/EXISTING | ScratchFBO și resurse interne persistente există; nu există pool generic ghicit |
| program resource masks | SHIPPED 002/011/012/014 | sampler, image, SSBO și vertex attribute |
| composite draw memo | PARTIAL/EXISTING | memo-uri separate, cu chei exacte, evită un stamp monolitic fragil |
| generation/dirty în loc de memcmp | PARTIAL/EXISTING | buffer/texture/program/FBO au epochs/versions; render state mai folosește trei span memcmp |
| incremental render-state sync | EXISTING | version gate + compare per capability/field; emite numai delta |
| frame-fence ring 6–8 | DEVICE MATRIX | numai dacă 006 arată wrap/reuse pressure |
| pre-reserve registries | GATED | necesită high-water counts; flat maps reduc deja rehash/cache cost |
| flat/open-addressing maps | EXISTING | `ska::flat_hash_map` este tipul `UnorderedMap` |
| split hot/cold fields | AUDITED, NOT SHIPPED | cere profil/cache-miss data, altfel doar churn structural |
| PGO | BLOCKED INPUT | nu există încă profil reprezentativ de pe tabletă |
| profile-guided function ordering | BLOCKED INPUT | aceeași dependență de profil |

## Experimental / fidelity / vendor

| Candidat | Verdict curent | Motiv |
|---|---|---|
| render-pass/FBO tracker | NOT IN FPS BUILD | monitorizarea ar contamina exact benchmarkul; 008 are counter bounded |
| QCOM tiled rendering | BLOCKED RISK | preserve mask greșit corupe frame-ul; cere profil separat și validare vizuală |
| multisampled-render-to-texture | GATED | întâi trebuie dovedit că workloadul folosește MSAA/resolve relevant |
| resolve elimination | GATED | dependent de candidatul anterior |
| QCOM shading rate | BLOCKED FIDELITY | poate schimba imaginea; niciodată default transparent |
| Adreno precision specialization | BLOCKED FIDELITY | cere offline compiler + validare numerică/vizuală |
| shader variants pe uniforme constante | GATED/HIGH COMPLEXITY | cere stabilitatea valorilor și control strict al exploziei cache-ului |
| device-specific `-mcpu` | BLOCKED INPUT | microarhitectura exactă nu este încă probată; buildul rămâne arm64 portabil |
| global `-ffast-math` | REJECTED | schimbă semantica; nu este inclus |
| eliminare fences | REJECTED | 010 și ringurile păstrează lifetime GPU |

Concluzie: fiecare variantă din listă a fost clasificată și recitită pentru V3.
Nu toate sunt pornite
orbește în build: asta ar face pachetul imposibil de atribuit și ar încălca
guardrail-urile. Toate pistele sigure implementate sunt combinabile, iar cele
care au nevoie de dovezi au telemetrie sau un motiv explicit de blocare.
