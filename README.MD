generic 16×2 HD44780-compatible character LCD with a PCF8574 I²C backpack
“1602” / “LCD1602” style display

16 columns × 2 rows of text characters

Each character is a 5×8 dot matrix (plus optional underline).
ype:

“1602” / “LCD1602” style display

16 columns × 2 rows of text characters

Each character is a 5×8 dot matrix (plus optional underline).

Controller:

An HD44780-compatible controller (often labeled HD44780, ST7066U, KS0066, etc.).

It expects a 4-bit or 8-bit parallel interface: RS, RW, E, D4–D7, etc.

The backpack hides that parallel interface from you and converts it to I²C.

Electrical (glass side):

Logic & LCD power: 5 V (typical)

Backlight: 5 V LED, usually controlled via a transistor and a series resistor

Contrast: set by the small blue potentiometer on the backpack

Turn it to go from all-white (no visible chars) → black blocks → nice readable characters.

I²C details:

Address:

In your case: 0x27 (confirmed by your scanner).

Others sometimes ship as 0x3F depending on A0–A2 solder pads.

Bus: I²C (2-wire)

SDA (data)

SCL (clock)

Pull-up resistors (often 4.7 kΩ) are usually on the backpack, often to 5 V.

Speed:

Typically used at 100 kHz or 400 kHz; both are fine for this LCD.

4-pin header (the one you wired to the TTGO):


4×4 membrane matrix keypad.
A thin, flexible keypad with 16 buttons arranged in a 4-row × 4-column grid:

Rows: 1, 2, 3, A

Columns: 4, 5, 6, B

And so on…
