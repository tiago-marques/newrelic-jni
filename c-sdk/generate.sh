gcc -fPIC -Wall -shared -o newrelicjni.so newrelicjni.c -I $JAVA_HOME/include -I $JAVA_HOME/include/linux -I ./include -L. -lnewrelic -lpcre -lpthread -lm -rdynamic
cp newrelicjni.so shared