#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "src/match.h"

int main() {
    const char *windows[] = {
        "Daniel Dario Lukic (DM) - GLS-STUDIO - Slack - Google Chrome",
        "parcelconnect - TheIntegratedScanLogUpload.kt [parcel jetbrains-idea",
        "Flagsmith Android/Kotlin SDK | Flagsmith Docs - Google Chrome",
        "KOROLOVA - KISS.CLUB.MIX | KISS FM Podcast (Captive S Google-chrome"
    };
    
    printf("Testing 'ddl' with normalization:\n");
    for (int i = 0; i < 4; i++) {
        if (has_match("ddl", windows[i])) {
            score_t raw_score = match("ddl", windows[i]);
            int len = strlen(windows[i]);
            // Try absolute value normalization
            score_t normalized = -(fabs(raw_score) / (double)len * 100.0);
            printf("  Score raw=%f norm=%f (len=%d): %s\n", 
                   raw_score, normalized, len, windows[i]);
        }
    }
    
    return 0;
}