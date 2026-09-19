#include "story/cc_core_model.h"
#include "test_support.h"

int main(int argc, char **argv)
{
    CC_CHECK(argc == 2);
    CcCoreModel *model = CcCoreModelLoad(argv[1]);
    CC_CHECK(model != NULL);
    const int ids[] = {128, 129, 160, 176, 178, 306, 512, 512, 512, 512, 131};
    int output[352];
    CC_CHECK(CcCoreModelBeginSemantic(model, ids, 11));
    CC_CHECK(CcCoreModelPrefixTokens(model, output, 352) == 11);
    for (int i = 0; i < 11; ++i) CC_CHECK(output[i] == ids[i]);
    CC_CHECK(CcCoreModelSemanticTokens(model, output, 352) == 0);
    CC_CHECK(!CcCoreModelBeginSemantic(model, NULL, 11));
    CC_CHECK(!CcCoreModelBeginSemantic(model, ids, 353));
    const int invalid[] = {128, 0, 131};
    CC_CHECK(!CcCoreModelBeginSemantic(model, invalid, 3));
    CC_CHECK(CcCoreModelBeginSemantic(model, ids, 11));
    CC_CHECK(CcCoreModelBeginParticipant(model, "crownless-person-v2\nnext:"));
    CC_CHECK(CcCoreModelSemanticTokens(model, output, 352) == -1);
    const int policy_ids[] = {1280, 129, 160, 176, 178, 306, 512, 512, 1580, 1024, 1087, 1281};
    CC_CHECK(CcCoreModelBeginPolicy(model, policy_ids, 12));
    CC_CHECK(CcCoreModelPrefixTokens(model, output, 352) == 12);
    for (int i = 0; i < 12; ++i) CC_CHECK(output[i] == policy_ids[i]);
    CC_CHECK(!CcCoreModelBeginPolicy(model, NULL, 12));
    const int bad_frame[] = {128, 129, 1580, 1024, 131};
    CC_CHECK(!CcCoreModelBeginPolicy(model, bad_frame, 5));
    const int bad_id[] = {1280, 1580, 1024, 1024, 1281};
    CC_CHECK(!CcCoreModelBeginPolicy(model, bad_id, 5));
    const int no_marker[] = {1280, 129, 1024, 1281};
    CC_CHECK(!CcCoreModelBeginPolicy(model, no_marker, 4));
    const int one_choice[] = {1280, 1580, 1024, 1281};
    CC_CHECK(CcCoreModelBeginPolicy(model, one_choice, 4));
    CC_CHECK(CcCoreModelStep(model, 1024U) == 1);
    CC_CHECK(CcCoreModelSemanticTokens(model, output, 352) == 1);
    CC_CHECK(output[0] == 1024);
    CC_CHECK(CcCoreModelText(model) == NULL);
    const int changed_choice[] = {1280, 1580, 1030, 1281};
    CC_CHECK(CcCoreModelBeginPolicy(model, changed_choice, 4));
    CC_CHECK(CcCoreModelStep(model, 1024U) == 1);
    CC_CHECK(CcCoreModelSemanticTokens(model, output, 352) == 1);
    CC_CHECK(output[0] == 1030);
    CcCoreModelFree(model);
    return 0;
}
