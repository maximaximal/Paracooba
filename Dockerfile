FROM ubuntu:18.04
ARG DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get upgrade -y && apt-get install libboost-system-dev libboost-asio-dev libboost-program-options-dev libboost-log-dev cmake g++ make git awscli -y
ADD . paracooba
RUN cd paracooba && mkdir build && cd build && cmake .. && make
EXPOSE 18001/tcp
ENTRYPOINT ["paracooba/aws-run.sh"]
CMD []
