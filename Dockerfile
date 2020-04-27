FROM ubuntu:19.10
ARG DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get upgrade -y && apt-get install libboost-system-dev libasio-dev libboost-program-options-dev libboost-log-dev cmake g++ make git awscli -y
ADD . paracooba
RUN cd paracooba && mkdir build && cd build && cmake .. -DENABLE_TESTS=OFF && make
EXPOSE 18001/tcp
EXPOSE 18080/tcp
ENTRYPOINT ["paracooba/aws-run.sh"]
CMD []
