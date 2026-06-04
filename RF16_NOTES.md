# GGFW RF16 draft save+send

Base: RF15 36-char final / RF13-RF12 RF base.

Changes:
- Draft edit screen MENU now saves the selected draft persistently and immediately sends the edited text.
- Compose / reply behavior unchanged.
- 36-character limit unchanged.
- Inbox 20 / Outbox 10 / Drafts 8 unchanged.
- RF / preamble / boot-prime / global beep logic unchanged.

Test:
1. Open Messenger -> Drafts.
2. Select a draft with MENU.
3. Edit text.
4. Press MENU: draft should save and transmit immediately.
5. Power cycle: edited draft should still be stored.
