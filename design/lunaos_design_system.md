# LunaOS Design System

This document defines the LunaOS desktop design system against the current
repository implementation.

It is not a generic UI style guide. It is a system contract for the current
`Luna Session` desktop path implemented across `USER`, `GRAPHICS`, `PROGRAM`,
and `PACKAGE`.

The design target is:

- modern desktop operating system behavior
- restrained, clean, high-trust visual language
- low ornament, low gradient usage
- strong usability and hierarchy
- dark mode first

The naming in this document follows the official LunaOS registry:

- desktop session: `Luna Session`
- desktop environment: `Luna Desktop`
- compositor: `Luna Scene`
- top bar: `skybar`
- launcher surface: `orbit launcher`
- task rail: `dockline`
- notification family: `signal tray`
- window family: `Luna Window`

## 1. Scope and Runtime Fit

The current implementation does not yet have a full token-based theme engine.
The live shell state passed from `USER` to `GRAPHICS` currently exposes only:

- `desktop_attr`
- `chrome_attr`
- `panel_attr`
- `accent_attr`

Those four fields are the current protocol-level theme bridge.

This design system therefore defines two layers:

1. `Protocol tokens`
   These map to the current `luna_desktop_shell_state` fields and can be used
   immediately.
2. `Visual tokens`
   These define the intended LunaOS UI language and should be carried forward
   into a future `Luna Theme Contract`.

## 2. Design Principles

### Task Before Ornament

The shell exists to help the user launch, switch, focus, read, edit, and
recover work. Decorative treatment must never reduce clarity.

### System Before App

Shell chrome, app chrome, and first-party apps must all look like clients of
one operating system.

### Stable Spatial Model

Each major surface has one job:

- `skybar`: status and session-level actions
- `dockline`: app launch and active-task presence
- `orbit launcher`: app discovery and launch
- `Luna Window`: work surface
- `signal tray`: transient attention and alerts

### Calm Hierarchy

Hierarchy should be created through:

- contrast
- spacing
- elevation
- restrained accent usage

Not through loud gradients, oversized text, or decorative motion.

## 3. Color System

### 3.1 Protocol Tokens

These tokens can be mapped onto the current shell state fields now.

- `desktop_attr`
  Maps to desktop background and large workspace field.
- `chrome_attr`
  Maps to active chrome, top-level bars, and focused shell framing.
- `panel_attr`
  Maps to floating panels, launcher surfaces, menus, sidebars, and secondary
  surfaces.
- `accent_attr`
  Maps to focused state, selected state, primary action state, and active item
  indication.

### 3.2 Visual Tokens: Moonglass Dark

Default dark theme family: `moonglass-dark`

```txt
Background
--bg-base: #0E1116
--bg-surface: #141922
--bg-elevated: #1A202B
--bg-overlay: rgba(9, 11, 15, 0.72)

Panel
--panel-default: rgba(22, 27, 35, 0.88)
--panel-strong: rgba(28, 34, 44, 0.94)
--panel-hover: #252C38

Border
--border-subtle: rgba(255, 255, 255, 0.06)
--border-default: rgba(255, 255, 255, 0.10)
--border-strong: rgba(255, 255, 255, 0.16)

Text
--text-primary: rgba(255, 255, 255, 0.92)
--text-secondary: rgba(255, 255, 255, 0.68)
--text-tertiary: rgba(255, 255, 255, 0.46)
--text-disabled: rgba(255, 255, 255, 0.28)

Accent
--accent-500: #5E9BFF
--accent-400: #7AB0FF
--accent-600: #3E7FE7
--accent-soft: rgba(94, 155, 255, 0.18)

Danger
--danger-500: #E36A72
--danger-400: #F1878F
--danger-600: #C94D58
--danger-soft: rgba(227, 106, 114, 0.16)
```

### 3.3 Color Usage Rules

- Use neutral backgrounds by default.
- Use accent color only for active, focused, selected, or primary action state.
- Use danger color only for destructive or error state.
- Do not use accent as a generic fill color for large surfaces.
- Do not rely on saturated gradients to create hierarchy.

### 3.4 Current LunaOS Mapping

The current implementation should interpret visual tokens like this:

- `desktop_attr` -> `--bg-base`
- `chrome_attr` -> `--bg-elevated`
- `panel_attr` -> `--panel-default`
- `accent_attr` -> `--accent-500`

When running in text-mode fallback or attr-only rendering, the system should
preserve:

- background separation
- focused vs inactive contrast
- panel vs workspace distinction
- active selection visibility

## 4. Spacing System

The LunaOS spacing scale is fixed:

```txt
--space-1: 4px
--space-2: 8px
--space-3: 12px
--space-4: 16px
--space-5: 24px
--space-6: 32px
```

