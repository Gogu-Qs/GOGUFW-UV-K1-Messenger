All notable changes to GOGUFW will be documented in this file.

⸻

v0.3.2

Fixed

* Fixed ACK retry timeout overflow bug
* s_wait_ack_ticks converted from uint8_t to uint16_t
* Correct 4-second ACK timeout now works properly
* Improved ACK reliability in delayed ACK environments

RF

* Improved retry timing stability
* Reduced delayed ACK / retry collisions

UI

* Improved Messenger HOME icon alignment
* Improved floppy disk icon proportions and shape
* Better SELECT footer spacing

⸻

v0.3.1

RF

* Increased ACK timeout window
* Improved retry scheduling timing
* Reduced retry collisions in multi-radio use

UI

* Reworked floppy disk Drafts icon
* Improved HOME screen icon positioning

⸻

v0.3.0

Messenger HOME UI

* Added Messenger HOME icons:
    * Inbox envelope
    * Compose pencil
    * Sent upload arrow
    * Draft floppy disk
* Added SELECT footer
* Improved list spacing
* Improved title alignment
* Improved icon positioning

Compose UI

* Added character counter
* Added dashed separators
* Improved metadata layout

⸻

v0.2.9

UI

* Improved icon alignment
* Improved Messenger HOME spacing
* Improved footer spacing
* Improved Compose screen layout

⸻

v0.2.8

Messenger HOME

* Added initial icon support
* Added footer separator
* Added SELECT footer text

⸻

v0.2.7

Messenger UI

* Improved Inbox/Sent compact metadata layout
* Added small-font hop display
* Added compact TO/FROM layout
* Added Reply / Resend / Delete footer actions

Compose

* Added SEND footer
* Improved B/b/2 placement

⸻

v0.2.6

RF

* Added randomized ACK delay
* Reduced simultaneous ACK collisions

⸻

v0.2.5

RF

* Improved ACK / Retry system
* Improved delayed retry behavior

UI

* Improved metadata rendering
* Improved message spacing

⸻

v0.2.x

Messenger

* Added ACK support
* Added retry support
* Added drafts
* Added T9 compose
* Added Inbox/Sent/Drafts screens
* Added unread notifications
* Added persistent Messenger settings

⸻

Early Development

* Initial Messenger implementation
* Initial FSK RF experiments
* Background RF receive work
* Boot-time RF initialization
* Voice-safe RF integration
* Initial packet format
* Initial hop support groundwork
