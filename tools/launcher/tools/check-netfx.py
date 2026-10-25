#!/usr/bin/env python3
"""Gate: the launcher must run on stock Windows (.NET Framework 4.8).

1) Static check of src/*.cs: bans overloads that exist in Mono / .NET Core but not in .NET Framework 4.8
   (string.Trim/TrimStart/TrimEnd/Split(char), Contains/StartsWith/EndsWith(char), string.Join(char, ...),
   Replace(..., StringComparison), Math.Clamp, Array.Empty, KeyValuePair deconstruction, Span, ranges, ...).
   Write the char[] form instead: s.Trim(new[] { '/' }), s.Split(new[] { '.' }).
2) With an exe given: scans its IL (ikdasm/monodis) for the same members, so a compiler that bound to
   a newer BCL is caught even if the source looks fine.
Usage: check-netfx.py [src-dir] [built.exe]"""
import glob, os, re, shutil, subprocess, sys

CH = r"'(?:\\u[0-9A-Fa-f]{4}|\\.|[^'\\])'"
SRC_BANS = [
    (rf"\.(Trim|TrimStart|TrimEnd)\(\s*{CH}\s*\)", "Trim/TrimStart/TrimEnd(char): use (new[] { 'x' })"),
    (rf"\.(Trim|TrimStart|TrimEnd)\(\s*{CH}\s*,", "Trim*(char, char...): use (new[] { 'x', 'y' })"),
    (rf"\.Split\(\s*{CH}\s*[,)]", "Split(char[, ...]): use Split(new[] { 'x' }[, options])"),
    (rf"\.(Contains|StartsWith|EndsWith)\(\s*{CH}", "Contains/StartsWith/EndsWith(char): use the string form \"x\""),
    (rf"\b[Ss]tring\.Join\(\s*{CH}", "string.Join(char, ...): use a string separator"),
    (r"\.Replace\(\s*[^,()]+,\s*[^,()]+,\s*StringComparison\.", "Replace(a, b, StringComparison) is .NET Core only"),
    (r"\.Contains\(\s*[^,()]+,\s*StringComparison\.", "Contains(string, StringComparison) is .NET Core only"),
    (r"\bMath\.Clamp\b|\bMathF\.", "Math.Clamp / MathF are .NET Core only"),
    (r"\bArray\.Empty<", "Array.Empty<T> is not in .NET Framework 4.5 targets; use new T[0]"),
    (r"\b(ReadOnly)?Span<|\bstackalloc\b|\bIndex\b\s*\.|\.\.\^|\[\^", "Span/ranges are .NET Core only"),
    (r"\b(ToHashSet|TakeLast|SkipLast|GetValueOrDefault|TryAdd|Deconstruct)\(", ".NET Core-only LINQ/dictionary helpers"),
    (r"\bvar\s*\(\s*\w+\s*,", "tuple/KeyValuePair deconstruction needs C# 7"),
]
IL_BANS = [r"String::Trim(Start|End)?\(char\)", r"String::Split\(char(,|\))", r"String::(Contains|StartsWith|EndsWith)\(char",
           r"String::Join\(char", r"String::Replace\(string,\s*string,\s*valuetype \[mscorlib\]System\.StringComparison",
           r"String::Contains\(string,\s*valuetype", r"Math::Clamp", r"Array::Empty", r"System\.(ReadOnly)?Span`1", r"System\.ValueTuple"]

def main():
    src = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "src")
    bad = []
    for f in sorted(glob.glob(os.path.join(src, "*.cs"))):
        for i, line in enumerate(open(f, encoding="utf-8-sig"), 1):
            code = re.sub(r'@"(?:""|[^"])*"', '"s"', line)            # verbatim strings
            code = re.sub(r'"(?:\\.|[^"\\])*"', '"s"', code)          # normal strings (char literals stay: the bans need them)
            code = re.sub(r'//.*$', '', code)
            for rx, why in SRC_BANS:
                if re.search(rx, code): bad.append(f"{os.path.basename(f)}:{i}: {why}: {line.strip()}")
    if len(sys.argv) > 2:
        dis = shutil.which("ikdasm") or shutil.which("monodis")
        if not dis: bad.append("no ikdasm/monodis to scan the exe's IL")
        else:
            il = subprocess.run([dis, sys.argv[2]], capture_output=True, text=True).stdout
            il = re.sub(r"\s*\n\s+(?=[,)\[\w])", " ", il)   # join wrapped signatures
            if "System.String::" not in il: bad.append("IL scan read nothing from " + sys.argv[2])
            for rx in IL_BANS:
                for m in sorted(set(x.group(0) for x in re.finditer(r"\S*" + rx + r"[^\s]*", il)))[:5]:
                    bad.append("IL: " + m.strip())
    if bad:
        print("NOT .NET Framework 4.8 safe:"); [print("  " + b) for b in bad]; sys.exit(1)
    print("check-netfx: OK (" + src + (", IL of " + sys.argv[2] if len(sys.argv) > 2 else "") + ")")

main()
