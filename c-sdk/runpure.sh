gcc -o newrelicjnipure newrelicjnipure.c -I include -L. -lnewrelic -lpcre -lpthread -lm -rdynamic && \
  ./newrelicjnipure