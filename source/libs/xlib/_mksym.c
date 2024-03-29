
/* Constructs hash tables for XStringToKeysym and XKeysymToString. */

#include <X11/X.h>
#include <X11/Xos.h>
#include <X11/Xresource.h>
#include <X11/keysymdef.h>
#include <stdio.h>
#include <stdlib.h>
#include <io.h>
#include <inttypes.h>

#include "../libs/xlib/Xresinternal.h"

#define KTNUM 4000

static struct info {
    char* name;
    KeySym	val;
} info[KTNUM];

#define MIN_REHASH 15
#define MATCHES 10

static char tab[KTNUM];
static unsigned short offsets[KTNUM];
static unsigned short indexes[KTNUM];
static KeySym values[KTNUM];
static int ksnum = 0;

static int
parse_line(const char* buf, char* key, KeySym* val, char* prefix)
{
    int i;
    char alias[128];
    char* tmp, * tmpa;

    /* See if we can catch a straight XK_foo 0x1234-style definition first;
     * the trickery around tmp is to account for prefices. */
    i = sscanf(buf, "#define %127s 0x%lx", key, val);
    if (i == 2 && (tmp = strstr(key, "XK_"))) {
        memcpy(prefix, key, tmp - key);
        prefix[tmp - key] = '\0';
        tmp += 3;
        memmove(key, tmp, strlen(tmp) + 1);
        return 1;
    }

    /* Now try to catch alias (XK_foo XK_bar) definitions, and resolve them
     * immediately: if the target is in the form XF86XK_foo, we need to
     * canonicalise this to XF86foo before we do the lookup. */
    i = sscanf(buf, "#define %127s %127s", key, alias);
    if (i == 2 && (tmp = strstr(key, "XK_")) && (tmpa = strstr(alias, "XK_"))) {
        memcpy(prefix, key, tmp - key);
        prefix[tmp - key] = '\0';
        tmp += 3;
        memmove(key, tmp, strlen(tmp) + 1);
        memmove(tmpa, tmpa + 3, strlen(tmpa + 3) + 1);

        for (i = ksnum - 1; i >= 0; i--) {
            if (strcmp(info[i].name, alias) == 0) {
                *val = info[i].val;
                return 1;
            }
        }

        fprintf(stderr, "can't find matching definition %s for keysym %s%s\n",
            alias, prefix, key);
    }

    return 0;
}

