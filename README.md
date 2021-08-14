[![New Relic Experimental](https://opensource.newrelic.com/static/Experimental-2a7855f9eed97a968fab9f8aa29b1afe.png)](https://opensource.newrelic.com/oss-category/#community-project)

# New Relic JNI

This is the New Relic JNI! If your application does not use other New Relic 
APM agent languages, you can use the JNI to take advantage of New Relic's
monitoring capabilities and features to instrument a wide range of applications.

# Problem

Java is moving forward to native builds. That makes Java Agent not supported anymore.
If you have an Java apllication on production and can't move on to native because New Relic integrations didn't support native builds maybe together we can get one solution to avoid JVM agent and say welcome to java native world.

# Idea

Using official C-SDK from New Relic we can create a shared library .so and call native from JNI to integrate with One New Relic platform via c-daemon. Something like that.

1. With C-SDK make cmd working, we create an shared library using same arc that our system that will running our platform (like ubi mininal, for example)
2. With JNI powers we can create a Java code to load our .so (shared library) that we can use C language to use methods to metrics transactions to c-daemon.
3. c-daemon handle secure integrations to One New Relic and we have all our traffic send to New Relic platform.

More simplify:
JAVA > JNI > SO > C-DAEMON (running in docker/pod/kubernetes/daemon) > https://one.newrelic.com/ 


# Running

Any native build that support JNI. (Could be easier when GraalVM supports native JNA)

--

Use docker-compose

1. docker-compose build
2. docker-compose up

## Installation

### generate file .h from NewRelicJNI.java (root project folder from terminal)
`javac newrelic-jni/src/main/java/com/dlocal/NewRelicJNI.java -h .`

### Requirements

The New Relic JNI needs C-SDK build to generate a shared library and now this is only possible working on 64-bit Linux operating systems with:

* gcc 4.8 or higher
* glibc 2.17 or higher
* Kernel version 2.6.26 or higher
* libpcre 8.20 or higher
* libpthread

Running unit tests requires cmake 2.8 or higher.

Compiling the New Relic daemon requires Go 1.7 or higher.

## Building

If your system meets the requirements, building the C SDK and 
daemon should be as simple as:

```sh
make
```

This will create two files in this directory:

* `libnewrelic.a`: the static C SDK library, ready to link against.
* `newrelic-daemon`: the daemon binary, ready to run.

You can start the daemon in the foreground with:

```sh
./newrelic-daemon -f --logfile stdout --loglevel debug
```

Alternatively, you can use the [C SDK daemon image on
Dockerhub](https://hub.docker.com/r/newrelic/c-daemon) to run the daemon.

Then you can invoke your instrumented application.  Your application,
which makes C SDK API calls, reports data to the daemon over a socket;
in turn, the daemon reports the data to New Relic.

For more information on installation and configuration of the C SDK for
different environments, see [Install and configure](https://docs.newrelic.com/docs/agents/c-sdk/install-configure).


**Support Channels**

* [New Relic Documentation](https://docs.newrelic.com/docs/c-sdk-table-contents): Comprehensive guidance for using our platform
* [New Relic C SDK documentation on GitHub](https://newrelic.github.io/c-sdk/index.html)
* [New Relic C SDK Guide](https://github.com/newrelic/c-sdk/blob/master/GUIDE.md)
* [New Relic C SDK distributed tracing example](https://github.com/newrelic/c-dt-example)
* [New Relic Community](https://discuss.newrelic.com/c/build-on-new-relic/Open-Source-Agents-SDKs): The best place to engage in troubleshooting questions
* [New Relic Developer](https://developer.newrelic.com/): Resources for building a custom observability applications
* [New Relic University](https://learn.newrelic.com/): A range of online training for New Relic users of every level
* Use your preferred search engine to find other New Relic resources.

**A note about vulnerabilities**

As noted in our [security policy](https://github.com/newrelic/c-sdk/security/policy), New Relic is committed to the privacy and security of our customers and their data. We believe that providing coordinated disclosure by security researchers and engaging with the security community are important means to achieve our security goals.

If you believe you have found a security vulnerability in this project or any of New Relic's products or websites, we welcome and greatly appreciate you reporting it to New Relic through [HackerOne](https://hackerone.com/newrelic).

If you would like to contribute to this project, review [these guidelines](./CONTRIBUTING.md).

To [all contributors](https://github.com/newrelic/c-sdk/graphs/contributors), we thank you!  Without your contribution, this project would not be what it is today.  We also host a community project page dedicated to [New Relic C SDK](https://opensource.newrelic.com/projects/newrelic/c-sdk).

## License
The New Relic C SDK is licensed under the [Apache 2.0](http://apache.org/licenses/LICENSE-2.0.txt) License.

The C SDK also uses source code from third party libraries. Full details on
which libraries are used and the terms under which they are licensed can be
found in the 
[third party notices document](https://github.com/newrelic/c-sdk/tree/master/THIRD_PARTY_NOTICES.md).

## Thanks

[@jorgeelucas](https://github.com/jorgeelucas)

[@felipetiburcio](https://github.com/felipetiburcio)

[@fernandobelotto](https://github.com/fernandobelotto)
