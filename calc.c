// Yagiz BALLI 251ADB060
// (optional) https://github.com/yagizballi/gradedlabc
// Compile with: gcc -O2 -Wall -Wextra -std=c17 -o calc calc.c -lm
// Resources used: <every AI tool, website and book you used; details in comments where you used them>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <errno.h>
#include <dirent.h>
#include <sys/stat.h>

#define FIRST_NAME "yagiz"
#define LAST_NAME "balli"
#define STUDENT_ID "251ADB060"

typedef struct {
    int is_float;
    long long integer;
    double real;
} Number;

typedef enum {
    T_NUMBER, T_PLUS, T_MINUS, T_MUL, T_DIV, T_POWER,
    T_LEFT, T_RIGHT, T_NEWLINE, T_END, T_INVALID
} TokenKind;

typedef struct {
    TokenKind kind;
    size_t position;
    Number number;
} Token;

typedef struct {
    char *text;
    size_t length;
    size_t index;
    int line_start;
    Token token;
    size_t error;
    int system_error;
} Parser;

typedef struct {
    Number value;
    size_t operand_position;
    size_t operator_position;
} PowerItem;

static Number expression(Parser *p);

static Number integer_number(long long value) {
    Number n = {0, value, 0.0};
    return n;
}

static void syntax_error(Parser *p, size_t position) {
    if (p->error == 0) p->error = position;
}

static void memory_error(void) {
    fprintf(stderr, "Memory allocation failed.\n");
}

static char *read_file(const char *path, size_t *length) {
    FILE *fp = fopen(path, "rb");
    if (fp == NULL) {
        perror(path);
        return NULL;
    }
    size_t capacity = 8192, used = 0;
    char *text = malloc(capacity);
    if (text == NULL) {
        memory_error();
        fclose(fp);
        return NULL;
    }
    for (;;) {
        if (used == capacity - 1) {
            if (capacity > (size_t)-1 / 2) {
                fprintf(stderr, "Input file is too large.\n");
                free(text);
                fclose(fp);
                return NULL;
            }
            size_t next_capacity = capacity * 2;
            char *larger = realloc(text, next_capacity);
            if (larger == NULL) {
                memory_error();
                free(text);
                fclose(fp);
                return NULL;
            }
            text = larger;
            capacity = next_capacity;
        }
        size_t got = fread(text + used, 1, capacity - used - 1, fp);
        used += got;
        if (got == 0) {
            if (ferror(fp)) {
                fprintf(stderr, "Cannot read %s.\n", path);
                free(text);
                fclose(fp);
                return NULL;
            }
            break;
        }
    }
    if (fclose(fp) != 0) {
        fprintf(stderr, "Cannot close %s.\n", path);
        free(text);
        return NULL;
    }
    text[used] = '\0';
    *length = used;
    return text;
}


static void next_token(Parser *p) {
    char *s = p->text;
    while (p->index < p->length &&
           (s[p->index] == ' ' || s[p->index] == '\t' ||
            s[p->index] == '\r')) p->index++;
    if (p->line_start && p->index < p->length && s[p->index] == '#') {
        while (p->index < p->length && s[p->index] != '\n') p->index++;
    }
    p->token.position = p->index + 1;
    p->token.number = integer_number(0);
    if (p->index == p->length) {
        p->token.kind = T_END;
        return;
    }
    unsigned char ch = (unsigned char)s[p->index];
    if (ch == '\n') {
        p->index++;
        p->line_start = 1;
        p->token.kind = T_NEWLINE;
        return;
    }
    p->line_start = 0;
    if (isdigit(ch)) {
        size_t start = p->index;
        while (p->index < p->length &&
               isdigit((unsigned char)s[p->index])) p->index++;
        int real = 0;
        if (p->index + 1 < p->length && s[p->index] == '.' &&
            isdigit((unsigned char)s[p->index + 1])) {
            real = 1;
            p->index++;
            while (p->index < p->length &&
                   isdigit((unsigned char)s[p->index])) p->index++;
        }
        char saved = s[p->index];
        s[p->index] = '\0';
        errno = 0;
        if (real) {
            p->token.number.is_float = 1;
            p->token.number.real = strtod(s + start, NULL);
        } else {
            p->token.number.integer = strtoll(s + start, NULL, 10);
        }
        int bad_integer = !real && errno == ERANGE;
        s[p->index] = saved;
        p->token.kind = bad_integer ? T_INVALID : T_NUMBER;
        return;
    }
    p->index++;
    switch (ch) {
        case '+': p->token.kind = T_PLUS; break;
        case '-': p->token.kind = T_MINUS; break;
        case '/': p->token.kind = T_DIV; break;
        case '(': p->token.kind = T_LEFT; break;
        case ')': p->token.kind = T_RIGHT; break;
        case '*':
            if (p->index < p->length && s[p->index] == '*') {
                p->index++;
                p->token.kind = T_POWER;
            } else p->token.kind = T_MUL;
            break;
        default: p->token.kind = T_INVALID; break;
    }
}

