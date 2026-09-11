# Public Unicode fixture provenance

`unicode_format_17.inc` enumerates the code points with General_Category `Cf`
from [UnicodeData 17.0.0](https://www.unicode.org/Public/17.0.0/ucd/UnicodeData.txt).
It checks the production request parser's compact rejection table independently.

Source SHA-256: `2e1efc1dcb59c575eedf5ccae60f95229f706ee6d031835247d843c11d96470c`.
Generated with `awk -F';' '$3 == "Cf" {printf "0x%s,\n", $1}' UnicodeData.txt`.
The source is Unicode data; see the [Unicode data license](https://www.unicode.org/license.txt).

This table rejects format controls in untrusted payment labels/messages. It
does not detect homoglyphs, all invisible glyphs, or general visual spoofing.
The receiving address must always be rendered independently from sender labels.
