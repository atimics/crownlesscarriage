#include "story/cc_hrakhor.h"
#include "test_support.h"
#include <string.h>

int main(void)
{
    CcCoreAccount account;
    CC_CHECK(CcCoreAccountPrepare(CC_EVENT_NOTICE_POSTED,
        "Stone posts a notice at Fire: Food.", 20, 0, &account));
    CcCoreAccount before = account;
    const char *english = "Stone took food to Fire. I do not trust the court, if the rumour is true. Food costs 7; mushrooms help.";
    char text[512], again[512];
    CC_CHECK(CcHrakhorCorrupt(&account, english, 100U, text, sizeof(text)));
    CC_CHECK(strcmp(text, "Stone rakh'ed food to Fire. Sha do not trust the drok'khor, if the rumour is true. Food costs 7; mukuk help.") == 0);
    /* The object called Food also protects the same word in lower case. */
    CC_CHECK(CcHrakhorCorrupt(&account, english, 0U, text, sizeof(text)));
    CC_CHECK(strcmp(text, english) == 0);
    CC_CHECK(CcHrakhorCorrupt(&account, english, 50U, text, sizeof(text)));
    CC_CHECK(CcHrakhorCorrupt(&account, english, 50U, again, sizeof(again)));
    CC_CHECK(strcmp(text, again) == 0);
    CC_CHECK(memcmp(&account, &before, sizeof(account)) == 0);
    CC_CHECK(!CcHrakhorCorrupt(&account, english, 101U, text, sizeof(text)));
    CC_CHECK(text[0] == '\0');
    CC_CHECK(!CcHrakhorCorrupt(&account, english, 100U, text, 7U));
    CC_CHECK(text[0] == '\0');
    CC_CHECK(!CcHrakhorCorrupt(NULL, english, 100U, text, sizeof(text)));
    CC_CHECK(!CcHrakhorCorrupt(&account, NULL, 100U, text, sizeof(text)));
    CC_CHECK(!CcHrakhorCorrupt(&account, english, 100U, NULL, 0U));
    CC_CHECK(CcCoreAccountPrepare(CC_EVENT_NOTICE_POSTED,
        "Mara posts a notice at Thornford: Relief charter.", 80, 0, &account));
    CC_CHECK(CcHrakhorCorrupt(&account,
        "I gathered food. You built shelter. Friends traded mushrooms. Firewood isn't fire. Éva's dragon, perhaps.",
        100U, text, sizeof(text)));
    CC_CHECK(strcmp(text,
        "Sha rakh'ed zhek. Thu grosh'ed nukh. Veshuk vesh'rakh'ed mukuk. Firewood isn't vrik. Éva's vrik'drok, perhaps.") == 0);
    char exact[5];
    CC_CHECK(CcHrakhorCorrupt(&account, "food", 100U, exact, sizeof(exact)));
    CC_CHECK(strcmp(exact, "zhek") == 0);
    CC_CHECK(!CcHrakhorCorrupt(&account, "food", 100U, exact, 4U));
    CC_CHECK(exact[0] == '\0');
    account.fields[0].length = (size_t)-1;
    CC_CHECK(!CcHrakhorCorrupt(&account, english, 100U, text, sizeof(text)));
    return 0;
}
