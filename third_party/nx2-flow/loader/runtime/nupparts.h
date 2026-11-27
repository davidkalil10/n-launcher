// A NUP file kept in parts, found: a card of the FAT32 kind takes no file of 4 GB or more, so a game's file is cut in pieces
// with whatever the user has at hand (7-Zip's "Split file", split, HJSplit, a dumping tool's own way), and the pieces are read
// here as the one file they were. One header, in C, for the game loader (runtime/game.c) and for nx2-flow's menu, so that both
// know the same game by the same rules. Nothing of the console's in it: the C library's folders and files only.
//
// What a part's name looks like (the game's whole file being "Game.nup"):
//   Game.nup.001  Game.nup.002  ...     a number as the last ending (7-Zip, HJSplit: from 001; split -d: from 00; from 0 or 1)
//   Game.nup.part1  Game.nup.part2      "part" and a number
//   Game.nup.aa  Game.nup.ab  ...       letters (split's own way), after ".nup" only
//   Game.nu0  Game.nu1  ...             the ending's last letter made a number (as a card dump's xc0, xc1)
//   Game.part1.nup  Game.001.nup        the number before the ending
//   00  01  02  ...                     no name at all, in a folder of the game's own (a dumping tool's way): the folder is the game
// The parts of one game share the name and the way, are numbered one after the other from 0 or 1 (aa), and are at least two
// (a folder of nameless parts may hold one). A name that looks like a part and has no such company is a file of its own.
#ifndef NUPPARTS_H
#define NUPPARTS_H

#include <dirent.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#define NUP_PARTS_MAX 64
#define NUP_PARTS_PATH 768
#if defined(__GNUC__) || defined(__clang__)
#define NUP_PARTS_MAYBE_UNUSED __attribute__((unused))
#else
#define NUP_PARTS_MAYBE_UNUSED
#endif

typedef struct {
    char path[NUP_PARTS_PATH];
    uint64_t size;
} NupPart;

enum { NUP_PART_NONE, NUP_PART_NUMBER, NUP_PART_WORD, NUP_PART_LETTERS, NUP_PART_NUX, NUP_PART_MID, NUP_PART_MIDWORD, NUP_PART_BARE };
typedef struct {
    int kind;
    unsigned index;
    char stem[NUP_PARTS_PATH];  // (what the parts of one game have in common: the whole file's name, where the way tells it)
} NupPartName;

static int nup__lower(int c) { return c >= 'A' && c <= 'Z' ? c + 32 : c; }
static int nup__same(const char *a, const char *b, size_t n) {  // (letters of either case)
    for (size_t k = 0; k < n; k++)
        if (nup__lower((unsigned char)a[k]) != nup__lower((unsigned char)b[k])) return 0;
    return 1;
}
// (so many digits and nothing else, four at most: their number, or -1)
static long nup__number(const char *s, size_t n) {
    long v = 0;
    if (n < 1 || n > 4) return -1;
    for (size_t k = 0; k < n; k++) {
        if (s[k] < '0' || s[k] > '9') return -1;
        v = v * 10 + (s[k] - '0');
    }
    return v;
}
// ("part" and a number: the number, or -1)
static long nup__word(const char *s, size_t n) { return n > 4 && nup__same(s, "part", 4) ? nup__number(s + 4, n - 4) : -1; }
static int nup__ends_nup(const char *s, size_t n) { return n > 4 && nup__same(s + n - 4, ".nup", 4); }

// Is this the name of a file that ends ".nup"?
NUP_PARTS_MAYBE_UNUSED static int nup_name_is_nup(const char *name) { return nup__ends_nup(name, strlen(name)); }

