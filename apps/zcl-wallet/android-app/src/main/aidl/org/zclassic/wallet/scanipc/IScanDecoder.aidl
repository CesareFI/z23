// Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
package org.zclassic.wallet.scanipc;
import org.zclassic.wallet.scanipc.IScanReply;
oneway interface IScanDecoder {
    void ready(IScanReply reply);
    void decode(in byte[] frame, int network, long requestId, IScanReply reply);
}