Usage guidance:

- `4px`: icon-to-label offsets, compact gaps, inline grouping
- `8px`: button padding, input padding, menu row spacing
- `12px`: compact block spacing, menu insets, grouped controls
- `16px`: default content padding inside windows and panels
- `24px`: section spacing, sidebar grouping, notification padding
- `32px`: large section separation, settings pages, first-run surfaces

Rules:

- A single component should usually use no more than two spacing steps.
- System shell surfaces should prefer `8`, `12`, and `16`.
- Settings and content-heavy apps should prefer `16`, `24`, and `32`.

## 5. Shape, Stroke, Shadow, and Blur

### 5.1 Radius

```txt
--radius-sm: 8px
--radius-md: 12px
--radius-lg: 16px
--radius-xl: 20px
```

Recommended usage:

- buttons and inputs: `8px`
- menus, side panels, notifications: `12px`
- windows: `16px`
- dock containers and large shell capsules: `20px`

### 5.2 Shadow

```txt
--shadow-sm: 0 2px 8px rgba(0, 0, 0, 0.20)
--shadow-md: 0 8px 24px rgba(0, 0, 0, 0.28)
--shadow-lg: 0 16px 40px rgba(0, 0, 0, 0.36)
```

Usage:

- hover elevation: `shadow-sm`
- launcher, menu, notification: `shadow-md`
- active window, modal shell surface: `shadow-lg`

### 5.3 Blur

```txt
--blur-panel: 16px
--blur-overlay: 24px
```

Usage:

- `skybar`
- `dockline`
- `orbit launcher`
- system overlays

Rules:

- Blur is a layering tool, not decoration.
- Blur must be paired with a subtle stroke.
- App content panes should not depend on blur for readability.

### 5.4 Stroke

- default stroke thickness: `1px`
- default stroke color: `--border-default`
- subtle stroke color: `--border-subtle`
- focused stroke color: `--accent-500`
- destructive stroke color: `--danger-500`

Rules:

- Avoid combining strong stroke, strong blur, and strong shadow on one surface.
- Active state should usually prefer accent stroke before accent fill.

### 5.5 Fallback Guidance

When the shell is operating in low-fidelity or text-oriented rendering:

- radius degrades to clear surface separation
- shadow degrades to contrast layering
- blur degrades to opacity and border distinction

The hierarchy must survive without pixel-perfect effects.

## 6. Typography

LunaOS should prioritize legibility and consistent system behavior over visual
branding in the first desktop line.

Recommended font family stack:

```txt
"Segoe UI", "SF Pro Text", "PingFang SC", "Noto Sans SC", sans-serif
```

### Text Roles

```txt
Window Title
14px / 20px / 600

Body
13px / 20px / 400

Supporting Text
12px / 18px / 400

Menu Text
13px / 18px / 500
```

Rules:

- Window titles should be stronger, not dramatically larger.
- Secondary hierarchy should prefer lower contrast before smaller size.
- Menu text must remain easily scannable at speed.
- Avoid web-style display headlines in system shell surfaces.

## 7. Motion

LunaOS motion should feel quiet, precise, and system-like.

### Timing

```txt
--motion-fast: 120ms
--motion-base: 180ms
--motion-slow: 240ms
```

### Easing

```txt
--ease-standard: cubic-bezier(0.2, 0, 0, 1)
--ease-emphasized: cubic-bezier(0.2, 0.8, 0.2, 1)
```

### State Rules

- hover:
  `120ms`, background/border/opacity only
- focus:
  `120ms`, accent ring or contrast shift
- open:
  `180ms`, fade with very small scale or lift
- close:
  `140ms`, fade with slight compression
- window transition:
  `220ms` to `240ms`, focus on clarity over flourish

Rules:

- No exaggerated spring effects.
- No large bounce, zoom, or fish-eye behavior.
- Motion must reinforce state change, not decorate it.

### Current Implementation Note

The current repository does not yet implement full motion primitives in
`GRAPHICS`. Until that changes, the minimum requirement is immediate but clear
state feedback for:

- hover
- focus
- active window
- minimized window
- maximized window
- launcher open and closed state

## 8. Component Specifications

### 8.1 `skybar`

Role:

- session status
- top-level system identity
- lightweight shell actions

Spec:

- height: `40px`
- background: `--panel-default`
- stroke: bottom `1px --border-subtle`
- optional blur: `--blur-panel`
- text role: menu text

Rules:

- Keep density moderate.
- Do not overload it with app-local controls.
- Status indicators should be glanceable, not dominant.

### 8.2 `dockline`

Role:

- pinned apps
- active app presence
- relaunch and restore access

Spec:

