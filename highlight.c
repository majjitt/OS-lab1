#include "types.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h"

#define MAX_INPUT 512
#define KEYWORDS_COUNT 7
int strncmp_custom(const char *s1, const char *s2, int n) {
    while (n > 0 && *s1 && (*s1 == *s2)) {
        s1++;
        s2++;
        n--;
    }
    return (n == 0) ? 0 : (*(unsigned char *)s1 - *(unsigned char *)s2);
}
const char *keywords[KEYWORDS_COUNT] = {"void", "int", "char", "if", "for", "while", "return"};

void highlight(const char *input) {
    int i = 0;
    int inside_comment = 0;  // برای نادیده گرفتن متن بین دو #
    
    if (input[0] != '!') {
        printf(1, "%s\n", input);
        return;
    }

    input++; // حذف `!` از ابتدا
    printf(1, "! ");  // جمله همان‌طور که هست paste می‌شود

    while (*input) {
        // بررسی ورود به محدوده‌ی نادیده‌گیری
        if (*input == '#') {
            inside_comment = !inside_comment;  // تغییر وضعیت ورود به محدوده‌ی نادیده‌گیری
            input++;
            continue;
        }

        if (inside_comment) {
            input++;  // نادیده گرفتن کاراکترها بین دو #
            continue;
        }

        // بررسی کلمات کلیدی و برجسته‌سازی آنها
        int found = 0;
        for (i = 0; i < KEYWORDS_COUNT; i++) {
            int len = 0;
            while (keywords[i][len]) len++;  // محاسبه طول رشته بدون `strlen`
            
            if (strncmp_custom(input, keywords[i], len) == 0 && (input[len] == ' ' || input[len] == ';' || input[len] == '\0')) {
                printf(1,  "\x1B[34m%s\x1B[0m", keywords[i]);  // نمایش کلمه کلیدی با رنگ آبی
                input += len;
                found = 1;
                break;
            }
        }

        // چاپ عادی در صورتی که کلمه کلیدی نبود
        if (!found) {
            printf(1, "%c", *input);
            input++;
        }
    }
    printf(1, "\n");
}

int main() {
    char buf[MAX_INPUT];

    printf(1, "Enter text: ");
    gets(buf, sizeof(buf));

    highlight(buf);

    exit();
}