# Copyright (c) 2025 Tearnote (Hubert Maraszek)
#
# Licensed under the Mozilla Public License Version 2.0 <LICENSE-MPL-2.0.txt
# or https://www.mozilla.org/en-US/MPL/2.0/> or the Boost Software License <LICENSE-BSL-1.0.txt
# or https://www.boost.org/LICENSE_1_0.txt>, at your option. This file may not be copied, modified,
# or distributed except according to those terms.

import sys

from fontTools.ttLib import TTFont
from fontTools.ttLib.removeOverlaps import removeOverlaps


def main():
    if len(sys.argv) != 3:
        print("Usage: python process_font.py <input_ttf> <output_ttf>")
        sys.exit(1)

    input_path = sys.argv[1]
    output_path = sys.argv[2]

    try:
        print(f"-- Loading font: {input_path}")
        font = TTFont(input_path)
        print("-- Removing overlaps (this may take a moment)...")
        removeOverlaps(font)
        print(f"-- Saving to: {output_path}")
        font.save(output_path)
        print("-- Done.")

    except Exception as e:
        print(f"Error processing font: {e}")
        sys.exit(1)


if __name__ == "__main__":
    main()
