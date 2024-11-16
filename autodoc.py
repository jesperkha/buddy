# Garbage script to automatically create documentation from header file

import sys
from typing import TextIO

OUTPUT_FILE = "DOCS.md"

def error_exit(msg: str) -> None:
    print(f"autodoc: {msg}")
    exit(1)

def to_kebab_case(value):
    return "-".join(value.lower().split())

def main(args: list[str]) -> None:
    "Accepts cmd args excluding the program name"
    if len(args) != 1:
        error_exit("incorrect arg count, expects one: <file>")
    with open(args[0], "r") as f:
        s = get_formatted(f)
    with open(OUTPUT_FILE, "w") as f:
        f.write(s)

def get_formatted(f: TextIO):
    doc = "" 
    docline = ""
    file_s = f.read()
    f.seek(0)
    titles = [] # list of tuple(name, num #)
    for line in f.readlines():
        if line.startswith("// TITLE"):
            title = line.removeprefix("// TITLE: ")
            doc += "\n<br>\n\n## " + title
            titles.append((title, 2))
            continue
        if line.startswith("// MARK"):
            title = line.removeprefix("// MARK: ")
            doc += "\n### " + title
            titles.append((title, 3))
            continue
        if line.startswith("// TODO"):
            continue
        if line.startswith("#ifdef BUDDY_IMPLEMENTATION"):
            s = ""
            s += "# Buddy Documentation\n\n"
            s += "## Table of Contents\n\n"
            for t in titles:
                name, idx = t
                name = name.strip("\n")
                if idx == 3:
                    s += f"  - [{name}](#{to_kebab_case(name)})\n"
                else:
                    s += f"- [{name}](#{to_kebab_case(name)})\n"
            s += "\n<br>\n"
            return s + doc
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
    first = 0

    for i, line in enumerate(lines, start=1):  # Iterate over lines with line numbers
        if line.startswith(term):
            occurrence_count += 1
            first = i
            if occurrence_count == 2:
                return i  # Return the line number of the second occurrence

    return first

if __name__ == "__main__":
    main(sys.argv[1:])
