package com.dlocal;

public class NewRelicJNI {

    static {
        System.load("/newrelicjni.so");
    }

    public native void init(String app, String accountId, String port);

    public native String startWebTransaction(String id, String transactionName, String segmentName,
        String segmentCustom);

    public native void endWebTransaction(String id);

    public native void destroy();
}
