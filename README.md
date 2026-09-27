# Capitol Defense

A compact, flash-style tower defense game with a 21st-century US politics theme. It's one `index.html` file with no build step. Open it in a browser to play.

There is also a native desktop version in C++ with raylib, with gamepad support. See [`cpp/README.md`](cpp/README.md).

**Goal:** keep your approval rating above 0% for 15 waves while mobs march down Pennsylvania Ave toward the Capitol.

## Defenses
| Tower | Cost | Role |
|---|---|---|
| Fact-Checker | $50 | Fast single-target shots |
| Filibuster | $70 | Area pulse that slows everything nearby |
| Attack Ad | $100 | Splash damage |
| Supreme Court | $160 | Long range, heavy hit on the toughest target |

Each tower can be upgraded twice or sold for 60% of what you spent on it.

## Mobs
Robocalls (fast), Lobbyists, Troll Bot swarms, Super PACs (tanky), Scandals (mini-boss on waves 5, 10 and 14) and the Government Shutdown (final boss).

## Controls
Click a tower button (or press 1–4), then click grass to build. Shift-click places several in a row. Click a tower to upgrade or sell it. Space starts the next wave, P pauses, Esc cancels.