- height: `64px` to `72px`
- background: `--panel-default`
- shape: `--radius-xl`
- spacing: `8px` outer, `8px` to `12px` inner
- active state: accent indicator or accent-soft backing

Rules:

- Active apps should be discoverable at a glance.
- Hover may lightly elevate or slightly enlarge icons.
- Avoid novelty magnification behavior.

### 8.3 `Luna Window`

Role:

- primary work surface

Spec:

- background: `--panel-strong`
- radius: `--radius-lg`
- stroke: `1px --border-default`
- inactive shadow: `--shadow-md`
- active shadow: `--shadow-lg`
- title bar height: `40px` to `44px`
- content padding: `16px` default

Rules:

- Active and inactive windows must remain clearly distinguishable.
- Title bar actions must be stable in placement and size.
- Window movement and resize affordances must be obvious.

### 8.4 `Sidebar`

Role:

- scoped navigation
- grouped tools or folders

Spec:

- width: `220px` to `280px`
- background: `--panel-default`
- grouped spacing: `24px`
- item height: `32px` to `36px`
- selected state: `--accent-soft`

Rules:

- Do not use highly saturated fills for selection.
- Keep navigation visually quieter than document content.

### 8.5 `Menu`

Role:

- commands
- context actions
- launcher listing

Spec:

- background: `--panel-default`
- radius: `--radius-md`
- stroke: `1px --border-default`
- shadow: `--shadow-md`
- padding: `8px`
- item height: `32px`

Rules:

- Hover uses `--panel-hover`
- Focus/selected uses `--accent-soft`
- Danger actions use danger text and danger-soft hover

### 8.6 `Button`

Variants:

- primary
- secondary
- ghost
- danger

Spec:

- default height: `32px`
- compact height: `28px`
- large height: `36px`
- radius: `--radius-sm`

Rules:

- Primary uses accent as action emphasis.
- Secondary uses neutral surface plus stroke.
- Ghost is transparent until interaction.
- Disabled state must reduce meaning, not only opacity.

### 8.7 `Input`

Spec:

- height: `32px`
- radius: `--radius-sm`
- background: low-contrast surface
- stroke: `1px --border-default`
- focus: accent stroke and soft outer emphasis
- placeholder: `--text-tertiary`

Rules:

- Inputs should visually align with buttons.
- Error state uses danger stroke and supporting text.
- Inputs should not look more elevated than buttons.

### 8.8 `Notification` / `signal tray`

Role:

- transient attention
- task outcome feedback
- recovery and safety alerts

Spec:

- width: `320px` to `400px`
- background: `--panel-strong`
- radius: `--radius-md`
- shadow: `--shadow-lg`
- title role: window title
- body role: supporting text

Rules:

- Notifications must not overwhelm active work.
- Persistent alerts should move to a tray or center surface.
- Destructive or high-risk messages must visually separate from neutral notices.

## 9. Window Role Guidance

The current package and runtime model already expose `window_role`.

Role guidance:

- `DOCUMENT`
  Main work surface. Prioritize content area, clear title, standard window
  controls.
- `UTILITY`
  Smaller focused surface. Dense controls are acceptable but should still follow
  shell spacing and typography.
- `CONSOLE`
  Content-first, high-information surface. Keep chrome restrained and maximize
  contrast consistency.

This role distinction should affect:

- default size
- padding behavior
- titlebar density
- emphasis of supporting chrome

It should not create separate visual languages.

## 10. Current Component Mapping in Repository

The current repository already has partial implementations that should evolve
toward this system:

- `skybar`
  The current top row in `GRAPHICS`
- `dockline`
  The current bottom launch/task strip
- `orbit launcher`
  The current application panel opened from the dock/start region
- `Control`
  Current placeholder for a system panel and future control center
- `Luna Window`
  Current window chrome drawn by `GRAPHICS`
- package-driven desktop entries
  Current package metadata feeding desktop and launch state

These existing components should be refined rather than replaced with unrelated
new shell metaphors.

## 11. Next Contract Upgrade

The current attr-only theme bridge is too small for a full desktop design
system.

The next version of the protocol should evolve from:

- `desktop_attr`
- `chrome_attr`
- `panel_attr`
- `accent_attr`

Toward a proper `Luna Theme Contract` carrying at least:

- background base
- background surface
- panel default
- panel hover
- border default
- text primary
- text secondary
- accent
- danger
- radius scale
- shadow strength

Compatibility rule:

- the old four attr fields should remain available as fallback while the shell
  and compositor are upgraded.

## 12. Non-Goals

This design system does not attempt to define:

- a marketing website visual language
- app-specific branding systems
- decorative animation systems
- bright, playful, or novelty-heavy themes as the default desktop identity

The system should feel advanced, quiet, and dependable before it feels
expressive.