// A name, read as a part's: 1 and what it would be a part of, or 0 when it has no such look.
static int nup_part_name(const char *name, NupPartName *out) {
    const size_t n = strlen(name);
    const char *dot = strrchr(name, '.');
    out->kind = NUP_PART_NONE, out->index = 0, out->stem[0] = 0;
    if (!n || n >= NUP_PARTS_PATH || name[0] == '.') return 0;
    if (!dot) {
        const long v = nup__number(name, n);
        if (v < 0) return 0;
        out->kind = NUP_PART_BARE, out->index = (unsigned)v;
        return 1;
    }
    const size_t s = (size_t)(dot - name), e = n - s - 1;  // (the name before its last ending, and that ending)
    const char *end = dot + 1;
    long v;
    if ((v = nup__number(end, e)) >= 0) {
        out->kind = NUP_PART_NUMBER;
    } else if ((v = nup__word(end, e)) >= 0) {
        out->kind = NUP_PART_WORD;
    } else if (e >= 2 && e <= 3 && nup__ends_nup(name, s)) {
        v = 0;
        for (size_t k = 0; k < e; k++) {
            if (end[k] < 'a' || end[k] > 'z') return 0;
            v = v * 26 + (end[k] - 'a');
        }
        out->kind = NUP_PART_LETTERS;
    } else if (e == 3 && nup__same(end, "nu", 2) && end[2] >= '0' && end[2] <= '9') {
        v = end[2] - '0', out->kind = NUP_PART_NUX;
    } else if (e == 3 && nup__same(end, "nup", 3) && s) {
        // (the number before the ending: "Game.part1.nup", "Game.001.nup"; the stem is the name without it)
        const char *mid = NULL;
        for (const char *c = dot - 1; c >= name; c--)
            if (*c == '.') {
                mid = c;
                break;
            }
        if (!mid || mid == name) return 0;
        const size_t m = (size_t)(dot - mid - 1);
        if ((v = nup__number(mid + 1, m)) >= 0) out->kind = NUP_PART_MID;
        else if ((v = nup__word(mid + 1, m)) >= 0) out->kind = NUP_PART_MIDWORD;
        else return 0;
        memcpy(out->stem, name, (size_t)(mid - name)), out->stem[mid - name] = 0;
        out->index = (unsigned)v;
        return 1;
    } else {
        return 0;
    }
    memcpy(out->stem, name, s), out->stem[s] = 0;
    out->index = (unsigned)v;
    return 1;
}

// (a folder and a name in it, as one path: 0 if it doesn't fit. The card's root is "sdmc:/", which has its slash already.)
static int nup__join(char *to, size_t size, const char *dir, const char *name) {
    const size_t n = strlen(dir);
    return snprintf(to, size, n && dir[n - 1] == '/' ? "%s%s" : "%s/%s", dir, name) < (int)size;
}

static uint64_t nup__size(const char *path, int *is_dir) {
#ifdef _WIN32
    struct _stat64 st;
    if (_stat64(path, &st)) return *is_dir = -1, 0;
#else
    struct stat st;
    if (stat(path, &st)) return *is_dir = -1, 0;
#endif
    *is_dir = (st.st_mode & S_IFMT) == S_IFDIR;
    return (uint64_t)st.st_size;
}

// The files of a folder that are parts of the same game as `like` (or, with like's kind NONE, all that look like parts of one
// game: the first way met decides). Their indices, into seen[] (0: not there), and the lowest and highest. The number found.
// others: how many more of the folder's files are a game's whole file or look like parts of another.
static int nup__group(const char *dir, NupPartName *like, char names[][256], unsigned char *seen, unsigned limit, unsigned *lowest, unsigned *others) {
    DIR *d = opendir(dir);
    int found = 0;
    *lowest = limit, *others = 0;
    memset(seen, 0, limit);
    if (!d) return 0;
    for (struct dirent *e; (e = readdir(d)) != NULL;) {
        NupPartName p;
        if (strlen(e->d_name) >= 256) continue;
        if (!nup_part_name(e->d_name, &p)) {
            *others += nup_name_is_nup(e->d_name);
            continue;
        }
        if (like->kind == NUP_PART_NONE && p.kind != NUP_PART_BARE) *like = p;
        if (p.kind != like->kind || p.index >= limit || strlen(p.stem) != strlen(like->stem) || !nup__same(p.stem, like->stem, strlen(p.stem))) {
            *others += p.kind != NUP_PART_BARE;
            continue;
        }
        if (seen[p.index]) {  // (two files for one number: "Game.nup.1" and "Game.nup.001". Not a game's parts.)
            found = 0;
            break;
        }
        seen[p.index] = 1, strcpy(names[p.index], e->d_name), found++;
        if (p.index < *lowest) *lowest = p.index;
    }
    closedir(d);
    return found;
}

