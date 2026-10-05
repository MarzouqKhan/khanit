# khanit

A command-line tool that translates systemd `.service` files into [dinit](https://github.com/davmac314/dinit) service files.

It parses actual `.service` files, maps systemd directives onto their dinit equivalents where (a reasonable) one exists, and flags anything it can't safely translate.

## Why

dinit is a small, dependency-aware init system and service supervisor and an alternative to systemd used by distros like Chimera Linux and (optionally) Artix. Projects like [elogind](https://github.com/elogind/elogind) and [turnstile](https://github.com/chimera-linux/turnstile) already let dinit-based systems run most systemd-dependent software. This tool handles a different piece of that puzzle: converting the actual `.service` files themselves.

## What it does

Given a `.service` file, it reads the `[Service]` section and produces a file with the equivalent dinit service properties. Directives with no dinit equivalent are left out and flagged with a comment explaining why, rather than silently dropped or guessed at.

Currently handled:
- Direct key mappings for ~48 systemd directives (`ExecStart`, `After`, `Requires`, etc.); see the mapping table in `khanit.c`
- `Type=` is translated to dinit's `process`/`bgprocess`/`scripted` types, with signal-aware handling of `notify` and `dbus` (which have no clean dinit equivalent)
- `Restart=` is validated against dinit's actual supported values, with `always` approximated as `yes` and unsupported systemd-only values (`on-watchdog`, `on-success`, etc.) flagged rather than passed through
- `KillSignal=` is validated against dinit's actual supported signal set and stripped of the `SIG` prefix
- `TimeoutSec=` / `TimeoutStartSec=` / `TimeoutStopSec=` supports full systemd time-span parsing (`1h30min`, `500ms`, etc.) converted to the plain seconds dinit expects
- Whitespace tolerance (`Type = simple` parses the same as `Type=simple`, per the systemd syntax spec)

## Remaining issues

- `Environment=` needs to generate a separate env-file (dinit has no inline equivalent); not yet implemented
- Line continuation (`\` at end of line) long wrapped directives currently won't parse correctly
- No distinction yet between "known directive, no dinit equivalent" and "directive not recognized at all"; both currently produce the same warning
- `[Unit]`/`[Install]` sections aren't parsed at all; only `[Service]`
- No handling for `Condition*=`/`Assert*=` directives beyond flagging them as unsupported (dinit has no equivalent mechanism)

## Usage

```
gcc khanit.c -o khanit
./khanit some.service
```

Produces `some` in the same directory.

## Status

Actively under development. Not yet recommended for production use.
