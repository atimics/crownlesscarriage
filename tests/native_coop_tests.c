#define _POSIX_C_SOURCE 200809L
#include "client/cc_coop_client.h"
#include "client/cc_company.h"
#include "client/cc_client_session.h"
#include "test_support.h"
#include <inttypes.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

void CcCoopCheckpointNow(void) {}

int main(int argc, char **argv)
{
    CC_CHECK(argc == 5);
    CcCompanyConfigure(argv[1]);
    CcCoopClientConfigure(argv[0], argv[2], argv[3]);
    CC_CHECK(CcCoopClientActive());
    char error[256] = "", path[768];
    CcSim *sim = malloc(sizeof(*sim));
    CC_CHECK(sim != NULL);
    CC_CHECK(CcCoopClientConnect(sim, error, sizeof(error)));
    CC_CHECK(CcSimValidate(sim, error, sizeof(error)));
    CC_CHECK(CcCoopClientOwner());
    CcCompany company;
    CC_CHECK(CcCompanyList(&company, error, sizeof(error)) && company.world_count == 1);
    CC_CHECK(CcCompanyLoad(&company, argv[3], error, sizeof(error)) && company.crew_count == 2);
    CC_CHECK(CcCompanyInvite(&company, false, error, sizeof(error)) && strlen(company.invitation) == 97);
    if (strcmp(argv[4], "resume") == 0) {
        CC_CHECK(CcCoopClientHasSession());
        (void)snprintf(path, sizeof(path), "%s.shared-%s.session", argv[2], argv[3]);
        CcClientSession session;
        CC_CHECK(CcClientSessionRead(path, &session, error, sizeof(error)));
        CC_CHECK(session.position_x == 6.25f && session.position_z == 2.75f);
        CC_CHECK(CcCoopClientAppearance() == 4096U);
        CC_CHECK(sim->player.cargo[CC_GOOD_BREAD] == 2);
    } else {
        CC_CHECK(!CcCoopClientHasSession());
        CC_CHECK(CcCoopClientSetAppearance(4096U, error, sizeof(error)));
        CC_CHECK(CcCoopClientAppearance() == 4096U);
        CcCommand buy = {.kind = CC_COMMAND_TRADE, .good = CC_GOOD_BREAD, .amount = 1};
        CC_CHECK(CcCoopClientApply(sim, &buy, error, sizeof(error)));
        CC_CHECK(sim->player.cargo[CC_GOOD_BREAD] == 1);
        /* Recreate a disconnect after the host saved an action but before its receipt arrived. */
        char *json = NULL;
        char endpoint[128];
        (void)snprintf(endpoint, sizeof(endpoint), "/api/worlds/%s/state", argv[3]);
        CC_CHECK(CcCompanyRequest(endpoint, NULL, &json, error, sizeof(error)));
        char body[512];
        (void)snprintf(body, sizeof(body), "{\"protocol\":1,\"sequence\":%lld,\"action_revision\":%lld,\"action\":\"trade\",\"target\":\"0\",\"good\":0,\"amount\":1,\"campaign\":true}",
            CcCompanyJsonNumber(json, "$.next_sequence"), CcCompanyJsonNumber(json, "$.action_revision"));
        free(json);
        (void)snprintf(path, sizeof(path), "%s.shared-%s.pending", argv[2], argv[3]);
        FILE *file = fopen(path, "wb");
        CC_CHECK(file != NULL && fputs(body, file) >= 0 && fclose(file) == 0);
        (void)snprintf(endpoint, sizeof(endpoint), "/api/worlds/%s/command", argv[3]);
        CC_CHECK(CcCompanyRequest(endpoint, body, &json, error, sizeof(error)));
        CC_CHECK(CcCompanyJsonNumber(json, "$.accepted") == 1);
        free(json);
        CC_CHECK(CcCoopClientApply(sim, &buy, error, sizeof(error)));
        CC_CHECK(sim->player.cargo[CC_GOOD_BREAD] == 2 && access(path, F_OK) != 0);
        CC_CHECK(CcCoopClientTogglePause(error, sizeof(error)) && CcCoopClientPaused());
        CC_CHECK(CcCoopClientTogglePause(error, sizeof(error)) && !CcCoopClientPaused());
        CcClientSession session = {.version = CC_CLIENT_SESSION_VERSION, .world_seed = sim->world_seed,
            .location_id = sim->player.location_id, .scene = CC_CLIENT_SESSION_MARKET,
            .coordinate_space = CC_CLIENT_SESSION_LEGACY_LOCAL, .position_x = 6.25f, .position_z = 2.75f,
            .athletics = {.level = {1, 1, 1}}};
        (void)snprintf(path, sizeof(path), "%s.shared-%s.session", argv[2], argv[3]);
        CC_CHECK(CcClientSessionWrite(path, &session, error, sizeof(error)));
        CcCoopClientCheckpoint(path);
        CcCrewMember crew[CC_CREW_CAPACITY];
        float pose[CC_CREW_POSE_FLOATS] = {0};
        int count = 0;
        for (int step = 0; step < 150 && count == 0; ++step) {
            (void)CcCoopClientPoll(sim, error, sizeof(error));
            count = CcCoopClientExchange(0, pose, crew);
            struct timespec pause = {.tv_nsec = 20000000};
            (void)nanosleep(&pause, NULL);
        }
        CC_CHECK(count == 1 && strcmp(crew[0].name, "Bren") == 0);
    }
    CcCoopClientShutdown();
    free(sim);
    puts("Native company state, receipts, crew, appearance and saved place passed.");
    return 0;
}
