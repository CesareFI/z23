// Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
package org.zclassic.wallet.scanipc;
oneway interface IScanReply {
    void onReady();
    void onResult(long requestId, in byte[] text);
}
