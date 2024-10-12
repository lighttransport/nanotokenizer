import regex as re

PRETOKENIZE_REGEX = r"""(?i:'s|'t|'re|'ve|'m|'ll|'d)|[^\r\n\p{L}\p{N}]?\p{L}+|\p{N}| ?[^\s\p{L}\p{N}]+[\r\n]*|\s*[\r\n]+|\s+(?!\S)|\s+"""


pat = re.compile(PRETOKENIZE_REGEX)

text = " 123 a bit"

toks = re.findall(pat, text)
print(toks)

text = " Hello\n I'm ok  @\n Hel"
toks = re.findall(pat, text)
print(toks)
