# GGFW RF15 - RF13 stable base + 64-char text + draft persistence

Base: RF13 TX long preamble stable RF build.

Changes:
- Kept RF13 RF/voice behavior; no RF14 warm-up frame.
- Increased common message text limit to 64 characters via MSG_TEXT_LEN.
- Updated native packet wire length from 78 to 94 bytes to carry 64-byte payload.
- Updated Messenger RF Aircopy-style frame from 36 words/72 bytes to 50 words/100 bytes.
- Messenger TX still uses 16-byte hardware preamble from RF13.
- Compose, Reply compose, and Draft edit all use the same 64-character T9 limit.
- Draft editing now commits with MENU and persists using MSG_STORE_SaveConfig().
- Editing an existing draft or an empty draft slot returns to the Drafts list after save.

Important test items:
1. Voice RX/TX remains normal before/after Messenger and after message TX.
2. Short RF message success rate remains near RF13.
3. 64-character messages send/receive without truncation.
4. Compose/Draft/Reply editors do not allow more than 64 chars.
5. Draft edits persist after exiting Messenger and after power cycle.