int main()
{
    //argv[2] = "";
    //argc = 2;

    int max_rehash;
    Signature sig;
    int i, j, k, l, z;
    FILE* fptr;
    char* name;
    char c;
    int first;
    int best_max_rehash;
    int best_z = 0;
    int num_found;
    KeySym val;
    char key[128], prefix[128];
    static char buf[1024];

    //for (l = 1; l < argc; l++)
    {
        fptr = fopen("D:/VS2015/include/libs/X11/keysymdef.h", "r");
        if (!fptr) {
            fprintf(stderr, "couldn't open %s\n", "");
            //continue;
        }

        while (fgets(buf, sizeof(buf), fptr)) {
            if (!parse_line(buf, key, &val, prefix))
                continue;

            if (val == XK_VoidSymbol)
                val = 0;
            if (val > 0x1fffffff) {
                fprintf(stderr, "ignoring illegal keysym (%s, %lx)\n", key,
                    val);
                continue;
            }

            name = (char*)malloc(strlen(prefix) + strlen(key) + 1);
            if (!name) {
                fprintf(stderr, "makekeys: out of memory!\n");
                exit(1);
            }
            sprintf(name, "%s%s", prefix, key);
            info[ksnum].name = name;
            info[ksnum].val = val;
            ksnum++;
            if (ksnum == KTNUM) {
                fprintf(stderr, "makekeys: too many keysyms!\n");
                exit(1);
            }
        }

        fclose(fptr);
    }

    fptr = fopen("D:/VS2015/include/libs/X11/keysymdef_2.h", "w");

    fprintf(fptr, "/* This file is generated from keysymdef.h. */\n");
    fprintf(fptr, "/* Do not edit. */\n");
    fprintf(fptr, "\n");

    best_max_rehash = ksnum;
    num_found = 0;
    for (z = ksnum; z < KTNUM; z++) {
        max_rehash = 0;
        for (name = &tab[0], i = z; --i >= 0;)
            *name++ = 0;
        for (i = 0; i < ksnum; i++) {
            name = info[i].name;
            sig = 0;
            while ((c = *name++))
                sig = (sig << 1) + c;
            first = j = sig % z;
            for (k = 0; tab[j]; k++) {
                j += first + 1;
                if (j >= z)
                    j -= z;
                if (j == first)
                    goto next1;
            }
            tab[j] = 1;
            if (k > max_rehash)
                max_rehash = k;
        }
        if (max_rehash < MIN_REHASH) {
            if (max_rehash < best_max_rehash) {
                best_max_rehash = max_rehash;
                best_z = z;
            }
            num_found++;
            if (num_found >= MATCHES)
                break;
        }
    next1:;
    }

    z = best_z;
    if (z == 0) {
        fprintf(stderr, "makekeys: failed to find small enough hash!\n"
            "Try increasing KTNUM in makekeys.c\n");
        exit(1);
    }
    fprintf(fptr, "#ifdef NEEDKTABLE\n");
    fprintf(fptr, "const unsigned char _XkeyTable[] = {\n");
    fprintf(fptr, "0,\n");
    k = 1;
    for (i = 0; i < ksnum; i++) {
        name = info[i].name;
        sig = 0;
        while ((c = *name++))
            sig = (sig << 1) + c;
        first = j = sig % z;
        while (offsets[j]) {
            j += first + 1;
            if (j >= z)
                j -= z;
        }
        offsets[j] = k;
        indexes[i] = k;
        val = info[i].val;
        fprintf(fptr, "0x%.2lx, 0x%.2lx, 0x%.2lx, 0x%.2lx, 0x%.2lx, 0x%.2lx, ",
            (sig >> 8) & 0xff, sig & 0xff,
            (val >> 24) & 0xff, (val >> 16) & 0xff,
            (val >> 8) & 0xff, val & 0xff);
        for (name = info[i].name, k += 7; (c = *name++); k++)
            fprintf(fptr, "'%c',", c);
        fprintf(fptr, (i == (ksnum - 1)) ? "0\n" : "0,\n");
    }
    fprintf(fptr, "};\n");
    fprintf(fptr, "\n");
    fprintf(fptr, "#define KTABLESIZE %d\n", z);
    fprintf(fptr, "#define KMAXHASH %d\n", best_max_rehash + 1);
    fprintf(fptr, "\n");
    fprintf(fptr, "static const unsigned short hashString[KTABLESIZE] = {\n");
    for (i = 0; i < z;) {
        fprintf(fptr, "0x%.4x", offsets[i]);
        i++;
        if (i == z)
            break;
        fprintf(fptr, (i & 7) ? ", " : ",\n");
    }
    fprintf(fptr, "\n");
    fprintf(fptr, "};\n");
    fprintf(fptr, "#endif /* NEEDKTABLE */\n");

    best_max_rehash = ksnum;
    num_found = 0;
    for (z = ksnum; z < KTNUM; z++) {
        max_rehash = 0;
        for (name = tab, i = z; --i >= 0;)
            *name++ = 0;
        for (i = 0; i < ksnum; i++) {
            val = info[i].val;
            first = j = val % z;
            for (k = 0; tab[j]; k++) {
                if (values[j] == val)
                    goto skip1;
                j += first + 1;
                if (j >= z)
                    j -= z;
                if (j == first)
                    goto next2;
            }
            tab[j] = 1;
            values[j] = val;
            if (k > max_rehash)
                max_rehash = k;
        skip1:;
        }
        if (max_rehash < MIN_REHASH) {
            if (max_rehash < best_max_rehash) {
                best_max_rehash = max_rehash;
                best_z = z;
            }
            num_found++;
            if (num_found >= MATCHES)
                break;
        }
    next2:;
    }

    z = best_z;
    if (z == 0) {
        fprintf(stderr, "makekeys: failed to find small enough hash!\n"
            "Try increasing KTNUM in makekeys.c\n");
        exit(1);
    }
    for (i = z; --i >= 0;)
        offsets[i] = 0;
    for (i = 0; i < ksnum; i++) {
        val = info[i].val;
        first = j = val % z;
        while (offsets[j]) {
            if (values[j] == val)
                goto skip2;
            j += first + 1;
            if (j >= z)
                j -= z;
        }
        offsets[j] = indexes[i] + 2;
        values[j] = val;
    skip2:;
    }
    fprintf(fptr, "\n");
    fprintf(fptr, "#ifdef NEEDVTABLE\n");
    fprintf(fptr, "#define VTABLESIZE %d\n", z);
    fprintf(fptr, "#define VMAXHASH %d\n", best_max_rehash + 1);
    fprintf(fptr, "\n");
    fprintf(fptr, "static const unsigned short hashKeysym[VTABLESIZE] = {\n");
    for (i = 0; i < z;) {
        fprintf(fptr, "0x%.4x", offsets[i]);
        i++;
        if (i == z)
            break;
        fprintf(fptr, (i & 7) ? ", " : ",\n");
    }
    fprintf(fptr, "\n");
    fprintf(fptr, "};\n");
    fprintf(fptr, "#endif /* NEEDVTABLE */\n");

    exit(0);
}
