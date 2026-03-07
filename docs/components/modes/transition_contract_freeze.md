# Reaper-OS Transition Contract Freeze

This document is the canonical transition contract for current implementation.

## 1. Transition Sources

- `TRANSITION_SOURCE_USER`: manual request path.
- `TRANSITION_SOURCE_DAEMON`: system-prompt/orchestrated request path.
- `TRANSITION_SOURCE_KERNEL`: automatic/internal kernel path.

## 2. Auth Flags (`GATE_OP_MODE_TRANSITION a1`)

- `MODE_AUTH_PASSWORD`
- `MODE_AUTH_SPECIAL_KEY`
- `MODE_AUTH_SYSTEM_PROMPT`
- `MODE_AUTH_COOLDOWN_ELAPSED`
- `MODE_AUTH_DEESC_ELAPSED`
- `MODE_AUTH_MANUAL`
- `MODE_AUTH_AUTOMATIC` (kernel/internal use)
- `MODE_AUTH_BOOT_INTENT` (boot-restore use)

## 3. Legal Matrix

| From \ To | Casual | Secure | Lockdown | Ghost |
| :--- | :---: | :---: | :---: | :---: |
| **Casual** | — | USER/KERNEL | USER/DAEMON | USER |
| **Secure** | USER/DAEMON | — | USER/DAEMON | USER/KERNEL |
| **Lockdown** | ⛔ | USER/DAEMON | — | ⛔ |
| **Ghost** | ⛔ | USER | KERNEL/DAEMON | — |

Notes:
- `MODE_VOID -> MODE_CASUAL` is the default bootstrap edge.
- boot-restore allows `MODE_VOID -> MODE_SECURE|MODE_LOCKDOWN` only with `MODE_AUTH_BOOT_INTENT`.

## 4. Per-Edge Guards

- `CASUAL -> LOCKDOWN`: requires password auth; daemon path also requires `MODE_AUTH_SYSTEM_PROMPT`.
- `CASUAL -> GHOST`: requires password auth.
- `SECURE -> CASUAL`: requires password auth and cooldown (or explicit `MODE_AUTH_COOLDOWN_ELAPSED`).
- `SECURE -> LOCKDOWN`: requires password auth.
- `LOCKDOWN -> SECURE`:
  - USER path: requires `MODE_AUTH_SPECIAL_KEY`.
  - DAEMON path: requires de-escalation window (or `MODE_AUTH_DEESC_ELAPSED`) and prompt+password auth.
- `GHOST -> SECURE`: requires password auth.

## 5. Reject Reason Codes

Reject reason codes are recorded in Fate transition records via `fault_error_code`:

- `MODE_REJECT_TARGET_INVALID`
- `MODE_REJECT_SOURCE_INVALID`
- `MODE_REJECT_EDGE_ILLEGAL`
- `MODE_REJECT_AUTH_REQUIRED`
- `MODE_REJECT_SPECIAL_KEY_REQUIRED`
- `MODE_REJECT_SYSTEM_PROMPT_REQUIRED`
- `MODE_REJECT_COOLDOWN_ACTIVE`
- `MODE_REJECT_DEESC_WINDOW_ACTIVE`
- `MODE_REJECT_BOOT_POLICY`
- `MODE_REJECT_AUTH_RETRY_LIMIT`

## 6. Boot Restore Contract (Current Implementation)

- Kernel keeps an integrity-checked boot restore record (magic + checksum + sequence)
  persisted in CMOS NVRAM bytes `0x40..0x47`.
- Restorable modes: `CASUAL`, `SECURE`, `LOCKDOWN`.
- `GHOST` is not restorable.
- On accepted transition into restorable modes, boot record is updated.
- If daemon-prompted `CASUAL -> LOCKDOWN` auth fails repeatedly, kernel sets lock-down boot intent marker.

## 7. Runtime Evidence

- Envelope markers emit compile/verify/apply/attest stages.
- Fate ledger includes accepted/rejected transitions and reject reason codes.
- Matrix gate validates presence of rejected evidence and reject-reason marker.
