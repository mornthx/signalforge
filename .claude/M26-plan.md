# M26 — DR-001 IA rewrite (plan)

Local commits on `milestone/M26` (no push). Visual states rebaselined at S3 (chrome change).

## S1 — menu-bar rewrite (#4)  [done]
Owned `buildMenuBar()` (File | Connections | Session | View | Help); feature modules register
actions into the stored menus; Record + Open Session under Session; Help → About.

## S2 — status-bar cleanup (#5)
Owned `buildStatusBar()` building all cells in a deliberate order; concise values (drop the
word already in the cell title); no idle false-warnings.

## S3 — rebaseline + close-out
Full ctest; accept the chrome-change baselines; `M26-done.md`; mark DR-001 #4/#5 resolved.
