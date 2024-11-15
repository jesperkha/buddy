# Garbage script to automatically create documentation from header file

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
    with open(args[0], "r") as f:
        s = get_formatted(f)
    with open(OUTPUT_FILE, "w") as f:
        f.write(f"# Buddy documentation\n")
        f.write(s)

def get_formatted(f: TextIO):
    doc = "" 
    docline = ""
    file_s = f.read()
    f.seek(0)
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
            line = get_function_implementation(file_s, stripped)
            source = f"[source](https://github.com/jesperkha/buddy/blob/main/buddy.h#L{line})"
            docline = f"\n```c\n{stripped}\n```\n\n{docline} {source}\n"
            doc += docline
            docline = ""
    
    return doc

def get_function_implementation(f, term):
    occurrence_count = 0
    lines = f.splitlines()

    for i, line in enumerate(lines, start=1):  # Iterate over lines with line numbers
        if line.startswith(term):
            occurrence_count += 1
            if occurrence_count == 2:
                return i  # Return the line number of the second occurrence

    error_exit(f"implementation not found for {term}")

if __name__ == "__main__":
    main(sys.argv[1:])