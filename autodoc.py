# Automatically create documentation from header file.
# Outputs to DOCS.md
# Run: python autodocs.py <file>

import sys
from typing import TextIO

OUTPUT_FILE = "DOCS.md"

def error_exit(msg: str) -> None:
    print(f"autodoc: {msg}")
    exit(1)

def main(args: list[str]) -> None:
    "Accepts cmd args excluding the program name"
    if len(args) != 1:
        error_exit("incorrect arg count, expects one: <file>")
    try:
        with open(args[0], "r") as f:
            s = get_formatted(f)
        with open(OUTPUT_FILE, "w") as f:
            f.write(f"# Buddy documentation\n")
            f.write(s)
    except:
        error_exit(f"file not found: {args[0]}")

def get_formatted(f: TextIO):
    doc = "" 
    docline = ""
    for line in f.readlines():
        if line.startswith("// MARK"):
            doc += "\n## " + line.removeprefix("// MARK: ")
            continue
        if line.startswith("// TODO"):
            continue
        if line.startswith("#ifdef BUDDY_IMPLEMENTATION"):
            return doc
        if line.startswith("//"):
            docline += line
        elif docline != "":
            docline = docline.replace("// ", "").replace("\n", " ")
            stripped = line.strip('\n').strip(";")
            source = f"([source](buddy.h#30))"
            docline = f"\n```c\n{stripped}\n```\n\n{docline} {source}\n"
            doc += docline
            docline = ""
    
    return doc

if __name__ == "__main__":
    main(sys.argv[1:])