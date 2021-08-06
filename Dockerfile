FROM adoptopenjdk/openjdk11:alpine-slim
COPY ./c-sdk .

RUN apk update 
RUN apk add bash

RUN apk add git
RUN apk add --update --no-cache build-base gcc g++ pcre pcre-dev
RUN apk add mpc1-dev

COPY --from=golang:1.16.7-alpine3.14 /usr/local/go/ /usr/local/go/
ENV PATH="/usr/local/go/bin:${PATH}"
RUN go env -w GO111MODULE=auto
RUN make

RUN gcc -fPIC -Wall -shared -o newrelicjni.so newrelicjni.c -I $JAVA_HOME/include -I $JAVA_HOME/include/linux -I ./include -L. -lnewrelic -lpcre -lpthread -lm -rdynamic

CMD cp newrelicjni.so /shared