static Number apply(Parser *p, TokenKind op, size_t op_position,
                    size_t right_position, Number left, Number right) {
    Number result = integer_number(0);
    if (op == T_DIV &&
        (right.is_float ? right.real == 0.0 : right.integer == 0)) {
        syntax_error(p, right_position);
        return result;
    }
    if (left.is_float || right.is_float) {
        double a = left.is_float ? left.real : (double)left.integer;
        double b = right.is_float ? right.real : (double)right.integer;
        result.is_float = 1;
        switch (op) {
            case T_PLUS:  result.real = a + b; break;
            case T_MINUS: result.real = a - b; break;
            case T_MUL:   result.real = a * b; break;
            case T_DIV:   result.real = a / b; break;
            case T_POWER: result.real = pow(a, b); break;
            default: break;
        }
        if (!isfinite(result.real)) syntax_error(p, op_position);
        return result;
    }
    switch (op) {
        case T_PLUS:  result.integer = left.integer + right.integer; break;
        case T_MINUS: result.integer = left.integer - right.integer; break;
        case T_MUL:   result.integer = left.integer * right.integer; break;
        case T_DIV:   result.integer = left.integer / right.integer; break;
        case T_POWER: {
            if (right.integer < 0) {
                syntax_error(p, right_position);
                return result;
            }
            long long base = left.integer, exponent = right.integer;
            result.integer = 1;
            while (exponent > 0) {
                if (exponent % 2 != 0) result.integer *= base;
                exponent /= 2;
                if (exponent > 0) base *= base;
            }
            break;
        }
        default: break;
    }
    return result;
}

static Number primary(Parser *p) {
    Number result = integer_number(0);
    if (p->token.kind == T_NUMBER) {
        result = p->token.number;
        next_token(p);
    } else if (p->token.kind == T_LEFT) {
        next_token(p);
        result = expression(p);
        if (!p->error && !p->system_error) {
            if (p->token.kind != T_RIGHT) syntax_error(p, p->token.position);
            else next_token(p);
        }
    } else syntax_error(p, p->token.position);
    return result;
}

static Number power(Parser *p) {
    size_t first_position = p->token.position;
    Number first = primary(p);
    if (p->error || p->system_error || p->token.kind != T_POWER) return first;
    size_t capacity = 8, count = 1;
    PowerItem *items = malloc(capacity * sizeof *items);
    if (items == NULL) {
        memory_error();
        p->system_error = 1;
        return integer_number(0);
    }
    items[0] = (PowerItem){first, first_position, 0};
    while (p->token.kind == T_POWER && !p->error && !p->system_error) {
        items[count - 1].operator_position = p->token.position;
        next_token(p);
        size_t position = p->token.position;
        Number value = primary(p);
        if (p->error || p->system_error) break;
        if (count == capacity) {
            if (capacity > (size_t)-1 / 2 / sizeof *items) {
                fprintf(stderr, "Expression is too large.\n");
                p->system_error = 1;
                break;
            }
            size_t next_capacity = capacity * 2;
            PowerItem *larger = realloc(items, next_capacity * sizeof *items);
            if (larger == NULL) {
                memory_error();
                p->system_error = 1;
                break;
            }
            items = larger;
            capacity = next_capacity;
        }
        items[count++] = (PowerItem){value, position, 0};
    }
    Number result = integer_number(0);
    if (!p->error && !p->system_error) {
        result = items[count - 1].value;
        while (count > 1 && !p->error) {
            size_t i = count - 2;
            result = apply(p, T_POWER, items[i].operator_position,
                           items[i + 1].operand_position, items[i].value, result);
            count--;
        }
    }
    free(items);
    return result;
}

static Number term(Parser *p) {
    Number result = power(p);
    while (!p->error && !p->system_error &&
           (p->token.kind == T_MUL || p->token.kind == T_DIV)) {
        Token operation = p->token;
        next_token(p);
        size_t position = p->token.position;
        Number right = power(p);
        if (!p->error && !p->system_error)
            result = apply(p, operation.kind, operation.position,
                           position, result, right);
    }
    return result;
}

