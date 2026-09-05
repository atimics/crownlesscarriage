#include "client/cc_voice_net.h"
#include "test_support.h"
#include <stdlib.h>
#include <time.h>

int main(int argc, char **argv)
{
    CC_CHECK(argc == 2);
    CcSpeech speech;
    CC_CHECK(CcSpeechCompose(&speech, "test", 1, "Mara Venn", 0,
        "Eight boxes.", CC_SPEECH_PLAIN, CC_SPEECH_CONVERSATION, 0));
    CC_CHECK(CcVoiceNetStart(&speech));
    CC_CHECK(!CcVoiceNetStart(&speech));
    bool cancel = argv[1][0] == 'c';
    if (cancel) CcVoiceNetCancel();
    unsigned char *bytes = NULL;
    size_t size = 0;
    int result = 0;
    for (int i = 0; i < 1000 && result == 0; ++i) {
        result = CcVoiceNetPoll(&bytes, &size);
        struct timespec pause = {.tv_sec = 0, .tv_nsec = 10000000};
        (void)nanosleep(&pause, NULL);
    }
    CC_CHECK(result == (argv[1][0] == 's' ? 1 : -1));
    if (result > 0) CC_CHECK(size >= 44 && bytes != NULL);
    free(bytes);
    CC_CHECK(!CcVoiceNetBusy());
    CcVoiceNetShutdown();
    return 0;
}
