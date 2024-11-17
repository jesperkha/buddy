# Garbage script to automatically create documentation from header file

import sys
from typing import TextIO

OUTPUT_FILE = "DOCS.md"

def error_exit(msg: str) -> None:
    print(f"autodoc: {msg}")
    exit(1)

def to_kebab_case(value):
    return "-".join(value.lower().split())

def main(args: list[str]):
    make_cheatsheet(args)

def make_cheatsheet(args: list[str]) -> None:
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
            doc += "\n// " + title + "// --------------------------------------\n"
            titles.append((title, 2))
            continue
        if line.startswith("// MARK"):
            title = line.removeprefix("// MARK: ")
            doc += "\n// " + title
            titles.append((title, 3))
            continue
        if line.startswith("// TODO"):
            continue
        if line.startswith("#ifdef BUDDY_IMPLEMENTATION"):
            s = ""
            s += "# Buddy Cheatsheet\n\n"
            # s += "## Table of Contents\n\n"
            # for t in titles:
            #     name, idx = t
            #     name = name.strip("\n")
            #     if idx == 3:
            #         s += f"  - [{name}](#{to_kebab_case(name)})\n"
            #     else:
            #         s += f"- [{name}](#{to_kebab_case(name)})\n"
            # s += "\n<br>\n"
            s += f"\n```c{doc}```"
            return s
        if line.startswith("//"):
            docline += line
        elif docline != "":
            docline = docline.replace("// ", "").replace("\n", " ")
            stripped = line.strip('\n')
            if not stripped.endswith(";"):
                stripped += ";"
            line = get_function_implementation(file_s, stripped)
            source = f"[source](https://github.com/jesperkha/buddy/blob/main/buddy.h#L{line})"
            # docline = f"\n```c\n{stripped}\n```\n\n{docline} {source}\n"
            # comment = ("\n"+48*' '+" // ").join(split_string(docline, 120).split("\n"))
            comment = docline
            docline = f"{stripped}{(48 - len(stripped)) * ' '} // {comment}\n"
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

def split_string(input_string, max_length):
    # Split the input string into words
    words = input_string.split()
    lines = []
    current_line = ""

    for word in words:
        # Check if adding the next word would exceed the max length
        if len(current_line) + len(word) + 1 > max_length:
            # If yes, add the current line to the list and start a new line
            lines.append(current_line.strip())
            current_line = word
        else:
            # If no, add the word to the current line
            current_line += " " + word

    # Add the last line to the list
    if current_line:
        lines.append(current_line.strip())

    # Join the lines with newline characters and return
    return "\n".join(lines)

if __name__ == "__main__":
    main(sys.argv[1:])
