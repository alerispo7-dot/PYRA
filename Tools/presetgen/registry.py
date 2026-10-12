"""Where the recipes register their archetypes."""
ARCH = {}        # (macro, sub) -> list of archetypes
ROLE = {}        # (macro, sub) -> role used by apply_character
CHARS = {}       # (macro, sub) -> list of characters to cycle through (default: all)


def arch(macro, sub, nouns, extra=None, role=None, chars=None):
    """Decorator: one archetype (a sound-design approach) of a sub-category."""
    def deco(fn):
        ARCH.setdefault((macro, sub), []).append({"nouns": nouns, "extra": extra or [], "build": fn})
        if role: ROLE[(macro, sub)] = role
        if chars: CHARS[(macro, sub)] = chars
        return fn
    return deco