static Number expression(Parser *p) {
    Number result = term(p);
    while (!p->error && !p->system_error &&
           (p->token.kind == T_PLUS || p->token.kind == T_MINUS)) {
        Token operation = p->token;
        next_token(p);
        size_t position = p->token.position;
        Number right = term(p);
        if (!p->error && !p->system_error)
            result = apply(p, operation.kind, operation.position,
                           position, result, right);
    }
    return result;
}

static int process_file(const char *input, const char *output) {
    size_t length = 0;
    char *text = read_file(input, &length);
    if (text == NULL) return -1;
    FILE *spool = tmpfile();
    if (spool == NULL) {
        perror("Temporary results");
        free(text);
        return -1;
    }
    Parser p = {0};
    p.text = text;
    p.length = length;
    p.line_start = 1;
    next_token(&p);
    while (p.token.kind != T_END && !p.error && !p.system_error) {
        if (p.token.kind == T_NEWLINE) {
            next_token(&p);
            continue;
        }
        Number value = expression(&p);
        if (p.error || p.system_error) break;
        if (p.token.kind != T_NEWLINE && p.token.kind != T_END) {
            syntax_error(&p, p.token.position);
            break;
        }
        int written = value.is_float ? fprintf(spool, "%.15g\n", value.real)
                                     : fprintf(spool, "%lld\n", value.integer);
        if (written < 0) {
            fprintf(stderr, "Cannot store results.\n");
            p.system_error = 1;
        }
    }
    free(text);
    if (p.system_error) {
        fclose(spool);
        return -1;
    }
    FILE *out = fopen(output, "wb");
    if (out == NULL) {
        perror(output);
        fclose(spool);
        return -1;
    }
    int failed = 0;
    if (p.error) failed = fprintf(out, "ERROR:%zu\n", p.error) < 0;
    else if (fflush(spool) != 0 || fseek(spool, 0, SEEK_SET) != 0) failed = 1;
    else {
        char buffer[8192];
        size_t got;
        while ((got = fread(buffer, 1, sizeof buffer, spool)) > 0) {
            if (fwrite(buffer, 1, got, out) != got) {
                failed = 1;
                break;
            }
        }
        if (ferror(spool)) failed = 1;
    }
    if (fclose(spool) != 0) failed = 1;
    if (fclose(out) != 0) failed = 1;
    if (failed) {
        fprintf(stderr, "Cannot write complete results to %s.\n", output);
        return -1;
    }
    return 0;
}

static char *join_path(const char *directory, const char *name) {
    size_t a = strlen(directory), b = strlen(name);
    if (a > (size_t)-1 - 2 || b > (size_t)-1 - a - 2) {
        memory_error();
        return NULL;
    }
    char *path = malloc(a + b + 2);
    if (path == NULL) {
        memory_error();
        return NULL;
    }
    memcpy(path, directory, a);
    path[a] = '/';
    memcpy(path + a + 1, name, b + 1);
    return path;
}

static int txt_name(const char *name) {
    size_t length = strlen(name);
    return length >= 4 && strcmp(name + length - 4, ".txt") == 0;
}

static char *base_name(const char *path, int remove_extension) {
    size_t end = strlen(path);
    while (end > 0 && path[end - 1] == '/') end--;
    size_t start = end;
    while (start > 0 && path[start - 1] != '/') start--;
    size_t length = end - start;
    if (remove_extension && length >= 4 &&
        memcmp(path + end - 4, ".txt", 4) == 0) length -= 4;
    if (end == 0) {
        path = "root";
        start = 0;
        length = 4;
    }
    char *base = malloc(length + 1);
    if (base == NULL) {
        memory_error();
        return NULL;
    }
    memcpy(base, path + start, length);
    base[length] = '\0';
    return base;
}

static char *with_suffix(const char *base, const char *suffix) {
    size_t a = strlen(base), b = strlen(suffix);
    if (a > (size_t)-1 - b - 1) {
        memory_error();
        return NULL;
    }
    char *name = malloc(a + b + 1);
    if (name == NULL) {
        memory_error();
        return NULL;
    }
    memcpy(name, base, a);
    memcpy(name + a, suffix, b + 1);
    return name;
}

static int make_output_directory(const char *path) {
    struct stat info;
    if (stat(path, &info) == 0) {
        if (S_ISDIR(info.st_mode)) return 0;
        fprintf(stderr, "Output path is not a directory: %s\n", path);
        return -1;
    }
    if (mkdir(path, 0777) != 0) {
        perror(path);
        return -1;
    }
    return 0;
}

