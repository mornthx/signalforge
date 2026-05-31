# Deferred configurable options (v0.4 UI)

This file is a registry of UI behaviours that **could** become user-facing
settings but are intentionally shipped with a single sensible default and **no
configuration surface yet**. Listing one here means: a fixed default is in code,
the toggle is *not* built, and we will only invest in the setting (and its
persistence + UI) when there is real demand.

Add an entry when you make a behavioural choice that a user might reasonably want
to flip — so the decision is recorded and the future option is discoverable —
without expanding scope now.

| Option | Shipped default | If/when made configurable |
| --- | --- | --- |
| **Jump to Dashboard on add card** | **Off** — adding a card from Parsed (row menu or inspector) stays on the current tier, so several card types can be added in a row and the user keeps their place. | A per-session toggle ("After adding a card, switch to Dashboard"). Persist in app settings; default stays off. |

## Notes

- These are **not** the same as the architecture's frozen interfaces or the
  milestone specs. They are product-behaviour defaults, owner-settable later.
- Keep each default's rationale in the table so a future implementer knows why
  the default was chosen before adding the toggle.
