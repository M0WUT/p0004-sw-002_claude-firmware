# PROTOTYPE (throwaway): GTE front-panel navigation

Answers the wayfinder ticket "Navigating the LVGL interface" (#17). This is **not production code**. It is a browser simulation of the 320×170 TFT and the four Soft Keys, used to settle how the LVGL UI should behave. The winning design is recorded in the ticket's resolution. This branch is never merged.

- `gte-ui-prototype.html`: the final iteration (7), self-contained. Open it in a browser and use keys 1–4, or click the Soft Keys.
- `template.html` + `logos.json`: the same page with the logos split out. The logos were decoded from sw-001's `include/logo.h` (A8) and `include/gte_logo.h` (I1).
- Published copy: https://claude.ai/artifact/96ECG4R1MfEqGQyndRPjEg (private to the owner).

Iterations, as decided with the user:
1. Three models (page carousel, menu tree, Service tabs). The user chose the **page carousel**.
2. No status bar or clock; Warnings as 2 s popups; legend hides after 5 s, and the first press only wakes it; legend overlays the page.
3. Warnings and About became pages; Warnings stay conditions, with a Clear key.
4. Legend always visible except on the Application's own screens; warning temperature vs critical temperature (fatal).
5. Every Config Parameter is editable on the device (Keysight-style text editor), and Edit shows only on Config rows.
6. Next-screen key; ▲ ▼ scroll only on long pages; editable character list including lowercase and `-`; secrets shown as •••• and start blank.
7. Selection list ▲ ▼ Edit Back; editor holds 2 s (destructive confirmations 3 s); no colour difference between Config and Status rows.