static int run_file(const char *input, const char *output_directory) {
    char *base = base_name(input, 1);
    if (base == NULL) return -1;
    char *name = with_suffix(base, "_" FIRST_NAME "_" LAST_NAME
                                   "_" STUDENT_ID ".txt");
    free(base);
    if (name == NULL) return -1;
    char *output = join_path(output_directory, name);
    free(name);
    if (output == NULL) return -1;
    int status = process_file(input, output);
    free(output);
    return status;
}

static int run_directory(const char *directory, const char *output_directory) {
    DIR *dir = opendir(directory);
    if (dir == NULL) {
        perror(directory);
        return -1;
    }
    char **paths = NULL;
    size_t count = 0, capacity = 0;
    int failed = 0;
    for (;;) {
        errno = 0;
        struct dirent *entry = readdir(dir);
        if (entry == NULL) {
            if (errno != 0) {
                perror(directory);
                failed = 1;
            }
            break;
        }
        if (!txt_name(entry->d_name)) continue;
        char *path = join_path(directory, entry->d_name);
        if (path == NULL) {
            failed = 1;
            break;
        }
        struct stat info;
        if (stat(path, &info) != 0) {
            perror(path);
            free(path);
            failed = 1;
            break;
        }
        if (!S_ISREG(info.st_mode)) {
            free(path);
            continue;
        }
        if (count == capacity) {
            if (capacity > (size_t)-1 / 2 / sizeof *paths) {
                memory_error();
                free(path);
                failed = 1;
                break;
            }
            size_t next_capacity = capacity == 0 ? 16 : capacity * 2;
            char **larger = realloc(paths, next_capacity * sizeof *paths);
            if (larger == NULL) {
                memory_error();
                free(path);
                failed = 1;
                break;
            }
            paths = larger;
            capacity = next_capacity;
        }
        paths[count++] = path;
    }
    if (closedir(dir) != 0) {
        fprintf(stderr, "Cannot close input directory.\n");
        failed = 1;
    }
    if (!failed) {
        for (size_t i = 0; i < count; i++) {
            if (run_file(paths[i], output_directory) != 0) {
                failed = 1;
                break;
            }
        }
    }
    for (size_t i = 0; i < count; i++) free(paths[i]);
    free(paths);
    return failed ? -1 : 0;
}

static int usage(const char *program) {
    fprintf(stderr, "Usage: %s [-d DIR | --dir DIR] "
                    "[-o OUTDIR | --output-dir OUTDIR] [input.txt]\n", program);
    return EXIT_FAILURE;
}

int main(int argc, char *argv[]) {
    const char *directory = NULL, *input = NULL, *output = NULL;
    int positional_only = 0;
    for (int i = 1; i < argc; i++) {
        const char *arg = argv[i];
        if (!positional_only && strcmp(arg, "--") == 0) positional_only = 1;
        else if (!positional_only &&
                 (strcmp(arg, "-d") == 0 || strcmp(arg, "--dir") == 0)) {
            if (directory != NULL || i + 1 == argc) return usage(argv[0]);
            directory = argv[++i];
        } else if (!positional_only &&
                   (strcmp(arg, "-o") == 0 || strcmp(arg, "--output-dir") == 0)) {
            if (output != NULL || i + 1 == argc) return usage(argv[0]);
            output = argv[++i];
        } else if (!positional_only && arg[0] == '-') return usage(argv[0]);
        else if (input != NULL) return usage(argv[0]);
        else input = arg;
    }
    if ((directory == NULL && input == NULL) ||
        (directory != NULL && input != NULL) ||
        (directory != NULL && directory[0] == '\0') ||
        (input != NULL && input[0] == '\0') ||
        (output != NULL && output[0] == '\0')) return usage(argv[0]);

    const char *source = directory != NULL ? directory : input;
    struct stat info;
    if (stat(source, &info) != 0) {
        perror(source);
        return EXIT_FAILURE;
    }
    if (directory != NULL ? !S_ISDIR(info.st_mode) : !S_ISREG(info.st_mode)) {
        fprintf(stderr, "Wrong input type: %s\n", source);
        return EXIT_FAILURE;
    }
    char *owned_output = NULL;
    if (output == NULL) {
        char *base = base_name(source, directory == NULL);
        if (base == NULL) return EXIT_FAILURE;
        owned_output = with_suffix(base, "_" FIRST_NAME "_" STUDENT_ID);
        free(base);
        if (owned_output == NULL) return EXIT_FAILURE;
        output = owned_output;
    }
    if (make_output_directory(output) != 0) {
        free(owned_output);
        return EXIT_FAILURE;
    }
    int status = directory != NULL ? run_directory(directory, output)
                                   : run_file(input, output);
    free(owned_output);
    return status == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
