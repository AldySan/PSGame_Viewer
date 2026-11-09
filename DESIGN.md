# Design direction

This file is the single source of truth for how PSGame-Viewer looks and
behaves visually. When a change touches color, spacing, type, motion or a
new surface, check it against this document first. The audit that produced
the current palette and interaction rules lives in the repository history
(fix/theme-contrast, fix/keyboard-access).

## Identity

PSGame-Viewer is a technical viewer: it answers "what is inside this
game file?" for collectors and archivists. The design goal is **comfort
and ease** — a calm, neutral workspace where the game's own cover art is
the loudest element on screen. The application chrome stays quiet so the
content can speak.

One-line personality: *a clean, professional tool with a warm, calm
hand; the data is the decoration.*

Dials:

- **ENERGY 1** — flat surfaces, hairline structure, no decorative noise
- **RHYTHM 1-2** — one spacing scale, sections rarely vary; the tool is
  a single-purpose workspace, not a marketing page
- **MOTION 1** — hover states and short affordance feedback only; no
  loops, no entrance animation

## Palette

Neutrals plus a single blue accent, plus two semantic colors. Two tokens
never carry meaning alone when text depends on them. All values live in
`src/qml/Theme.qml`; nothing outside that file may hardcode a UI color.

| Token | Light | Dark | Role |
| --- | --- | --- | --- |
| `bg` | `#f4f5f7` | `#171717` | page background |
| `card` | `#ffffff` | `#202020` | panels, dialogs, menus |
| `card2` | `#eceef2` | `#2a2a2a` | inset surfaces: chips, preview boxes |
| `inputBg` | `#ffffff` | `#1b1b1b` | text fields, combos |
| `border` | `#82868e` | `#767676` | component boundaries, 3:1 minimum |
| `text` | `#1d2330` | `#f1f3f8` | primary text |
| `muted` | `#5c6575` | `#8b93a5` | secondary text |
| `accent` | `#2f6fd0` | `#4f8ef7` | the one accent; fills of primary buttons, focus rings, selection borders |
| `accentSoft` | `#598cd9` | `#3b6bb9` | selection/hover fill; text on it stays `text` |
| `accentText` | `#ffffff` | `#0e1116` | text on accent fills |
| `danger` | `#c0392b` | `#e26055` | errors, failed rows |
| `good` | `#27863a` | `#5bbf6b` | success states, "OFC" badge |

Contrast is a design commitment, not a review checklist item:

- body text and control labels clear **4.5:1** on every surface they sit
  on, in both themes
- component boundaries (inputs, cards, chips) clear **3:1** against the
  surfaces they touch (WCAG 1.4.11), so low-vision users can find inputs
  that are not focused
- when a token changes, re-run the contrast matrix over the affected
  pairs; the values above are the current verified set

Accent discipline: the accent marks the primary action of a view, the
selected state and the keyboard focus. It does not decorate content
rows, icons or labels.

## Typography

The UI font is the **platform's native UI face**: "Segoe UI" on Windows,
the system default everywhere else (no bundled font; a viewer that
people carry on USB sticks should not grow megabytes). Hierarchy comes
from weight and size, never from color or extra fonts:

- 13 px regular — body text, list rows
- 12 px regular/demi-bold — labels, secondary rows, column headers
- 16-20 px bold — dialog titles, page titles
- mono is reserved for values that are identifiers (Title IDs, sizes),
  rendered by the data itself, not styled as a design device

## Shape, spacing, elevation

- corner radii: 6 (chips, small marks), 8 (controls, cards), 12
  (dialogs) — from `Theme.radiusSm`/`radius`/`radiusLg`; no other radii
- one spacing scale: 4/8/12/16 with 10 and 14 as the workhorse paddings
- elevation is flat: surfaces are separated by their border, never by
  shadows; there is exactly one floating element (the toast)

## Motion

- press feedback: 90 ms scale-down on buttons
- state transitions (checkbox fill, toast fade): 120-180 ms
- the toast lives 3.5 s, then fades on its own
- keyboard focus rings appear and disappear without animation
- nothing pulses, loops or floats; the only animations respond to direct
  user input

## Interaction honesty

Every gesture the app accepts is visible somewhere: copyable values
carry a "Click to copy" tooltip, extractable files carry "Double-click
to extract", and every mouse-only shortcut has a keyboard equivalent
(lists take Return; chips and thumbnails are real buttons). Hover
highlight is reserved for rows that actually do something on click.
Empty, loading and error states say why they are empty and what to do
next; destructive actions confirm before they act.
