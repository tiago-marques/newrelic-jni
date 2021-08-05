FROM adoptopenjdk/openjdk11:alpine-slim

COPY ./c-sdk/* .

RUN apk add --update --no-cache build-base gcc g++ pcre pcre-dev
RUN apk add mpc1-dev

RUN wget https://dl.google.com/go/go1.10.3.linux-amd64.tar.gz
RUN tar -C /usr/local -xzf go1.10.3.linux-amd64.tar.gz
RUN export PATH=$PATH:/usr/local/go/bi

CMD make

ENTRYPOINT [ "sh", "generate.sh"]
