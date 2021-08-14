package com.dlocal;

public interface NewRelicNative {

    void init(String app, String accountId, String port);

    String startWebTransaction(String id, String transactionName, String segmentName,
        String segmentCustom);

    void endWebTransaction(String id);

    void destroy();
}
