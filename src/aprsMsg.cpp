#include "aprsMsg.h"
#include <string.h>

static AprsMsg inbox[APRS_MSG_INBOX_SIZE];
static int     inboxCount = 0;

void aprsMsgAdd(const char *from, const char *text, const char *msgid) {
    int keep = (inboxCount < APRS_MSG_INBOX_SIZE) ? inboxCount : APRS_MSG_INBOX_SIZE - 1;
    memmove(&inbox[1], &inbox[0], keep * sizeof(AprsMsg));
    if (inboxCount < APRS_MSG_INBOX_SIZE) inboxCount++;
    strlcpy(inbox[0].from,  from  ? from  : "", sizeof(inbox[0].from));
    strlcpy(inbox[0].text,  text  ? text  : "", sizeof(inbox[0].text));
    strlcpy(inbox[0].msgid, msgid ? msgid : "", sizeof(inbox[0].msgid));
    inbox[0].when = time(NULL);
}

AprsMsg aprsMsgGet(int idx) {
    if (idx < 0 || idx >= inboxCount) { AprsMsg e = {}; return e; }
    return inbox[idx];
}

int aprsMsgCount() { return inboxCount; }