// The parts of the game a path names, in their order: the path of a whole file (one part: itself), of a game's first part, or
// of a folder that holds a game's parts and nothing else that looks like one. How many (max at most); 0 when there is no such
// file or the folder holds none. `later`, if given: set when the path is a part of a game but not its first.
static int nup_parts_find_(const char *path, NupPart *parts, int max, int *later) {
    enum { LIMIT = 1024 };
    static char names[LIMIT][256];
    static unsigned char seen[LIMIT];
    char dir[NUP_PARTS_PATH];
    NupPartName like;
    int is_dir = 0;
    unsigned lowest = 0, others = 0;
    const size_t n = strlen(path);
    if (later) *later = 0;
    if (!n || n >= NUP_PARTS_PATH - 260 || max < 1) return 0;
    const uint64_t size = nup__size(path, &is_dir);
    if (is_dir < 0) return 0;
    if (is_dir) {
        strcpy(dir, path);
        // (nameless parts first: a folder of them may hold other things, a note, a checksum)
        like.kind = NUP_PART_BARE, like.stem[0] = 0, like.index = 0;
        int found = nup__group(dir, &like, names, seen, LIMIT, &lowest, &others);
        if (!found) {
            // (or the named parts of one game, and no other game's file beside them: a folder of several games is not a game)
            like.kind = NUP_PART_NONE;
            found = nup__group(dir, &like, names, seen, LIMIT, &lowest, &others);
            if (found < 2 || others) return 0;
        }
        if (lowest > 1) return 0;
        for (unsigned k = lowest; k < lowest + (unsigned)found; k++)  // (one after the other, none missing)
            if (k >= LIMIT || !seen[k]) return 0;
    } else {
        const char *slash = strrchr(path, '/');
#ifdef _WIN32
        const char *back = strrchr(path, '\\');
        if (back && (!slash || back > slash)) slash = back;
#endif
        const char *name = slash ? slash + 1 : path;
        // (the folder the file is in; a file of the root keeps the root's slash: "sdmc:" alone is not the card's root)
        size_t d = slash ? (size_t)(slash - path) : 0;
        if (slash && (!d || path[d - 1] == ':')) d++;
        if (slash) memcpy(dir, path, d), dir[d] = 0;
        else strcpy(dir, ".");
        int alone = !nup_part_name(name, &like);
        // (a nameless part named itself: the folder it is in is the game)
        if (!alone && like.kind == NUP_PART_BARE) return slash ? nup_parts_find_(dir, parts, max, NULL) : 0;
        if (!alone) {
            const unsigned mine = like.index;
            const int found = nup__group(dir, &like, names, seen, LIMIT, &lowest, &others);
            unsigned run = 0;
            while (lowest + run < LIMIT && seen[lowest + run]) run++;
            if (found < 2 || lowest > 1 || run < 2 || mine >= lowest + run) alone = 1;
            else if (mine != lowest) {
                if (later) *later = 1;
                alone = 1;
            }
        }
        if (alone) {
            strcpy(parts[0].path, path), parts[0].size = size;
            return 1;
        }
    }
    int count = 0;
    for (unsigned k = lowest; k < LIMIT && seen[k] && count < max; k++, count++) {
        int sub = 0;
        if (!nup__join(parts[count].path, sizeof parts[count].path, dir, names[k])) return 0;
        parts[count].size = nup__size(parts[count].path, &sub);
        if (sub) return 0;
    }
    return count;
}
NUP_PARTS_MAYBE_UNUSED static int nup_parts_find(const char *path, NupPart *parts, int max) { return nup_parts_find_(path, parts, max, NULL); }

// For a library's listing, what an entry of a folder is:
//   0    nothing to list: not a game's file, or a later part of a game that is listed by its first;
//   n    a game in n parts (1: a whole file), this entry being its first part, or the folder that holds its parts;
//  -1    a folder that is not one game's parts: its own entries are to be looked at.
// parts[] is filled for n; its first is where the game's NUP header is, and what the loader is to be given.
NUP_PARTS_MAYBE_UNUSED static int nup_parts_entry(const char *dir, const char *name, int is_dir, NupPart *parts, int max) {
    char path[NUP_PARTS_PATH];
    if (name[0] == '.' || strlen(dir) + strlen(name) + 2 >= sizeof path - 260) return is_dir ? -1 : 0;
    if (!nup__join(path, sizeof path, dir, name)) return is_dir ? -1 : 0;
    if (is_dir) {
        const int n = nup_parts_find(path, parts, max);
        return n > 0 ? n : -1;
    }
    int later = 0;
    const int n = nup_parts_find_(path, parts, max, &later);
    if (n > 1) return n;
    return n == 1 && !later && nup_name_is_nup(name) ? 1 : 0;
}

// Reading the parts as the one file they are: where a place in the whole file is. The part's number and the place in it, and
// how much of that part is left from there; -1 past the end.
NUP_PARTS_MAYBE_UNUSED static int nup_parts_at(const NupPart *parts, int count, uint64_t at, uint64_t *in_part, uint64_t *left) {
    for (int k = 0; k < count; k++) {
        if (at < parts[k].size) {
            *in_part = at, *left = parts[k].size - at;
            return k;
        }
        at -= parts[k].size;
    }
    return -1;
}
NUP_PARTS_MAYBE_UNUSED static uint64_t nup_parts_size(const NupPart *parts, int count) {
    uint64_t all = 0;
    for (int k = 0; k < count; k++) all += parts[k].size;
    return all;
}

#endif
